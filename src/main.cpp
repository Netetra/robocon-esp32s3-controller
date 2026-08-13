#include <Arduino.h>
#include <peer_link.h>
#include "gamepad.h"

#define TAG "App"

const uint8_t LED_PIN = 21;
const uint8_t WIFI_CHANNEL = 14;
const peer_id_t PEER_ID = 0x11;
const uint8_t GAMEPAD_TIMEOUT_MS = 250;

struct GamepadData gamepad_data;
Gamepad gamepad = Gamepad();
uint32_t last_receive_gamepad;

enum class MessageType: uint8_t {
    Gamepad = 0
};

void peer_link_recv_cb(const peer_id_t peer_id, const std::vector<struct Message>& messages) {}

void update_gamepad_data() {
    if (gamepad_poll(&gamepad_data)) {
        gamepad.update(&gamepad_data);
        last_receive_gamepad = millis();
    }
    if (millis() - last_receive_gamepad > GAMEPAD_TIMEOUT_MS) {
        gamepad.set_neutral();
        last_receive_gamepad = millis();
        ESP_LOGW(TAG, "Gamepad timeout.");
    }
    
}

void setup() {
    cherryusb_task_init(LED_PIN);
    peer_link_task_init(WIFI_CHANNEL, PEER_ID);
    last_receive_gamepad = millis();
}

void loop() {
    update_gamepad_data();
    std::vector<struct Message> messages;
    struct Message gamepad_message = {
        .type = static_cast<uint8_t>(MessageType::Gamepad),
        .data = std::vector((uint8_t*)&gamepad_data, (uint8_t*)&gamepad_data + sizeof(struct GamepadData))
    };
    messages.push_back(std::move(gamepad_message));
    peer_link_send(BROADCAST_ID, messages);
    vTaskDelay(50);
}
