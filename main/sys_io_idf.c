#include "sys_io_idf.h"
#include "config_idf.h"
#include "driver/gpio.h"

static void gpio_set(uint8_t pin, int level) {
    if (pin == 255) return;
    gpio_set_level((gpio_num_t)pin, level);
}

void setLED(uint8_t which, bool hi) {
    if (which == 255) return;
    gpio_set(which, hi ? 1 : 0);
}

void toggleSD_LED(void) {
    SysSettings.sdToggle = !SysSettings.sdToggle;
    setLED(SysSettings.LED_SD, SysSettings.sdToggle);
}

void toggleRXLED(void) {
    static int counter = 0;
    counter++;
    if (counter >= BLINK_SLOWNESS) {
        counter = 0;
        SysSettings.rxToggle = !SysSettings.rxToggle;
        setLED(SysSettings.LED_CANRX, SysSettings.rxToggle);
    }
}

void toggleTXLED(void) {
    static int counter = 0;
    counter++;
    if (counter >= BLINK_SLOWNESS) {
        counter = 0;
        SysSettings.txToggle = !SysSettings.txToggle;
        setLED(SysSettings.LED_CANTX, SysSettings.txToggle);
    }
}
