#include "freertos/FreeRTOS.h"
#include <Arduino.h>
#include "led_blink.h"

const uint32_t STACK_SIZE = 2048;
const uint8_t PRIORITY = 0;

volatile uint16_t led_blink_interval = static_cast<uint16_t>(LedBlinkInterval::DISCONNECTED);
TaskHandle_t led_blink_task_handle;

void led_blink_task(void* args) {
    uint8_t led_pin = *(uint8_t*)args;
    bool state = false;
    while (true) {
        digitalWrite(led_pin, state);
        state = !state;
        vTaskDelay(led_blink_interval);
    }
}

void led_blink_task_init(const uint8_t* led_pin) {
    pinMode(*led_pin, OUTPUT);
    xTaskCreateUniversal(
        led_blink_task,
        "LED Blink",
        STACK_SIZE,
        (void*)led_pin,
        PRIORITY,
        &led_blink_task_handle,
        CONFIG_ARDUINO_RUNNING_CORE
    );
}