#include <Arduino.h>
#include "usbh_core.h"
#include "task/led_blink.h"
#include "task/esp_now.h"
#include "gamepad.h"

const uint8_t LED_PIN = 21;

void setup() {
    // cherryusb_task_init();
    esp_now_task_init();
    led_blink_task_init(&LED_PIN);
}

void loop() {
    vTaskDelay(10);
    // struct Gamepad *gamepad = gamepad_poll();
    // if (gamepad == NULL) { return; }
    // print_gamepad_state(gamepad);
}
