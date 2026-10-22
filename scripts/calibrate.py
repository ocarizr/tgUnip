"""Generate a nominal MPU-6050 profile or fit six stationary sensor-axis faces.

python scripts/calibrate.py --nominal --output .build/CalibrationData.h
python scripts/calibrate.py xp.csv xn.csv yp.csv yn.csv zp.csv zn.csv --output .build/CalibrationData.h

CSV inputs use telemetry.py columns and the firmware's 50 Hz, ±2 g, ±250 °/s
configuration. Keep the sensor physically stationary in each known face.
"""
import argparse
import csv
import hashlib
import json
import math
import statistics as st
from pathlib import Path

import mpu6050_specs as specs

CHANNELS = ('ax', 'ay', 'az', 'gx', 'gy', 'gz')


def load(path):
    with open(path, encoding='utf8', newline='') as stream:
        reader = csv.DictReader(stream)
        required = {'seq', 't_us', 'config_id', 'temp_raw', *CHANNELS}
        if not required.issubset(reader.fieldnames or []):
            raise ValueError(f'{path}: missing telemetry columns')
        rows = list(reader)
    if len(rows) < 151:
        raise ValueError(f'{path}: at least 151 samples / 3 seconds required')
    if any(int(row['config_id']) != 1 for row in rows):
        raise ValueError(f'{path}: expected config_id=1 (50 Hz, ±2 g, ±250 deg/s)')
    dt = [(int(b['t_us']) - int(a['t_us'])) & 0xffffffff
          for a, b in zip(rows, rows[1:])]
    if any(not 15000 <= t <= 25000 for t in dt) or sum(dt) < 3000000:
        raise ValueError(f'{path}: nonuniform, missing or insufficient timestamps')
    if any(int(b['seq']) != ((int(a['seq']) + 1) & 0xffffffff)
           for a, b in zip(rows, rows[1:])):
        raise ValueError(f'{path}: sequence gap')

    means, deviations = [], []
    for axis in CHANNELS:
        values = [int(row[axis]) for row in rows]
        if max(abs(value) for value in values) >= 32700:
            raise ValueError(f'{path}: saturation')
        deviation = st.stdev(values)
        limit = (specs.ACCEL_COUNTS_PER_G * specs.MAX_ACCEL_STDEV_G
                 if axis[0] == 'a'
                 else specs.GYRO_COUNTS_PER_DPS * specs.MAX_GYRO_STDEV_DPS)
        if deviation > limit:
            raise ValueError(f'{path}: excessive variance in {axis}')
        means.append(st.mean(values))
        deviations.append(deviation)
    gyro_norm = math.sqrt(sum((v / specs.GYRO_COUNTS_PER_DPS) ** 2 for v in means[3:]))
    if gyro_norm > specs.STARTUP_GYRO_NORM_DPS:
        raise ValueError(f'{path}: excessive gyro mean for startup policy')
    temperatures = [int(row['temp_raw']) / 340 + 36.53 for row in rows]
    if max(temperatures) - min(temperatures) > specs.MAX_TEMPERATURE_SPAN_C:
        raise ValueError(f'{path}: unstable sensor temperature')
    return {
        'mean_counts': means,
        'stdev_counts': deviations,
        'samples': len(rows),
        'duration_s': sum(dt) / 1e6,
        'sensor_temperature_mean_c': st.mean(temperatures),
        'sensor_temperature_span_c': max(temperatures) - min(temperatures),
    }


