"""Six sensor-axis positions, independent diagonal scale/offset fit.
python scripts/calibrate.py xp.csv xn.csv yp.csv yn.csv zp.csv zn.csv --output .build/CalibrationData.h
Inputs are all-channel CSVs from telemetry.py. Header generation is explicit;
no file in the firmware is overwritten automatically. Static detection is heuristic.
"""
import argparse
import csv
import hashlib
import json
import math
import statistics as st
from pathlib import Path


def load(path):
    with open(path, encoding='utf8', newline='') as stream:
        rows = list(csv.DictReader(stream))
    if len(rows) < 151:
        raise ValueError(f'{path}: at least 151 samples / 3 seconds required')
    dt = [int(b['t_us']) - int(a['t_us']) & 0xFFFFFFFF for a, b in zip(rows, rows[1:])]
    if any((not 15000 <= t <= 25000 for t in dt)) or sum(dt) < 3000000:
        raise ValueError(f'{path}: nonuniform, missing or insufficient timestamps')
    if any((int(b['seq']) != int(a['seq']) + 1 & 0xFFFFFFFF for a, b in zip(rows, rows[1:]))):
        raise ValueError(f'{path}: sequence gap')
    means = []
    for axis in ['ax', 'ay', 'az', 'gx', 'gy', 'gz']:
        v = [int(r[axis]) for r in rows]
        if max((abs(x) for x in v)) >= 32700:
            raise ValueError(f'{path}: saturation')
        limit = 16384 * 0.02 if axis[0] == 'a' else 131 * 0.3
        if st.stdev(v) > limit:
            raise ValueError(f'{path}: excessive variance in {axis}')
        means.append(st.mean(v))
    if math.sqrt(sum(((v / 131) ** 2 for v in means[3:]))) > 4.5:
        raise ValueError(f'{path}: excessive angular rate')
    return means


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('positions', nargs=6)
    p.add_argument('--output', required=True)
    p.add_argument('--g-local', type=float, default=1.0, help='local gravity in standard g')
    a = p.parse_args()
    if not 0.97 <= a.g_local <= 1.03:
        p.error('g-local outside terrestrial range')
    v = [load(f) for f in a.positions]
    offset = [(v[2 * i][i] + v[2 * i + 1][i]) / 2 for i in range(3)]
    gain = [(v[2 * i][i] - v[2 * i + 1][i]) / (2 * a.g_local) for i in range(3)]
    if any((not 8000 < x < 24000 for x in gain)) or math.sqrt(sum((x * x for x in offset))) >= 16000:
        raise ValueError('invalid sensitivity, order, polarity or offset')
    for k, row in enumerate(v):
        ideal = [0.0, 0.0, 0.0]
        ideal[k // 2] = a.g_local * (1 if k % 2 == 0 else -1)
        residual = math.sqrt(sum((((row[i] - offset[i]) / gain[i] - ideal[i]) ** 2 for i in range(3))))
        if residual > 0.04:
            raise ValueError(f'{a.positions[k]}: cross-axis/alignment residual {residual:.4f} g')
    ident = int.from_bytes(
        hashlib.sha256(b''.join((Path(f).read_bytes() for f in a.positions))).digest()[:2],
        'little'
    ) or 1
    offset_values = ', '.join(f'{x:.8f}f' for x in offset)
    gain_values = ', '.join(f'{x:.8f}f' for x in gain)
    text = (
        '#ifndef DYNAMIC_IMU_CALIBRATION_DATA_H\n'
        '#define DYNAMIC_IMU_CALIBRATION_DATA_H\n\n'
        f'constexpr unsigned int CALIBRATION_ID = {ident};\n'
        'constexpr bool CALIBRATION_MEASURED = true;\n'
        f'constexpr float ACCEL_OFFSET[3] = {{{offset_values}}};\n'
        f'constexpr float ACCEL_COUNTS_PER_G[3] = {{{gain_values}}};\n\n'
        '// Verify physical mounting independently before integrating this header.\n'
        'constexpr float MOUNT_QUAT[4] = {1, 0, 0, 0};\n\n'
        '#endif\n'
    )
    Path(a.output).write_text(text, encoding='utf8')
    Path(a.output + '.json').write_text(
        json.dumps(
            {
                'inputs': a.positions,
                'sha256': {f: hashlib.sha256(Path(f).read_bytes()).hexdigest() for f in a.positions},
                'gravity_g': a.g_local,
                'offset_counts': offset,
                'counts_per_g': gain,
                'calibration_id': ident,
                'scope': 'diagonal accelerometer calibration only; gyro stationary bias estimated on explicit C; mounting remains unverified'
            },
            indent=2
        ),
        encoding='utf8'
    )


if __name__ == '__main__':
    main()
