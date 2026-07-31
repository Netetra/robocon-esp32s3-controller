#include <freertos/FreeRTOS.h>
#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <esp_timer.h>
#include "./esp_now.h"
#include "../protocol.h"

#define TAG "ESP Now"

const uint32_t ESP_NOW_SEND_TASK_STACK_SIZE = 4096;
const uint8_t ESP_NOW_SEND_TASK_PRIORITY = 12;
const uint32_t ESP_NOW_RECEIVE_TASK_STACK_SIZE = 4096;
const uint8_t ESP_NOW_RECEIVE_TASK_PRIORITY = 15;
const uint32_t BEACON_WATCH_TASK_STACK_SIZE = 4096;
const uint8_t BEACON_WATCH_TASK_PRIORITY = 15;
const uint32_t PAIRING_TASK_STACK_SIZE = 4096;
const uint8_t PAIRING_TASK_PRIORITY = 15;

TaskHandle_t esp_now_send_task_handle;
TaskHandle_t esp_now_receive_task_handle;
TaskHandle_t beacon_watch_task_handle;
TaskHandle_t pairing_task_handle;

QueueHandle_t sender_queue;
QueueHandle_t receiver_queue;
QueueHandle_t pairing_queue;

SemaphoreHandle_t send_done;

void esp_now_send_wrap(const uint8_t *peer_addr, const uint8_t *data, size_t len) {
    struct SendParams send_params = { 0 };
    memcpy(send_params.peer_addr, peer_addr, ESP_NOW_ETH_ALEN);
    memcpy(send_params.data, data, len);
    send_params.len = len;
    xQueueSend(sender_queue, &send_params, portMAX_DELAY);
}

void esp_now_send_cb(const esp_now_send_info_t *tx_info, esp_now_send_status_t status) {
    if (status == ESP_NOW_SEND_FAIL) {
        ESP_LOGW(TAG, "Failed to send.");
    }
    xSemaphoreGive(send_done);
}

void pairing_task(void* args) {
    struct ReceiveParams receive_params = { 0 };
    ESP_LOGI(TAG, "Pairing task initialized.");
    while (true) {
        if (xQueueReceive(pairing_queue, &receive_params, portMAX_DELAY)) {
            esp_now_recv_info_t *esp_now_info = &receive_params.esp_now_info;
            uint8_t *receiveData = receive_params.receiveData;
            int data_len = receive_params.data_len;
            if (data_len == sizeof(BEACON_FRAME) && memcmp(BEACON_FRAME, receiveData, sizeof(BEACON_FRAME)) == 0) {
                esp_now_peer_info_t peer;
                esp_err_t result = esp_now_get_peer(esp_now_info->src_addr, &peer);
                if (result == ESP_OK) {
                    ((struct PeerState*)peer.priv)->last_receive_beacon = millis();
                    continue;
                } else if (result == ESP_ERR_ESPNOW_NOT_FOUND) {
                    esp_now_send_wrap(BROADCAST_MAC, PAIRING_REQUEST_FRAME, sizeof(PAIRING_REQUEST_FRAME));
                    ESP_LOGI(TAG, "Pairing requested. address: " MACSTR, MAC2STR(esp_now_info->src_addr));
                } else {
                    ESP_LOGE(TAG, "Unknown error. result = %d", result);
                }
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
                    continue;
                }
                memcpy(peer.peer_addr, esp_now_info->src_addr, ESP_NOW_ETH_ALEN);
                peer.ifidx = WIFI_IF_STA;
                peer.channel = 0;
                peer.encrypt = false;
                peer.priv = heap_caps_malloc(sizeof(struct PeerState), MALLOC_CAP_8BIT);
                if (peer.priv == NULL) {
                    ESP_LOGE(TAG, "Failed to allocate peer.priv");
                    continue;
                }
                memset(peer.priv, 0, sizeof(struct PeerState));
                ((struct PeerState*)peer.priv)->last_receive_beacon = millis();
                esp_now_add_peer(&peer);
                ESP_LOGI(TAG, "Added new peer. address: " MACSTR, MAC2STR(peer.peer_addr));
            }
        }
    }
}

