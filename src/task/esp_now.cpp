#include <freertos/FreeRTOS.h>
#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <esp_timer.h>
#include <esp_mac.h>
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
    uint8_t peer_id = ((struct PairingTaskParam*)args)->peer_id;
    mac_addr_t mac_addr;
    esp_read_mac(mac_addr.data(), ESP_MAC_WIFI_STA);

    ESP_LOGI(TAG, "Pairing task initialized.");
    while (true) {
        struct ReceiveParams receive_params = { 0 };
        if (xQueueReceive(pairing_queue, &receive_params, portMAX_DELAY)) {
            esp_now_recv_info_t *esp_now_info = &receive_params.esp_now_info;
            int data_len = receive_params.data_len;
            std::vector<uint8_t> frame(
                (uint8_t*)receive_params.receiveData,
                (uint8_t*)receive_params.receiveData + data_len
            );
            mac_addr_t remote_mac_addr;
            std::copy(esp_now_info->src_addr, esp_now_info->src_addr + sizeof(mac_addr_t), remote_mac_addr.begin());
            uint8_t remote_peer_id;

            if (Frame::is_beacon(frame)) {
                esp_now_peer_info_t peer;
                esp_err_t result = esp_now_get_peer(esp_now_info->src_addr, &peer);
                if (result == ESP_OK) {
                    ((struct PeerState*)peer.priv)->last_receive_beacon = millis();
                    continue;
                } else if (result == ESP_ERR_ESPNOW_NOT_FOUND) {
                    std::vector<uint8_t> pairing_request = Frame::pairing_request(remote_mac_addr);
                    esp_now_send_wrap(BROADCAST_MAC.data(), pairing_request.data(), pairing_request.size());
                    ESP_LOGI(TAG, "Pairing requested. address: " MACSTR, MAC2STR(esp_now_info->src_addr));
                } else {
                    ESP_LOGE(TAG, "Unknown error. result = %d", result);
                }
            } else if (Frame::is_pairing_request(frame, mac_addr)) {
                std::vector<uint8_t> pairing_accept = Frame::pairing_accept(remote_mac_addr, peer_id);
                esp_now_send_wrap(BROADCAST_MAC.data(), pairing_accept.data(), pairing_accept.size());
            } else if (Frame::is_pairing_accept(frame, mac_addr, &remote_peer_id)) {
                if (peer_id == remote_peer_id) {
                    ESP_LOGE(TAG, "Duplicate ID detected. id: 0x%02X, address: " MACSTR, peer_id, MAC2STR(esp_now_info->src_addr));
                    continue;
                }
                esp_now_peer_info_t peer;
                esp_err_t result = esp_now_get_peer(esp_now_info->src_addr, &peer);
                if (result == ESP_OK) { continue; }
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
                ((struct PeerState*)peer.priv)->peer_id = remote_peer_id;
                ((struct PeerState*)peer.priv)->last_receive_beacon = millis();
                esp_now_add_peer(&peer);
                ESP_LOGI(
                    TAG,
                    "Added new peer. id: 0x%02X, address: " MACSTR,
                    ((struct PeerState*)peer.priv)->peer_id,
                    MAC2STR(peer.peer_addr)
                );
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
            if (memcmp(receive_params.esp_now_info.des_addr, BROADCAST_MAC.data(), ESP_NOW_ETH_ALEN) == 0) {
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
            if (memcmp(peer.peer_addr, BROADCAST_MAC.data(), ESP_NOW_ETH_ALEN) == 0) { continue; }
            if ((millis() - ((struct PeerState*)peer.priv)->last_receive_beacon) > (ESP_NOW_BEACON_INTERVAL_MS * 2)) {
                uint8_t tmp_id = ((struct PeerState*)peer.priv)->peer_id;
                uint8_t tmp_mac_addr[ESP_NOW_ETH_ALEN];
                memcpy(tmp_mac_addr, peer.peer_addr, ESP_NOW_ETH_ALEN);
                heap_caps_free(peer.priv);
                esp_err_t result = esp_now_del_peer(peer.peer_addr);
                if (result == ESP_OK) {
                    ESP_LOGI(
                        TAG,
                        "Deleted peer. id: 0x%02X, address: " MACSTR,
                        tmp_id,
                        MAC2STR(tmp_mac_addr)
                    );
                } else {
                    ESP_LOGE(TAG, "Unknown error. result = %d", result);
                }
            } else {
                ESP_LOGD(
                    TAG,
                    "Peer alive. id: 0x%02X, address: " MACSTR,
                    ((struct PeerState*)peer.priv)->peer_id,
                    MAC2STR(peer.peer_addr)
                );
            }
        }
        vTaskDelay(ESP_NOW_BEACON_INTERVAL_MS * 3L);
    }
}

void beacon_sender_timer_cb(void*) {
    std::vector<uint8_t> beacon = Frame::beacon();
    esp_now_send_wrap(BROADCAST_MAC.data(), beacon.data(), beacon.size());
}

void esp_now_task_init(uint8_t channel, uint8_t peer_id) {
    esp_err_t result;
    WiFi.useStaticBuffers(true);
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    WiFi.setSleep(false);
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous(true));
    ESP_ERROR_CHECK(esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE));
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous(false));
    ESP_ERROR_CHECK(esp_now_init());

    esp_now_peer_info_t peer = { 0 };
    memcpy(peer.peer_addr, BROADCAST_MAC.data(), ESP_NOW_ETH_ALEN);
    peer.channel = 0;
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;
    ESP_ERROR_CHECK(esp_now_add_peer(&peer));

    sender_queue = xQueueCreate(5, sizeof(struct SendParams));
    receiver_queue = xQueueCreate(5, sizeof(struct ReceiveParams));
    pairing_queue = xQueueCreate(5, sizeof(struct ReceiveParams));
    send_done = xSemaphoreCreateBinary();

    ESP_ERROR_CHECK(esp_now_register_send_cb(esp_now_send_cb));
    ESP_ERROR_CHECK(esp_now_register_recv_cb(esp_now_recv_cb));

    void* pairing_task_param = heap_caps_malloc(sizeof(struct PairingTaskParam), MALLOC_CAP_8BIT);
    ((struct PairingTaskParam*)pairing_task_param)->peer_id = peer_id;

    xTaskCreate(
        esp_now_receive_task,
        TAG " Receiver",
        ESP_NOW_RECEIVE_TASK_STACK_SIZE,
        NULL,
        ESP_NOW_RECEIVE_TASK_PRIORITY,
        &esp_now_receive_task_handle
    );
    xTaskCreate(
        esp_now_send_task,
        TAG " Sender",
        ESP_NOW_SEND_TASK_STACK_SIZE,
        NULL,
        ESP_NOW_SEND_TASK_PRIORITY,
        &esp_now_send_task_handle
    );
    xTaskCreate(
        beacon_watch_task,
        TAG " Beacon watch",
        BEACON_WATCH_TASK_STACK_SIZE,
        NULL,
        BEACON_WATCH_TASK_PRIORITY,
        &beacon_watch_task_handle
    );
    xTaskCreate(
        pairing_task,
        TAG " Pairing",
        PAIRING_TASK_STACK_SIZE,
        pairing_task_param,
        PAIRING_TASK_PRIORITY,
        &pairing_task_handle
    );

    const esp_timer_create_args_t timer_args = {
        .callback = &beacon_sender_timer_cb,
        .name = "Beacon sender"
    };
    esp_timer_handle_t timer;
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(
        timer,
        ESP_NOW_BEACON_INTERVAL_MS * 1000L // micro second.
    ));
}
