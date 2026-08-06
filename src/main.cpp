#include <Arduino.h>
#include "task/esp_now.h"

#define TAG "App"

const uint8_t WIFI_CHANNEL = 14;
const uint8_t PEER_ID = 0x01;

void setup() {
    esp_now_task_init(WIFI_CHANNEL, PEER_ID);
}

void loop() {
    vTaskDelay(10);
}
