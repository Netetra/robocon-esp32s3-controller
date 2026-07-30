#include <Arduino.h>
#include "usbh_core.h"
#include "task/led_blink.h"
#include "gamepad.h"

const uint8_t LED_PIN = 21;

void setup() {
    led_blink_task_init(&LED_PIN); // indicator LED task initialize.

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    usbh_initialize(0, ESP_USBH_BASE); // CherryUSB Host stack initialize.
}

void loop() {
    vTaskDelay(10);
    struct Gamepad *gamepad = gamepad_poll();
    if (gamepad == NULL) { return; }
    print_gamepad_state(gamepad);
}