void esp_now_recv_cb(const esp_now_recv_info_t *esp_now_info, const uint8_t *receiveData, int data_len) {
    struct ReceiveParams receive_params = { 0 };
    memcpy(&receive_params.esp_now_info, esp_now_info, sizeof(esp_now_recv_info_t));
    memcpy(receive_params.receiveData, receiveData, data_len);
    receive_params.data_len = data_len;
    xQueueSend(receiver_queue, &receive_params, portMAX_DELAY);
}

void esp_now_send_task(void* args) {
    struct SendParams send_params = { 0 };
    ESP_LOGI(TAG, "Send task initialized.");
    while (true) {
        if (xQueueReceive(sender_queue, &send_params, portMAX_DELAY)) {
            esp_err_t result = esp_now_send(
                send_params.peer_addr,
                send_params.data,
                send_params.len
            );
            if (result != ESP_OK) {
                ESP_LOGE(TAG, "Failed to send. result = %d", result);
            }
            xSemaphoreTake(send_done, portMAX_DELAY);
        }
    }
}

void esp_now_receive_task(void* args) {
    struct ReceiveParams receive_params = { 0 };
    ESP_LOGI(TAG, "Receive task initialized.");
    while (true) {
        if (xQueueReceive(receiver_queue, &receive_params, portMAX_DELAY)) {
            if (memcmp(receive_params.esp_now_info.des_addr, BROADCAST_MAC, ESP_NOW_ETH_ALEN) == 0) {
                xQueueSend(pairing_queue, &receive_params, portMAX_DELAY);
            }
        }
    }
}

void beacon_watch_task(void* args) {
    ESP_LOGI(TAG, "Beacon watch task initialized.");
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
            } else {
                ESP_LOGI(TAG, "Peer alive. address = " MACSTR, MAC2STR(peer.peer_addr));
            }
        }
        vTaskDelay(BEACON_INTERVAL * 3);
    }
}

void beacon_sender_timer_cb(void*) {
    esp_now_send_wrap(BROADCAST_MAC, BEACON_FRAME, sizeof(BEACON_FRAME));
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

    sender_queue = xQueueCreate(5, sizeof(struct SendParams));
    receiver_queue = xQueueCreate(5, sizeof(struct ReceiveParams));
    pairing_queue = xQueueCreate(5, sizeof(struct ReceiveParams));
    send_done = xSemaphoreCreateBinary();

    esp_now_register_send_cb(esp_now_send_cb);
    esp_now_register_recv_cb(esp_now_recv_cb);

    xTaskCreate(
        esp_now_send_task,
        TAG " Sender",
        ESP_NOW_SEND_TASK_STACK_SIZE,
        NULL,
        ESP_NOW_SEND_TASK_PRIORITY,
        &esp_now_send_task_handle
    );
    xTaskCreate(
        esp_now_receive_task,
        TAG " Receiver",
        ESP_NOW_RECEIVE_TASK_STACK_SIZE,
        NULL,
        ESP_NOW_RECEIVE_TASK_PRIORITY,
        &esp_now_receive_task_handle
    );
    xTaskCreate(
        pairing_task,
        TAG " Pairing",
        PAIRING_TASK_STACK_SIZE,
        NULL,
        PAIRING_TASK_PRIORITY,
        &pairing_task_handle
    );
    xTaskCreate(
        beacon_watch_task,
        TAG " Beacon watch",
        BEACON_WATCH_TASK_STACK_SIZE,
        NULL,
        BEACON_WATCH_TASK_PRIORITY,
        &beacon_watch_task_handle
    );

    const esp_timer_create_args_t timer_args = {
        .callback = &beacon_sender_timer_cb,
        .name = "Beacon sender"
    };
    esp_timer_handle_t timer;
    esp_timer_create(&timer_args, &timer);
    esp_timer_start_periodic(
        timer,
        BEACON_INTERVAL * 1000U // micro second.
    );
}
