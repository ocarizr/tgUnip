"""Manufacturer reference, not measured calibration of an individual sensor.

InvenSense PS-MPU-6000A-00 rev. 3.4 (2013-08-19), sections 6.1–6.2.
See docs/calibracao.md for conditions, typical values and engineering policies.
"""
import math

SOURCE = {
    'document': 'PS-MPU-6000A-00',
    'revision': '3.4',
    'date': '2013-08-19',
    'sections': ['6.1', '6.2'],
    'url': 'https://invensense.tdk.com/wp-content/uploads/2015/02/MPU-6000-Datasheet.pdf',
}
ACCEL_COUNTS_PER_G = 16384.0
GYRO_COUNTS_PER_DPS = 131.0
SENSITIVITY_TOLERANCE = 0.03
ACCEL_TYPICAL_OFFSET_G = (0.05, 0.05, 0.08)
GYRO_INITIAL_BIAS_DPS = 20.0
GYRO_TEMPERATURE_VARIATION_DPS = 20.0

# Software plausibility policies, not additional manufacturer guarantees.
STARTUP_GYRO_NORM_DPS = math.sqrt(3) * (
    GYRO_INITIAL_BIAS_DPS + GYRO_TEMPERATURE_VARIATION_DPS
) + 1.0
MAX_ACCEL_STDEV_G = 0.02
MAX_GYRO_STDEV_DPS = 0.3
MAX_TEMPERATURE_SPAN_C = 1.0
MAX_FACE_RESIDUAL_G = 0.04
