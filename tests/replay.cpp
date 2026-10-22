#define _CRT_SECURE_NO_WARNINGS
#include "../src/DynamicIMU/ImuCore.h"
#include <cstdio>
#include <cinttypes>

int main() {
    imu::Observer fusion, gyro;
    fusion.beginCalibration();
    gyro.beginCalibration();
    gyro.kp = 0;
    uint32_t t = 0;
    float ax = 0, ay = 0, az = 0, gx = 0, gy = 0, gz = 0;
    std::puts("t_us,state,ready,qw,qx,qy,qz,gw,gx,gy,gz");
    while (std::scanf("%" SCNu32 ",%f,%f,%f,%f,%f,%f", &t, &ax, &ay, &az, &gx, &gy, &gz) == 7) {
        fusion.sample(imu::Vec(ax, ay, az), imu::Vec(gx, gy, gz), t, true);
        gyro.sample(imu::Vec(ax, ay, az), imu::Vec(gx, gy, gz), t, true);
        const imu::Quat q = fusion.q, g = gyro.q;
        std::printf("%" PRIu32 ",%u,%u,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g\n",
                    t,
                    static_cast<unsigned>(fusion.state),
                    static_cast<unsigned>(fusion.ready),
                    static_cast<double>(q.w),
                    static_cast<double>(q.x),
                    static_cast<double>(q.y),
                    static_cast<double>(q.z),
                    static_cast<double>(g.w),
                    static_cast<double>(g.x),
                    static_cast<double>(g.y),
                    static_cast<double>(g.z));
    }
    return 0;
}
