#include <Arduino.h>
#include <peer_link.h>
#include "gamepad.h"

#define TAG "App"

const uint8_t LED_PIN = 21;
const uint8_t WIFI_CHANNEL = 14;
const peer_id_t PEER_ID = 0x11;
const uint8_t GAMEPAD_TIMEOUT_MS = 250;

struct GamepadData gamepad_data;
struct PS4Data ps4_data;
struct PS4OutReport ps4_state = {
    .report_id = PS4_OUT_REPORT_ID,
    .magic_number = PS4_OUT_REPORT_MAGIC_NUMBER,
    .__reserved = 0,
    .small_rumble = 0,
    .big_rumble = 0,
    .r = 0,
    .g = 0,
    .b = 0,
    .flash_on = 0,
    .flash_off = 0
};
Gamepad gamepad = Gamepad();
uint32_t last_receive_gamepad;

enum class MessageType: uint8_t {
    Gamepad = 0,
    PS4Data = 0x12,
    PS4SetRumble = 0xA1,
    PS4SetLed = 0xA2,
};

struct __attribute__((packed)) PS4RumbleData {
    uint8_t small_rumble;
    uint8_t big_rumble;
};

struct __attribute__((packed)) PS4LedData {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t flash_on;
    uint8_t flash_off;
};


void peer_link_recv_cb(const peer_id_t peer_id, const std::vector<struct Message>& messages) {
    for (auto message: messages) {
        if (message.type == 0xA1) {
            ps4_state.small_rumble = 255;
            ps4_state.big_rumble = 255;
        } else if (message.type == 0xA2) {
            ps4_state.r = 0;
            ps4_state.g = 0;
            ps4_state.b = 255;
            ps4_state.flash_on = 16;
            ps4_state.flash_off = 8;
        }
    }
}

void update_gamepad_data() {
    enum GamepadType gamepad_type;
    uint8_t buffer[1024];
    if (gamepad_poll(&gamepad_type, buffer)) {
        switch (gamepad_type) {
            case GamepadType::PS4:
                ps4_report_parser(buffer, &gamepad_data, &ps4_data);
                gamepad.update(&gamepad_data);
                last_receive_gamepad = millis();
                break;
        }
    }
    if (millis() - last_receive_gamepad > GAMEPAD_TIMEOUT_MS) {
        gamepad.set_neutral();
        last_receive_gamepad = millis();
        ESP_LOGW(TAG, "Gamepad timeout.");
    }
    
}

void setup() {
    gamepad.set_neutral();
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
    struct Message ps4_message = {
        .type = static_cast<uint8_t>(MessageType::PS4Data),
        .data = std::vector((uint8_t*)&ps4_data, (uint8_t*)&ps4_data + sizeof(struct PS4Data))
    };
    messages.push_back(std::move(gamepad_message));
    messages.push_back(std::move(ps4_message));
    peer_link_send(BROADCAST_ID, messages);
    ps4_set_state(&ps4_state);
    vTaskDelay(50);
}
