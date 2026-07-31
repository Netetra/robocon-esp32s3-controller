#pragma once

#include <Arduino.h>
#include <esp_now.h>

struct PeerState {
    uint32_t last_receive_beacon;
};

struct RecvData {
    const esp_now_recv_info_t *esp_now_info;
    const uint8_t *receiveData;
    int data_len;
};

void esp_now_send_wrap(const uint8_t *peer_addr, const uint8_t *data, size_t len);
void esp_now_task_init();
