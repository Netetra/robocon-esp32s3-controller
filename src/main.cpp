#include <Arduino.h>
#include "usbh_core.h"

const uint8_t LED_PIN = 21;

enum class LedBlinkInterval {
    DISCONNECTED = 100,
    CONNECTED = 1000,
};

volatile uint16_t led_delay_ms = static_cast<uint16_t>(LedBlinkInterval::DISCONNECTED);
TaskHandle_t led_blink_task_handle;

void led_blink_task(void*) {
    bool state = false;
    while (true) {
        digitalWrite(LED_PIN, state);
        state = !state;
        vTaskDelay(led_delay_ms);
    }
}

void setup() {
    // indicator LED task initialize.
    pinMode(LED_PIN, OUTPUT);
    xTaskCreateUniversal(led_blink_task, "LED Blink", 2048, NULL, 0, &led_blink_task_handle, CONFIG_ARDUINO_RUNNING_CORE);
    // CherryUSB Host stack initialize.
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    usbh_initialize(0, ESP_USBH_BASE);
}

void loop() {
    vTaskDelay(10);
}
