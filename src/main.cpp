#include <Arduino.h>
#include "task/esp_now.h"
#include "task/led_blink.h"

#define TAG "App"

const uint8_t LED_PIN = 21;
const uint8_t WIFI_CHANNEL = 14;
const peer_id_t PEER_ID = 0x00;

void setup() {
    esp_now_task_init(WIFI_CHANNEL, PEER_ID);
    led_blink_task_init(LED_PIN);
}

void loop() {
    auto frame = Frame::beacon();
    esp_now_send(BROADCAST_MAC.data(), frame.data(), frame.size());
    vTaskDelay(500);
}
