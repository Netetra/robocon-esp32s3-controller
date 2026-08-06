#include <freertos/FreeRTOS.h>
#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <esp_timer.h>
#include <esp_mac.h>
#include "./esp_now.h"

#define TAG "ESP Now"

const uint32_t ESP_NOW_RECV_TASK_STACK_SIZE = 4096;
const uint8_t ESP_NOW_RECV_TASK_PRIORITY = 15;
const uint32_t ESP_NOW_BEACON_SEND_TASK_STACK_SIZE = 4096;
const uint8_t ESP_NOW_BEACON_SEND_TASK_PRIORITY = 13;
const uint32_t ESP_NOW_BEACON_WATCH_TASK_STACK_SIZE = 4096;
const uint8_t ESP_NOW_BEACON_WATCH_TASK_PRIORITY = 14;

TaskHandle_t esp_now_recv_task_handle;
TaskHandle_t esp_now_beacon_send_task_handle;
TaskHandle_t esp_now_beacon_watch_task_handle;

SemaphoreHandle_t esp_now_mutex;
QueueHandle_t esp_now_send_cb_status_mailbox;
QueueHandle_t esp_now_recv_info_queue;

__attribute__((weak)) void esp_now_recv_cb(uint8_t peer_id, struct ReceiveInfo *info) {}

esp_err_t esp_now_get_peer_wrap(uint8_t peer_id, esp_now_peer_info_t *peer) {
    xSemaphoreTake(esp_now_mutex, portMAX_DELAY);
    bool first = true;
    while (esp_now_fetch_peer(first, peer) == ESP_OK) {
        first = false;
        struct PeerState *peer_state = (struct PeerState*)(peer->priv);
        // NOTE: Broadcast用のアドレスは無視されるので分岐は不要
        // if (memcmp(peer->peer_addr, BROADCAST_MAC.data(), sizeof(mac_addr_t)) == 0) { continue; }
        if (peer_state->peer_id == peer_id) {
            xSemaphoreGive(esp_now_mutex);
            return ESP_OK;
        }
    }
    xSemaphoreGive(esp_now_mutex);
    return ESP_ERR_ESPNOW_NOT_FOUND;
}

std::vector<uint8_t> esp_now_get_all_peer_id() {
    xSemaphoreTake(esp_now_mutex, portMAX_DELAY);
    esp_now_peer_info_t peer;
    std::vector<uint8_t> peer_ids;
    bool first = true;
    while (esp_now_fetch_peer(first, &peer) == ESP_OK) {
        first = false;
        struct PeerState *peer_state = (struct PeerState*)(peer.priv);
        // NOTE: Broadcast用のアドレスは無視されるので分岐は不要
        // if (memcmp(peer->peer_addr, BROADCAST_MAC.data(), sizeof(mac_addr_t)) == 0) { continue; }
        peer_ids.push_back(peer_state->peer_id);
    }
    xSemaphoreGive(esp_now_mutex);
    return peer_ids;
}

esp_err_t esp_now_get_peer_id(const mac_addr_t& peer_addr, uint8_t *peer_id) {
    xSemaphoreTake(esp_now_mutex, portMAX_DELAY);
    esp_now_peer_info_t peer;
    esp_err_t result = esp_now_get_peer(peer_addr.data(), &peer);
    if (result != ESP_OK) {
        xSemaphoreGive(esp_now_mutex);
        return result;
    }
    struct PeerState *peer_state = (struct PeerState*)(peer.priv);
    *peer_id = peer_state->peer_id;
    xSemaphoreGive(esp_now_mutex);
    return result;
}

esp_err_t esp_now_add_peer_wrap(const mac_addr_t& peer_addr, uint8_t peer_id) {
    xSemaphoreTake(esp_now_mutex, portMAX_DELAY);
    struct PeerState *priv = new struct PeerState;
    if (priv == nullptr) {
        xSemaphoreGive(esp_now_mutex);
        return ESP_ERR_ESPNOW_NO_MEM;
    }
    priv->peer_id = peer_id;
    priv->last_receive_beacon = millis();
    esp_now_peer_info_t peer = {
        .channel = 0,
        .ifidx = WIFI_IF_STA,
        .encrypt = false,
        .priv = (void*)priv
    };
    memcpy(peer.peer_addr, peer_addr.data(), sizeof(mac_addr_t));
    esp_err_t result = esp_now_add_peer(&peer);
    xSemaphoreGive(esp_now_mutex);
    return result;
}

esp_err_t esp_now_del_peer_wrap(uint8_t peer_id) {
    xSemaphoreTake(esp_now_mutex, portMAX_DELAY);
    esp_now_peer_info_t peer;
    esp_err_t result;
    result = esp_now_get_peer_wrap(peer_id, &peer);
    if (result != ESP_OK) {
        xSemaphoreGive(esp_now_mutex);
        return result;
    }
    delete (struct PeerState*)(peer.priv);
    result = esp_now_del_peer(peer.peer_addr);
    xSemaphoreGive(esp_now_mutex);
    return result;
}

