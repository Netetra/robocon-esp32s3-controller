#include <freertos/FreeRTOS.h>
#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include "./esp_now.h"
#include "../protocol.h"

#define TAG "ESP Now"

const uint32_t SEND_TASK_STACK_SIZE = 2048;
const uint8_t SEND_TASK_PRIORITY = 6;
const uint32_t WATCH_TASK_STACK_SIZE = 2048;
const uint8_t WATCH_TASK_PRIORITY = 5;
const uint32_t PAIRING_TASK_STACK_SIZE = 2048;
const uint8_t PAIRING_TASK_PRIORITY = 4;

QueueHandle_t queue_mailbox;

TaskHandle_t beacon_send_task_handle;
TaskHandle_t beacon_watch_task_handle;
TaskHandle_t pairing_task_handle;

volatile bool esp_now_send_complated = true;

void esp_now_send_wrap(const uint8_t *peer_addr, const uint8_t *data, size_t len) {
    while (!esp_now_send_complated) { vTaskDelay(1); }
    esp_now_send_complated = false;
    esp_err_t result = esp_now_send(peer_addr, data, len);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Unknown error. result = %d", result);
    }
}

void esp_now_send_cb(const esp_now_send_info_t *tx_info, esp_now_send_status_t status) {
    if (status == ESP_NOW_SEND_FAIL) {
        ESP_LOGW(TAG, "Failed to send.");
    }
    esp_now_send_complated = true;
}

void esp_now_recv_cb(const esp_now_recv_info_t *esp_now_info, const uint8_t *receiveData, int data_len) {
    struct RecvData recv_data = {
        .esp_now_info = esp_now_info,
        .receiveData = receiveData,
        .data_len = data_len
    };
    xQueueSend(queue_mailbox, &recv_data, portMAX_DELAY);
}

void beacon_send_task(void* args) {
    ESP_LOGI(TAG, "Beacon sender task initialized.");
    while (true) {
        esp_now_send_wrap(BROADCAST_MAC, BEACON_FRAME, sizeof(BEACON_FRAME));
        vTaskDelay(BEACON_INTERVAL);
    }
}

void beacon_watch_task(void* args) {
    ESP_LOGI(TAG, "Beacon watcher task initialized.");
    while (true) {
        esp_now_peer_info_t peer;
        bool first = true;
        while (esp_now_fetch_peer(first, &peer) == ESP_OK) {
            first = false;
            if (memcmp(peer.peer_addr, BROADCAST_MAC, ESP_NOW_ETH_ALEN) == 0) { continue; }
            if ((millis() - ((struct PeerState*)peer.priv)->last_receive_beacon) > (BEACON_INTERVAL + BEACON_DEADLINE)) {
                heap_caps_free(peer.priv);
                uint8_t tmp[ESP_NOW_ETH_ALEN];
                memcpy(tmp, peer.peer_addr, ESP_NOW_ETH_ALEN);
                esp_err_t result = esp_now_del_peer(peer.peer_addr);
                if (result == ESP_OK) {
                    ESP_LOGI(TAG, "Deleted peer. address: " MACSTR, MAC2STR(tmp));
                } else {
                    ESP_LOGE(TAG, "Unknown error. result = %d", result);
                }
            }
        }
        vTaskDelay(BEACON_INTERVAL * 3);
    }
}

