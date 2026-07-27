#include <Arduino.h>
#include "usbh_core.h"

extern const uint8_t LED_PIN;
extern TaskHandle_t led_blink_task_handle;
void led_blink_task(void*);

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
