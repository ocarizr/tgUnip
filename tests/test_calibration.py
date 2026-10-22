"""Synthetic calibration verification; these fixtures are not recovered lab data."""
import csv
import json
import math
import subprocess
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
import calibrate
import mpu6050_specs as specs


class CalibrationTests(unittest.TestCase):
    def setUp(self):
        self.directory = ROOT / '.build/calibration-spec-fixtures'
        self.directory.mkdir(parents=True, exist_ok=True)

    def faces(self, change=None):
        # Sensitivities at ±3%, typical catalog offsets, and ±20 deg/s gyro bias.
        offsets = [819, -819, 1311]
        gains = [16384 * 1.03, 16384 * 0.97, 16384]
        paths = []
        for face in range(6):
            path = self.directory / f'{face}.csv'
            paths.append(str(path))
            with path.open('w', newline='') as stream:
                fields = ['seq', 't_us', 'config_id', 'temp_raw', 'ax', 'ay', 'az', 'gx', 'gy', 'gz']
                writer = csv.DictWriter(stream, fieldnames=fields)
                writer.writeheader()
                for sample in range(151):
                    row = dict(seq=sample, t_us=sample * 20000, config_id=1,
                               temp_raw=-5620, gx=2620, gy=-2620, gz=2620)
                    for axis, key in enumerate(['ax', 'ay', 'az']):
                        row[key] = round(offsets[axis] + (
                            gains[axis] * (1 if face % 2 == 0 else -1)
                            if axis == face // 2 else 0
                        ))
                    if change:
                        change(row, face, sample)
                    writer.writerow(row)
        return paths, offsets, gains

    def test_nominal_profile_keeps_provenance(self):
        target = self.directory / 'Nominal.h'
        subprocess.run([sys.executable, str(ROOT / 'scripts/calibrate.py'), '--nominal',
                        '--output', str(target)], check=True)
        report = json.loads(Path(str(target) + '.json').read_text())
        self.assertEqual(report['kind'], 'manufacturer_nominal')
        self.assertFalse(report['measured'])
        self.assertEqual(report['calibration_id'], 0)
        self.assertEqual(report['counts_per_g'], [16384] * 3)
        self.assertIn('CALIBRATION_MEASURED = false', target.read_text())

    def test_fit_with_factory_tolerances_and_large_gyro_bias(self):
        paths, offsets, gains = self.faces()
        report = calibrate.fit(paths, 1.0)
        self.assertTrue(report['measured'])
        self.assertEqual(report['offset_counts'], offsets)
        for actual, expected in zip(report['counts_per_g'], gains):
            self.assertAlmostEqual(actual, expected, delta=0.5)
        self.assertLess(max(report['face_residuals_g']), 1e-6)
        self.assertEqual(report['gyro_mean_span_dps'], [0, 0, 0])
        # Normalization to local gravity is part of the calibration identity.
        self.assertNotEqual(report['calibration_id'], calibrate.fit(paths, 0.9986)['calibration_id'])

    def test_invalid_windows_rejected(self):
        scenarios = {
            'wrong scale configuration': lambda r, f, i: r.update(config_id=2),
            'sequence gap': lambda r, f, i: r.update(seq=i + (i >= 75)),
            'missing timestamp': lambda r, f, i: r.update(t_us=i * 40000),
            'thermal drift': lambda r, f, i: r.update(temp_raw=i * 10),
            'vibration': lambda r, f, i: r.update(gx=2620 + (100 if i % 2 else -100)),
            'saturation': lambda r, f, i: r.update(ax=32767),
            'changing gyro bias': lambda r, f, i: r.update(gx=2620 + f * 131),
        }
        for name, change in scenarios.items():
            with self.subTest(name=name):
                paths, _, _ = self.faces(change)
                with self.assertRaises(ValueError):
                    calibrate.fit(paths, 1.0)

    def test_timestamp_and_sequence_rollover(self):
        def rollover(row, face, sample):
            row['t_us'] = (0xffff0000 + sample * 20000) & 0xffffffff
            row['seq'] = (0xffffffd0 + sample) & 0xffffffff
        paths, _, _ = self.faces(rollover)
        report = calibrate.fit(paths, 1.0)
        self.assertEqual(report['windows'][0]['duration_s'], 3.0)
        self.assertTrue(math.isfinite(report['counts_per_g'][0]))


if __name__ == '__main__':
    unittest.main()
