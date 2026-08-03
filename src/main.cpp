#include <Arduino.h>
#include "usbh_core.h"
#include "task/led_blink.h"
#include "task/esp_now.h"
#include "gamepad.h"

volatile const uint8_t LED_PIN = 21;
volatile const uint8_t PEER_ID = 0x02;

// Gamepad gamepad = Gamepad();

void setup() {
    // cherryusb_task_init();
    esp_now_task_init(PEER_ID);
    led_blink_task_init(LED_PIN);
}

void loop() {
    // struct GamepadData *gamepad_data = gamepad_poll();
    // gamepad.update(gamepad_data);
    vTaskDelay(10);
}
