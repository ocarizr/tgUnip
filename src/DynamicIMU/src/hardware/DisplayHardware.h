#ifndef DYNAMIC_IMU_HARDWARE_DISPLAY_H
#define DYNAMIC_IMU_HARDWARE_DISPLAY_H

#include <UTFT.h>
#include <URTouch.h>

namespace hardware {
// Pinagem, controladores e inicialização do conjunto LCD/toque original.
// O desenho da interface e o significado dos botões pertencem a ui/Screen.
class DisplayHardware {
  public:
    DisplayHardware();
    void begin();

    UTFT lcd;
    URTouch touch;
};
} // namespace hardware

#endif
