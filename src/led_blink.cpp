#include "freertos/FreeRTOS.h"
#include <Arduino.h>
#include "led_blink.h"

extern const uint8_t LED_PIN = 21;

volatile uint16_t led_blink_interval = static_cast<uint16_t>(LedBlinkInterval::DISCONNECTED);
TaskHandle_t led_blink_task_handle;

void led_blink_task(void*) {
    bool state = false;
    while (true) {
        digitalWrite(LED_PIN, state);
        state = !state;
        vTaskDelay(led_blink_interval);
    }
}
