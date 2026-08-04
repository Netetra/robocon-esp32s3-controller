#include <Arduino.h>
#include <vector>
#include "usbh_core.h"
#include "task/led_blink.h"
#include "task/esp_now.h"
#include "gamepad.h"

#define TAG "App"

const uint8_t LED_PIN = 21;
const uint8_t WIFI_CHANNEL = 14;
const uint8_t PEER_ID = 0x00;
const uint8_t GAMEPAD_TIMEOUT_MS = 250;

struct GamepadData gamepad_data;
Gamepad gamepad = Gamepad();
uint32_t last_receive_gamepad;

enum class MessageType: uint8_t {
    Gamepad = 0
};

void esp_now_recv_cb(struct ReceiveParams* receive_params) {
    if (memcmp(receive_params->des_addr.data(), BROADCAST_MAC.data(), ESP_NOW_ETH_ALEN) == 0) {
        return;
    } else {
        esp_now_peer_info_t peer;
        esp_err_t result = esp_now_get_peer(receive_params->src_addr.data(), &peer);
        if (result != ESP_OK) {
            ESP_LOGW(TAG, "Failed get peer. result = %d", result);
            return;
        }
        struct PeerState *peer_state = ((struct PeerState*)peer.priv);
        std::vector<uint8_t> frame(receive_params->receiveData, receive_params->receiveData + receive_params->data_len);
        std::vector<struct Message> messages;
        Frame::parse_message(frame, messages);
        for (struct Message message: messages) {
            printf("from: id = 0x%02X; Message { type = %d, data = { ", peer_state->peer_id, message.type);
            for (uint8_t byte : message.data) {
                printf("%02X, ", byte);
            }
            printf("} };\n");
        }
    }
}

void update_gamepad_data() {
    if (gamepad_poll(&gamepad_data)) {
        last_receive_gamepad = millis();
        gamepad.update(&gamepad_data);
    }
    if (millis() - last_receive_gamepad > GAMEPAD_TIMEOUT_MS) {
        gamepad_data_set_neutral(&gamepad_data);
        ESP_LOGW(TAG, "Gamepad timeout.");
    }
}

void setup() {
    cherryusb_task_init();
    esp_now_task_init(WIFI_CHANNEL, PEER_ID);
    led_blink_task_init(LED_PIN);
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
    esp_now_send_messages(0xFF, messages);
    vTaskDelay(50);
}
