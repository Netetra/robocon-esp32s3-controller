#include <Arduino.h>
#include "usbh_core.h"
#include "led_blink.h"
#include "gamepad.h"

const uint8_t LED_PIN = 21;

void setup() {
    led_blink_task_init(&LED_PIN); // indicator LED task initialize.
    gamepad_poll_task_init(); // gamepad poll task initialize.

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    usbh_initialize(0, ESP_USBH_BASE); // CherryUSB Host stack initialize.
}

void loop() {
    vTaskDelay(10);
}
