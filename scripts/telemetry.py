"""Decode firmware v1 binary telemetry, with CRC and byte-stream resynchronization.
Usage: python scripts/telemetry.py recording.bin output.csv
Serial collection (optional pyserial): --port COM4 --seconds 60 --raw recording.bin.
This program NEVER sends reset/calibration commands or flashes hardware.
"""
import argparse
import binascii
import csv
import struct
import time
from pathlib import Path
FIELDS = [
    'seq',
    't_us',
    'ax',
    'ay',
    'az',
    'temp_raw',
    'gx',
    'gy',
    'gz',
    'qw',
    'qx',
    'qy',
    'qz',
    'bx_rad_s',
    'by_rad_s',
    'bz_rad_s',
    'cal_id',
    'faults',
    'estimated_misses',
    'tx_drops',
    'max_render_us',
    'flags',
    'config_id',
    'acquisition_us',
    'state',
    'temp_c'
]


def packets(data):
    pos = 0
    while pos + 80 <= len(data):
        if data[pos:pos + 3] != b'\xa5\x5a\x01':
            pos += 1
            continue
        packet = data[pos:pos + 80]
        if binascii.crc_hqx(packet[:78], 0xFFFF) != struct.unpack_from('<H', packet, 78)[0]:
            pos += 1
            continue
        values = struct.unpack_from('<II7h7fH4I3H', packet, 4)
        row = dict(zip(FIELDS, values))
        row['state'] = packet[3]
        row['temp_c'] = row['temp_raw'] / 340 + 36.53
        yield row
        pos += 80


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('input', nargs='?')
    p.add_argument('output')
    p.add_argument('--port')
    p.add_argument('--seconds', type=float, default=60)
    p.add_argument('--raw', default='recording.bin')
    a = p.parse_args()
    if a.port:
        import serial
        with serial.Serial(a.port, 115200, timeout=0.2) as s, open(a.raw, 'wb') as out:
            # Opening a serial port may reset the Mega through DTR. Log warm-up separately.
            end = time.monotonic() + a.seconds
            while time.monotonic() < end:
                out.write(s.read(4096))
        a.input = a.raw
    if not a.input:
        p.error('input binary file or --port is required')
    with open(a.output, 'w', newline='', encoding='utf8') as out:
        w = csv.DictWriter(out, fieldnames=FIELDS)
        w.writeheader()
        w.writerows(packets(Path(a.input).read_bytes()))


if __name__ == '__main__':
    main()