esp_err_t __esp_now_update_peer_life(uint8_t peer_id) {
    xSemaphoreTake(esp_now_mutex, portMAX_DELAY);
    esp_now_peer_info_t peer;
    esp_err_t result = esp_now_get_peer_wrap(peer_id, &peer);
    if (result != ESP_OK) {
        xSemaphoreGive(esp_now_mutex);
        return result;
    }
    struct PeerState *peer_state = (struct PeerState*)(peer.priv);
    peer_state->last_receive_beacon = millis();
    xSemaphoreGive(esp_now_mutex);
    return ESP_OK;
}

bool __esp_now_check_peer_life(uint8_t peer_id, uint32_t timeout) {
    xSemaphoreTake(esp_now_mutex, portMAX_DELAY);
    esp_now_peer_info_t peer;
    esp_err_t result;
    result = esp_now_get_peer_wrap(peer_id, &peer);
    struct PeerState *peer_state = (struct PeerState*)(peer.priv);
    if (millis() - peer_state->last_receive_beacon > timeout) {
        xSemaphoreGive(esp_now_mutex);
        return false;
    }
    xSemaphoreGive(esp_now_mutex);
    return true;
}

esp_err_t esp_now_send_wrap(uint8_t peer_id, const std::vector<uint8_t>& frame) {
    xSemaphoreTake(esp_now_mutex, portMAX_DELAY);
    esp_now_peer_info_t peer;
    esp_err_t result;
    result = esp_now_get_peer_wrap(peer_id, &peer);
    if (result != ESP_OK) {
        xSemaphoreGive(esp_now_mutex);
        return result;
    }
    result = esp_now_send(peer.peer_addr, frame.data(), frame.size());
    if (result != ESP_OK) {
        xSemaphoreGive(esp_now_mutex);
        return result;
    }
    esp_now_send_status_t status;
    xQueueReceive(esp_now_send_cb_status_mailbox, &status, portMAX_DELAY);
    if (status != ESP_NOW_SEND_SUCCESS) {
        xSemaphoreGive(esp_now_mutex);
        return ESP_ERR_ESPNOW_INTERNAL;
    }
    xSemaphoreGive(esp_now_mutex);
    return ESP_OK;
}

esp_err_t __esp_now_send_broadcast(const std::vector<uint8_t>& frame) {
    xSemaphoreTake(esp_now_mutex, portMAX_DELAY);
    esp_err_t result = esp_now_send(BROADCAST_MAC.data(), frame.data(), frame.size());
    if (result != ESP_OK) {
        xSemaphoreGive(esp_now_mutex);
        return result;
    }
    esp_now_send_status_t status;
    xQueueReceive(esp_now_send_cb_status_mailbox, &status, portMAX_DELAY);
    if (status != ESP_NOW_SEND_SUCCESS) {
        xSemaphoreGive(esp_now_mutex);
        return ESP_ERR_ESPNOW_INTERNAL;
    }
    xSemaphoreGive(esp_now_mutex);
    return ESP_OK;
}

void __esp_now_recv_cb(const esp_now_recv_info_t *esp_now_info, const uint8_t *receiveData, int data_len) {
    ESP_LOGD(TAG, "__esp_now_recv_cb call");
    struct ReceiveInfo *info = new struct ReceiveInfo;
    memcpy(info->src_mac_addr.data(), esp_now_info->src_addr, sizeof(mac_addr_t));
    memcpy(info->des_mac_addr.data(), esp_now_info->des_addr, sizeof(mac_addr_t));
    info->payload = std::vector<uint8_t>(receiveData, receiveData + data_len);
    xQueueSend(esp_now_recv_info_queue, &info, portMAX_DELAY);
}

void __esp_now_send_cb(const esp_now_send_info_t *tx_info, esp_now_send_status_t status) {
    ESP_LOGD(TAG, "__esp_now_send_cb call");
    xQueueSend(esp_now_send_cb_status_mailbox, &status, portMAX_DELAY);
}

