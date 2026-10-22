#include "DisplayHardware.h"

extern uint8_t BigFont[];

namespace hardware {
DisplayHardware::DisplayHardware() : lcd(SSD1289, 38, 39, 40, 41), touch(6, 5, 4, 3, 2) {}

void DisplayHardware::begin() {
    lcd.InitLCD(LANDSCAPE);
    lcd.setFont(BigFont);
    lcd.clrScr();
    touch.InitTouch(LANDSCAPE);
    touch.setPrecision(PREC_HI);
}
} // namespace hardware
