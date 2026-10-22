"""Deterministic file/stream tests; all calibration fixtures are synthetic and ignored."""
import binascii
import csv
import importlib.util
import struct
import subprocess
import sys
import unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
from telemetry import packets, FIELDS


class ToolTests(unittest.TestCase):

    def test_resync_crc_and_signed_values(self):
        packet = bytearray(80)
        packet[:4] = b'\xa5Z\x01\x02'
        struct.pack_into(
            '<II7h7fH4I3H',
            packet,
            4,
            123,
            4294967280,
            16384,
            -32768,
            0,
            340,
            131,
            -131,
            0,
            1,
            0,
            0,
            0,
            0,
            0,
            0,
            42,
            1,
            2,
            3,
            4,
            3,
            1,
            500
        )
        struct.pack_into('<H', packet, 78, binascii.crc_hqx(packet[:78], 0xFFFF))
        bad = bytearray(packet)
        bad[15] ^= 1
        rows = list(packets(b'junk' + bad + packet + packet[:20]))
        self.assertEqual(len(rows), 1)
        self.assertEqual(rows[0]['ay'], -32768)
        self.assertEqual(rows[0]['seq'], 123)
        self.assertEqual(rows[0]['state'], 2)
        self.assertAlmostEqual(rows[0]['temp_c'], 37.53)
        self.assertEqual(rows[0]['acquisition_us'], 500)

    def test_six_position_fit_and_rejection(self):
        root = Path('.build/calibration-fixtures')
        root.mkdir(parents=True, exist_ok=True)
        paths = []
        for k in range(6):
            p = root / f'{k}.csv'
            paths.append(str(p))
            with p.open('w', newline='') as f:
                w = csv.DictWriter(f, fieldnames=FIELDS)
                w.writeheader()
                for i in range(151):
                    row = dict.fromkeys(FIELDS, 0)
                    row.update(seq=i, t_us=i * 20000, config_id=1)
                    for axis, key in enumerate(['ax', 'ay', 'az']):
                        row[key] = [
                            100,
                            -20,
                            300
                        ][axis] + (16300 * (1 if k % 2 == 0 else -1) if axis == k // 2 else 0)
                    w.writerow(row)
        target = root / 'CalibrationData.h'
        r = subprocess.run(
            [sys.executable, 'scripts/calibrate.py', *paths, '--output', str(target)],
            capture_output=True,
            text=True
        )
        self.assertEqual(r.returncode, 0, r.stderr)
        text = target.read_text().replace(' ', '')
        self.assertIn('100.00000000f,-20.00000000f,300.00000000f', text)
        self.assertIn('16300.00000000f,16300.00000000f,16300.00000000f', text)
        # Reversed position signs must fail, not silently generate negative sensitivities.
        r = subprocess.run(
            [
                sys.executable,
                'scripts/calibrate.py',
                paths[1],
                paths[0],
                *paths[2:],
                '--output',
                str(target)
            ],
            capture_output=True
        )
        self.assertNotEqual(r.returncode, 0)


if __name__ == '__main__':
    unittest.main()