void esp_now_pairing(const uint8_t peer_id, const mac_addr_t& mac_addr, const struct ReceiveInfo *info) {
    uint8_t remote_peer_id;
    if (Frame::is_beacon(info->payload)) {
        esp_err_t result = esp_now_get_peer_id(info->src_mac_addr, &remote_peer_id);
        if (result == ESP_OK) {
            __esp_now_update_peer_life(remote_peer_id);
        } else if (result == ESP_ERR_ESPNOW_NOT_FOUND) {
            std::vector<uint8_t> pairing_request = Frame::pairing_request(info->src_mac_addr);
            __esp_now_send_broadcast(pairing_request);
        }
    } else if (Frame::is_pairing_request(info->payload, mac_addr)) {
        std::vector<uint8_t> pairing_accept = Frame::pairing_accept(info->src_mac_addr, peer_id);
        __esp_now_send_broadcast(pairing_accept);
    } else if (Frame::is_pairing_accept(info->payload, mac_addr, &remote_peer_id)) {
        if (peer_id == remote_peer_id) {
            ESP_LOGE(
                TAG,
                "Duplicate ID detected. id: 0x%02X, address: " MACSTR,
                peer_id,
                MAC2STR(info->src_mac_addr.data())
            );
            return;
        }
        esp_now_add_peer_wrap(info->src_mac_addr, remote_peer_id);
        ESP_LOGI(
            TAG,
            "Added new peer. id: 0x%02X, address: " MACSTR,
            remote_peer_id,
            MAC2STR(info->src_mac_addr.data())
        );
    }
}

void esp_now_recv_task(void* args) {
    struct ReceiveParams *params = (struct ReceiveParams*)args;
    struct ReceiveInfo *info;
    while (true) {
        if (xQueueReceive(esp_now_recv_info_queue, &info, portMAX_DELAY)) {
            if (info->des_mac_addr == BROADCAST_MAC) {
                esp_now_pairing(params->peer_id, params->mac_addr, info);
            } else {
                uint8_t peer_id;
                esp_err_t result = esp_now_get_peer_id(info->src_mac_addr, &peer_id);
                if (result == ESP_OK) {
                    esp_now_recv_cb(peer_id, info);
                }
            }
        }
    }
}

void esp_now_beacon_send_task(void* args) {
    TickType_t wake_time = xTaskGetTickCount();
    const std::vector<uint8_t> beacon = Frame::beacon();
    while (true) {
        __esp_now_send_broadcast(beacon);
        vTaskDelayUntil(&wake_time, ESP_NOW_BEACON_INTERVAL_MS);
    }
}

void esp_now_beacon_watch_task(void* args) {
    TickType_t wake_time = xTaskGetTickCount();
    uint32_t timeout = ESP_NOW_BEACON_INTERVAL_MS * 2;
    while (true) {
        for (uint8_t peer_id: esp_now_get_all_peer_id()) {
            if(__esp_now_check_peer_life(peer_id, timeout)) {
                esp_now_del_peer_wrap(peer_id);
                ESP_LOGI(TAG, "Deleted peer. id: 0x%02X", peer_id);
            } else {
                ESP_LOGD(TAG, "Peer alive. id: 0x%02X", peer_id);
            }
        }
        vTaskDelayUntil(&wake_time, ESP_NOW_BEACON_INTERVAL_MS * 3L);
    }
}

void esp_now_task_init(uint8_t channel, uint8_t peer_id) {
    esp_now_mutex = xSemaphoreCreateMutex();
    esp_now_send_cb_status_mailbox = xQueueCreate(1, sizeof(esp_now_send_status_t));
    esp_now_recv_info_queue = xQueueCreate(5, sizeof(struct ReceiveInfo*));

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    WiFi.setSleep(false);
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous(true));
    ESP_ERROR_CHECK(esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE));
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous(false));
    ESP_ERROR_CHECK(esp_now_init());

    esp_now_peer_info_t peer = {
        .channel = 0,
        .ifidx = WIFI_IF_STA,
        .encrypt = false,
    };
    memcpy(peer.peer_addr, BROADCAST_MAC.data(), sizeof(mac_addr_t));
    ESP_ERROR_CHECK(esp_now_add_peer(&peer));

    ESP_ERROR_CHECK(esp_now_register_send_cb(__esp_now_send_cb));
    ESP_ERROR_CHECK(esp_now_register_recv_cb(__esp_now_recv_cb));

    struct ReceiveParams *receive_params = new struct ReceiveParams;
    receive_params->peer_id = peer_id;
    esp_read_mac(receive_params->mac_addr.data(), ESP_MAC_WIFI_STA);

    xTaskCreate(
        esp_now_recv_task,
        TAG " Receive",
        ESP_NOW_RECV_TASK_STACK_SIZE,
        receive_params,
        ESP_NOW_RECV_TASK_PRIORITY,
        &esp_now_recv_task_handle
    );
    xTaskCreate(
        esp_now_beacon_send_task,
        TAG " Beacon send",
        ESP_NOW_BEACON_SEND_TASK_STACK_SIZE,
        NULL,
        ESP_NOW_BEACON_SEND_TASK_PRIORITY,
        &esp_now_beacon_send_task_handle
    );
    xTaskCreate(
        esp_now_beacon_watch_task,
        TAG " Beacon watch",
        ESP_NOW_BEACON_WATCH_TASK_STACK_SIZE,
        NULL,
        ESP_NOW_BEACON_WATCH_TASK_PRIORITY,
        &esp_now_beacon_watch_task_handle
    );
}

