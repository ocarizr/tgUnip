"""Independent NumPy matrix reference; deterministic synthetic experiments only.
Build tests/replay.cpp to .build/replay.exe first. No evaluation thresholds are tuned here.
"""
import argparse
import os
import csv
import io
import json
import subprocess
from pathlib import Path
import numpy as np
SEED = 20261022
ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / '.build/results'
REPLAY = ROOT / '.build' / ('replay.exe' if os.name == 'nt' else 'replay')


def run(name, rate=50, jitter=False, disturbance=False, bias=False):
    rng = np.random.default_rng(SEED)
    t = 0.0
    q = np.eye(3)
    inputs = []
    refs = []
    times = []
    accels = []
    duration = 23
    while t < duration:
        dt = 1 / rate * (rng.uniform(0.9, 1.1) if jitter else 1.0)

        # 3.3 seconds stationary for independent startup, then time-varying, noncommuting rates.
        def omega(s):
            return np.zeros(3) if s < 3.3 else np.array([
                0.7 * np.sin(0.8 * (s - 3.3)),
                0.5 * np.cos(0.6 * (s - 3.3)),
                0.3
            ])
        steps = 20
        for k in range(steps):
            v = omega(t + (k + 0.5) * dt / steps) * dt / steps
            angle = np.linalg.norm(v)
            if angle > 0:
                x, y, z = v / angle
                K = np.array([[0, -z, y], [z, 0, -x], [-y, x, 0]])
                q = q @ (np.eye(3) + np.sin(angle) * K + (1 - np.cos(angle)) * (K @ K))
        t += dt
        a = q.T @ np.array([0, 0, 1])
        g = omega(t)
        if disturbance and 9 < t < 11:
            a = a + np.array([0.8, 0, 0.5])
        # A fixed stationary offset is learned at startup; additional thermal-like bias appears in motion.
        g = g + np.array([0.005, -0.004, 0.008])
        if bias and t > 7:
            g = g + np.array([0.015, 0, 0.02])
        a = a + rng.normal(0, 0.002, 3)
        g = g + rng.normal(0, 0.0005, 3)
        times.append(t)
        refs.append(q.copy())
        accels.append(a)
        inputs.append(','.join([
            str(int(t * 1000000.0) & 0xFFFFFFFF),
            *[f'{x:.9g}' for x in a],
            *[f'{x:.9g}' for x in g]
        ]))
    result = subprocess.run(
        [str(REPLAY)],
        input='\n'.join(inputs) + '\n',
        text=True,
        capture_output=True,
        check=True
    )
    records = list(csv.DictReader(io.StringIO(result.stdout)))
    if len(records) != len(times):
        raise RuntimeError('Replay incomplete')
    truths = np.array(refs)
    up = truths[:, 2, :]
    errors = {}
    full = {}
    for prefix, label in [('q', 'fusion'), ('g', 'gyro_only')]:
        qs = np.array([[float(r[prefix + a]) for a in 'wxyz'] for r in records])
        qs /= np.linalg.norm(qs, axis=1)[:, None]
        w, x, y, z = qs.T
        est = np.array([
            [1 - 2 * (y * y + z * z), 2 * (x * y - w * z), 2 * (x * z + w * y)],
            [2 * (x * y + w * z), 1 - 2 * (x * x + z * z), 2 * (y * z - w * x)],
            [2 * (x * z - w * y), 2 * (y * z + w * x), 1 - 2 * (x * x + y * y)]
        ]).transpose(2, 0, 1)
        pred = est[:, 2, :]
        errors[label] = np.rad2deg(np.arccos(np.clip(np.sum(pred * up, axis=1), -1, 1)))
        cosine = (np.sum(truths * est, axis=(1, 2)) - 1) / 2
        full[label] = np.rad2deg(np.arccos(np.clip(cosine, -1, 1)))
    a = np.array(accels)
    a /= np.linalg.norm(a, axis=1)[:, None]
    errors['static'] = np.rad2deg(np.arccos(np.clip(np.sum(a * up, axis=1), -1, 1)))
    valid = np.array([r['ready'] == '1' for r in records])
    mask = valid & (np.array(times) > 3.3)
    if not np.all(valid[np.array(times) > 3.3]):
        raise RuntimeError('Unexpected synthetic tracking fault')
    metric = {
        label: {
            'inclination_RMSE_deg': float(np.sqrt(np.mean(e[mask] ** 2))),
            'inclination_max_deg': float(np.max(e[mask]))
        } for label,
        e in errors.items()
    }
    for label, e in full.items():
        metric[label]['full_attitude_RMSE_deg'] = float(np.sqrt(np.mean(e[mask] ** 2)))
    metric['gyro_only_samples'] = sum((int(r['state']) == 3 for r in records))
    metric['samples'] = len(records)
    out = OUTPUT / f'{name}.csv'
    with out.open('w', newline='') as f:
        w = csv.writer(f)
        w.writerow([
            't_s',
            'fusion_tilt_error_deg',
            'gyro_tilt_error_deg',
            'static_tilt_error_deg',
            'state'
        ])
        w.writerows(zip(
            times,
            errors['fusion'],
            errors['gyro_only'],
            errors['static'],
            [r['state'] for r in records]
        ))
    ymax = max(1.0, max((float(np.max(e[mask])) for e in errors.values())))
    svg = [
        '<svg xmlns="http://www.w3.org/2000/svg" width="960" height="400" viewBox="0 0 960 400">',
        '<rect width="960" height="400" fill="white"/>',
        f'<text x="70" y="25" font-size="18">{name} — simulação sintética</text>',
        '<path d="M70 50V330H920" fill="none" stroke="black"/>',
        '<text x="400" y="385">Tempo (s)</text>',
        f'<text x="5" y="45">graus</text><text x="5" y="65">{ymax:.2f}</text><text x="30" y="330">0</text>'
    ]
    for k, (label, e) in enumerate(errors.items()):
        color = ['#0060a0', '#b04000', '#555555'][k]
        points = ' '.join((
            f'{70 + (s - 3.3) / 19.7 * 850:.2f},{330 - v / ymax * 270:.2f}' for s,
            v in zip(np.array(times)[mask], e[mask])
        ))
        svg.append(f'<polyline points="{points}" fill="none" stroke="{color}" stroke-width="1"/>')
        svg.append(f'<text x="{70 + k * 260}" y="360" fill="{color}">{label}</text>')
    svg.append('</svg>')
    (OUTPUT / (name + '.svg')).write_text('\n'.join(svg), encoding='utf8')
    return metric


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--replay', type=Path, default=REPLAY)
    options = parser.parse_args()
    REPLAY = options.replay.resolve()
    OUTPUT.mkdir(parents=True, exist_ok=True)
    results = {
        'kind': 'synthetic software verification; no physical measurements',
        'seed': SEED,
        'reference': 'NumPy Rodrigues matrices; 20 midpoint substeps per measurement; independent implementation',
        'duration_s': 23,
        'cases': {}
    }
    for name, kwargs in [
        ('smooth_50hz', {}),
        ('jitter_100hz', {'rate': 100, 'jitter': True}),
        ('acceleration_50hz', {'disturbance': True}),
        ('bias_change_50hz', {'bias': True})
    ]:
        results['cases'][name] = run(name, **kwargs)
    (OUTPUT / 'resultados-simulados.json').write_text(
        json.dumps(results, indent=2) + '\n',
        encoding='utf8'
    )
    print(json.dumps(results, indent=2))