void pairing_task(void* args) {
    struct RecvData recv_data;
    ESP_LOGI(TAG, "Pairing task initialized.");
    while (true) {
        // broadcast
        struct RecvData recv_data;
        xQueueReceive(queue_mailbox, &recv_data, portMAX_DELAY);
        const esp_now_recv_info_t *esp_now_info = recv_data.esp_now_info;
        const uint8_t *receiveData = recv_data.receiveData;
        int data_len = recv_data.data_len;
        if (memcmp(esp_now_info->des_addr, BROADCAST_MAC, ESP_NOW_ETH_ALEN) == 0) {
            if (data_len == sizeof(BEACON_FRAME) && memcmp(BEACON_FRAME, receiveData, sizeof(BEACON_FRAME)) == 0) {
                esp_now_peer_info_t peer;
                esp_err_t result = esp_now_get_peer(esp_now_info->src_addr, &peer);
                if (result == ESP_OK) {
                    ((struct PeerState*)peer.priv)->last_receive_beacon = millis();
                    ESP_LOGI(TAG, "Peer alive. address = " MACSTR, MAC2STR(peer.peer_addr));
                    return;
                } else if (result == ESP_ERR_ESPNOW_NOT_FOUND) {
                    esp_now_send_wrap(BROADCAST_MAC, PAIRING_REQUEST_FRAME, sizeof(PAIRING_REQUEST_FRAME));
                    ESP_LOGI(TAG, "Pairing requested. address: " MACSTR, MAC2STR(esp_now_info->src_addr));
                } else {
                    ESP_LOGE(TAG, "Unknown error. result = %d", result);
                }
                return;
            } else if (
                data_len == sizeof(PAIRING_REQUEST_FRAME) && 
                memcmp(PAIRING_REQUEST_FRAME, receiveData, sizeof(PAIRING_REQUEST_FRAME)) == 0
            ) {
                esp_now_send_wrap(BROADCAST_MAC, PAIRING_ACCEPT_FRAME, sizeof(PAIRING_ACCEPT_FRAME));
            } else if (
                data_len == sizeof(PAIRING_ACCEPT_FRAME) && 
                memcmp(PAIRING_ACCEPT_FRAME, receiveData, sizeof(PAIRING_ACCEPT_FRAME)) == 0
            ) {
                esp_now_peer_info_t peer;
                esp_err_t result = esp_now_get_peer(esp_now_info->src_addr, &peer);
                if (result == ESP_OK) { return; }
                if (result != ESP_ERR_ESPNOW_NOT_FOUND) {
                    ESP_LOGE(TAG, "Unknown error. result = %d", result);
                    return;
                }
                memcpy(peer.peer_addr, esp_now_info->src_addr, ESP_NOW_ETH_ALEN);
                peer.ifidx = WIFI_IF_STA;
                peer.channel = 0;
                peer.encrypt = false;
                peer.priv = heap_caps_malloc(sizeof(struct PeerState), MALLOC_CAP_8BIT);
                if (peer.priv == NULL) {
                    ESP_LOGE(TAG, "Failed to allocate peer.priv");
                    return;
                }
                memset(peer.priv, 0, sizeof(struct PeerState));
                ((struct PeerState*)peer.priv)->last_receive_beacon = millis();
                esp_now_add_peer(&peer);
                ESP_LOGI(TAG, "Added new peer. address: " MACSTR, MAC2STR(peer.peer_addr));
            }
        }

        // unicast

        vTaskDelay(1);
    }
}

void esp_now_task_init() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    WiFi.setSleep(false);
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(14, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous(false);
    if(esp_now_init() != ESP_OK){
        ESP_LOGE(TAG, "Failed to initialize.");
        while (true);
    }

    esp_now_peer_info_t peer = { 0 };
    memcpy(peer.peer_addr, BROADCAST_MAC, ESP_NOW_ETH_ALEN);
    peer.channel = 0;
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;
    ESP_ERROR_CHECK(esp_now_add_peer(&peer));

    esp_now_send_complated = true;
    queue_mailbox = xQueueCreate(1, sizeof(struct RecvData));

    esp_now_register_send_cb(esp_now_send_cb);
    esp_now_register_recv_cb(esp_now_recv_cb);

    xTaskCreateUniversal(
        pairing_task,
        TAG,
        PAIRING_TASK_STACK_SIZE,
        NULL,
        PAIRING_TASK_PRIORITY,
        &pairing_task_handle,
        CONFIG_ARDUINO_RUNNING_CORE
    );
    xTaskCreateUniversal(
        beacon_send_task,
        TAG,
        SEND_TASK_STACK_SIZE,
        NULL,
        SEND_TASK_PRIORITY,
        &beacon_send_task_handle,
        CONFIG_ARDUINO_RUNNING_CORE
    );
    xTaskCreateUniversal(
        beacon_watch_task,
        TAG,
        WATCH_TASK_STACK_SIZE,
        NULL,
        WATCH_TASK_PRIORITY,
        &beacon_watch_task_handle,
        CONFIG_ARDUINO_RUNNING_CORE
    );
}