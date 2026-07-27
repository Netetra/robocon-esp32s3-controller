#include "driver/general.h"
#include "led_blink.h"

extern volatile uint16_t led_blink_interval;

extern "C" void usbh_gamepad_run(struct usbh_gamepad *gamepad_class) {
    if (gamepad_class->gamepad_type == GamepadType::Xbox) {}
    led_blink_interval = static_cast<uint16_t>(LedBlinkInterval::CONNECTED);
}

extern "C" void usbh_gamepad_stop(struct usbh_gamepad *gamepad_class) {
    led_blink_interval = static_cast<uint16_t>(LedBlinkInterval::DISCONNECTED);
}