def fit(positions, gravity):
    windows = [load(path) for path in positions]
    values = [window['mean_counts'] for window in windows]
    offset = [(values[2 * i][i] + values[2 * i + 1][i]) / 2 for i in range(3)]
    gain = [(values[2 * i][i] - values[2 * i + 1][i]) / (2 * gravity) for i in range(3)]
    if any(not 8000 < value < 24000 for value in gain):
        raise ValueError('invalid sensitivity, order or polarity')
    if math.sqrt(sum(value * value for value in offset)) >= 16000:
        raise ValueError('invalid accelerometer offset')

    residuals = []
    for k, row in enumerate(values):
        ideal = [0.0, 0.0, 0.0]
        ideal[k // 2] = gravity * (1 if k % 2 == 0 else -1)
        residual = math.sqrt(sum(
            ((row[i] - offset[i]) / gain[i] - ideal[i]) ** 2 for i in range(3)
        ))
        if residual > specs.MAX_FACE_RESIDUAL_G:
            raise ValueError(f'{positions[k]}: cross-axis/alignment residual {residual:.4f} g')
        residuals.append(residual)

    # The six faces should share one stationary gyro bias, not six rotation rates.
    gyro_span = [
        (max(row[i] for row in values) - min(row[i] for row in values))
        / specs.GYRO_COUNTS_PER_DPS for i in range(3, 6)
    ]
    if max(gyro_span) > 0.5:
        raise ValueError('gyro means disagree between stationary faces')
    warnings = []
    if any(abs(value / specs.ACCEL_COUNTS_PER_G - 1) > specs.SENSITIVITY_TOLERANCE
           for value in gain):
        warnings.append('Sensitivity differs by more than the catalog ±3%; review setup and temperature.')
    if any(abs(offset[i] / specs.ACCEL_COUNTS_PER_G) > specs.ACCEL_TYPICAL_OFFSET_G[i]
           for i in range(3)):
        warnings.append('Offset exceeds a catalog typical value; this is not a guaranteed maximum.')
    hashes = {path: hashlib.sha256(Path(path).read_bytes()).hexdigest() for path in positions}
    identity = json.dumps({'sha256': hashes, 'gravity_g': gravity}, sort_keys=True).encode()
    ident = int.from_bytes(hashlib.sha256(identity).digest()[:2], 'little') or 1
    return {
        'kind': 'six_position_accelerometer_fit',
        'measured': True,
        'calibration_id': ident,
        'inputs': positions,
        'sha256': hashes,
        'gravity_g': gravity,
        'offset_counts': offset,
        'counts_per_g': gain,
        'windows': windows,
        'face_residuals_g': residuals,
        'gyro_mean_span_dps': gyro_span,
        'warnings': warnings,
    }


def nominal():
    return {
        'kind': 'manufacturer_nominal',
        'measured': False,
        'calibration_id': 0,
        'offset_counts': [0.0] * 3,
        'counts_per_g': [specs.ACCEL_COUNTS_PER_G] * 3,
        'warnings': ['No individual coefficients recovered; catalog tolerances are not corrections.'],
    }


def write_profile(output, report):
    offsets = ', '.join(f'{value:.8f}f' for value in report['offset_counts'])
    gains = ', '.join(f'{value:.8f}f' for value in report['counts_per_g'])
    measured = str(report['measured']).lower()
    text = (
        '#ifndef DYNAMIC_IMU_CALIBRATION_DATA_H\n'
        '#define DYNAMIC_IMU_CALIBRATION_DATA_H\n\n'
        f'// Source: {report["kind"]}; see accompanying JSON for provenance.\n'
        f'constexpr unsigned int CALIBRATION_ID = {report["calibration_id"]};\n'
        f'constexpr bool CALIBRATION_MEASURED = {measured};\n'
        f'constexpr float ACCEL_OFFSET[3] = {{{offsets}}};\n'
        f'constexpr float ACCEL_COUNTS_PER_G[3] = {{{gains}}};\n\n'
        '// Verify mounting independently; gyro bias is estimated at each stationary startup.\n'
        'constexpr float MOUNT_QUAT[4] = {1, 0, 0, 0};\n\n'
        '#endif\n'
    )
    Path(output).write_text(text, encoding='utf8')
    Path(str(output) + '.json').write_text(
        json.dumps(report, indent=2, allow_nan=False) + '\n', encoding='utf8'
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('positions', nargs='*')
    parser.add_argument('--nominal', action='store_true')
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--g-local', type=float, default=1.0,
                        help='local gravity in standard g for six-position fit')
    args = parser.parse_args()
    if not 0.97 <= args.g_local <= 1.03:
        parser.error('g-local outside terrestrial range')
    if args.nominal and args.positions:
        parser.error('--nominal cannot be combined with measured files')
    if not args.nominal and len(args.positions) != 6:
        parser.error('provide six ordered files (+X,-X,+Y,-Y,+Z,-Z) or --nominal')
    try:
        report = nominal() if args.nominal else fit(args.positions, args.g_local)
        report['manufacturer_reference'] = specs.SOURCE
        report['scope'] = (
            'Accelerometer diagonal model only. Gyro scale remains nominal; '
            'gyro bias is estimated during stationary startup. Mounting must be verified. '
            'Sensor temperature is untrimmed and does not certify laboratory temperature.'
        )
        write_profile(args.output, report)
    except (ValueError, OSError, KeyError) as error:
        parser.error(str(error))


if __name__ == '__main__':
    main()
