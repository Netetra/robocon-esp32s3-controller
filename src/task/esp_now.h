#pragma once

#include <Arduino.h>
#include <esp_now.h>
#include "../protocol.h"

struct PairingTaskParam {
    uint8_t peer_id;
};

struct PeerState {
    uint8_t peer_id;
    uint32_t last_receive_beacon;
};

struct SendParams {
    uint8_t peer_addr[ESP_NOW_ETH_ALEN];
    uint8_t data[ESP_NOW_MAX_DATA_LEN];
    size_t len;
};

struct ReceiveParams {
    mac_addr_t src_addr;
    mac_addr_t des_addr;
    uint8_t receiveData[ESP_NOW_MAX_DATA_LEN];
    int data_len;
};

void esp_now_send_wrap(const uint8_t *peer_addr, const uint8_t *data, size_t len);
void esp_now_task_init(uint8_t channel, uint8_t peer_id);
esp_err_t esp_now_get_peer(uint8_t peer_id, esp_now_peer_info_t *peer);
esp_err_t esp_now_send_messages(uint8_t peer_id, std::vector<struct Message> messages);