std::vector<uint8_t> Frame::beacon() {
    return Frame::header(static_cast<uint8_t>(FrameCode::Beacon));
}

std::vector<uint8_t> Frame::pairing_request(const mac_addr_t& mac_addr) {
    std::vector<uint8_t> frame = Frame::header(static_cast<uint8_t>(FrameCode::PairingRequest));
    frame.insert(frame.end(), mac_addr.begin(), mac_addr.end());
    return frame;
}

std::vector<uint8_t> Frame::pairing_accept(const mac_addr_t& mac_addr, uint8_t peer_id) {
    std::vector<uint8_t> frame = Frame::header(static_cast<uint8_t>(FrameCode::PairingAccept));
    frame.insert(frame.end(), mac_addr.begin(), mac_addr.end());
    frame.push_back(peer_id);
    return frame;
}

std::vector<uint8_t> Frame::message(const std::vector<struct Message>& messages) {
    std::vector<uint8_t> frame = Frame::header(static_cast<uint8_t>(FrameCode::Message));
    for (struct Message message: messages) {
        frame.push_back(message.type);
        frame.push_back(message.data.size());
        frame.insert(frame.end(), message.data.begin(), message.data.end());
    }
    return frame;
}

std::vector<uint8_t> Frame::custom(uint8_t frame_type, const std::vector<uint8_t>& data) {
    std::vector<uint8_t> frame = Frame::header(static_cast<uint8_t>(FrameCode::PairingAccept));
    frame.insert(frame.end(), data.begin(), data.end());
    return frame;
}

bool Frame::is_beacon(const std::vector<uint8_t>& frame) {
    if (frame.size() != (SOF.size() + 1)) { return false; }
    if (!Frame::is_start_sof(frame)) { return false; }
    return static_cast<uint8_t>(FrameCode::Beacon) == frame[SOF.size()];
}

bool Frame::is_pairing_request(const std::vector<uint8_t>& frame, const mac_addr_t& mac_addr) {
    if (frame.size() != (SOF.size() + 1 + sizeof(mac_addr_t))) { return false; }
    if (!Frame::is_start_sof(frame)) { return false; }
    if (static_cast<uint8_t>(FrameCode::PairingRequest) != frame[SOF.size()]) { return false; }
    bool is_match_mac_addr = std::equal(
        frame.begin() + SOF.size() + 1,
        frame.begin() + SOF.size() + 1 + sizeof(mac_addr_t),
        mac_addr.begin()
    );
    if (!is_match_mac_addr) { return false; }
    return true;
}

bool Frame::is_pairing_accept(const std::vector<uint8_t>& frame, const mac_addr_t& mac_addr, uint8_t *peer_id) {
    if (frame.size() != (SOF.size() + 1 + sizeof(mac_addr_t) + 1)) { return false; }
    if (!Frame::is_start_sof(frame)) { return false; }
    if (static_cast<uint8_t>(FrameCode::PairingAccept) != frame[SOF.size()]) { return false; }
    bool is_match_mac_addr = std::equal(
        frame.begin() + SOF.size() + 1,
        frame.begin() + SOF.size() + 1 + sizeof(mac_addr_t),
        mac_addr.begin()
    );
    if (!is_match_mac_addr) { return false; }
    *peer_id = frame[SOF.size() + 1 + sizeof(mac_addr_t)];
    return true;
}

bool Frame::parse_message(const std::vector<uint8_t>& frame, std::vector<struct Message>& messages) {
    if (!Frame::is_start_sof(frame)) { return false; }
    if (static_cast<uint8_t>(FrameCode::Message) != frame[SOF.size()]) { return false; }
    if (frame.size() == (SOF.size() + 1)) { return true; } // messages count 0
    size_t pos = SOF.size() + 1;
    while (pos < frame.size()) {
        if (pos >= frame.size()) { return false; }
        uint8_t type = frame[pos++];
        if (pos >= frame.size()) { return false; }
        uint8_t len = frame[pos++];
        if (pos + len > frame.size()) { return false; }

        struct Message message;
        message.type = type;
        message.data.assign(
            frame.begin() + pos,
            frame.begin() + pos + len
        );
        messages.push_back(std::move(message));
        pos += len;
    }
    return true;
}

std::vector<uint8_t> Frame::header(uint8_t frame_type) {
    std::vector<uint8_t> header;
    header.insert(header.end(), SOF.begin(), SOF.end());
    header.push_back(frame_type);
    return header;
}

bool Frame::is_start_sof(const std::vector<uint8_t>& frame) {
    if (frame.size() < (SOF.size() + 1)) { return false; }
    return std::equal(SOF.begin(), SOF.end(), frame.begin());
}
