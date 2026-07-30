#pragma once

#include "usbh_core.h"

enum class LedBlinkInterval { // milli second.
    DISCONNECTED = 100,
    CONNECTED = 1000,
};

void led_blink_task(void*);
void led_blink_task_init(const uint8_t* led_pin);
