#pragma once

#include <Arduino.h>
#include <esp_now.h>

struct PeerState {
    uint32_t last_receive_beacon;
};

struct SendParams {
    uint8_t peer_addr[ESP_NOW_ETH_ALEN];
    uint8_t data[ESP_NOW_MAX_DATA_LEN];
    size_t len;
};

struct ReceiveParams {
    esp_now_recv_info_t esp_now_info;
    uint8_t receiveData[ESP_NOW_MAX_DATA_LEN];
    int data_len;
};

void esp_now_send_wrap(const uint8_t *peer_addr, const uint8_t *data, size_t len);
void esp_now_task_init();
