#include <map>
#include <esp_wifi.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_mac.h>
#include "./esp_now.h"

#define TAG "ESP Now"

const uint32_t ESP_NOW_RECV_TASK_STACK_SIZE = 4096;
const uint8_t ESP_NOW_RECV_TASK_PRIORITY = 10;
const uint32_t ESP_NOW_BEACON_SEND_TASK_STACK_SIZE = 4096;
const uint8_t ESP_NOW_BEACON_SEND_TASK_PRIORITY = 10;
const uint32_t ESP_NOW_BEACON_WATCH_TASK_STACK_SIZE = 4096;
const uint8_t ESP_NOW_BEACON_WATCH_TASK_PRIORITY = 10;

TaskHandle_t esp_now_recv_task_handle;
TaskHandle_t esp_now_beacon_send_task_handle;
TaskHandle_t esp_now_beacon_watch_task_handle;

SemaphoreHandle_t esp_now_mutex;
QueueHandle_t esp_now_recv_info_queue;
QueueHandle_t esp_now_send_cb_status_mailbox;

__attribute__((weak)) void esp_now_recv_cb(const peer_id_t peer_id, const std::vector<struct Message>& messages) {}

void print_esp_now_send_info(const struct SendInfo *info) {
    printf(
        "send address to: " MACSTR ", len: %d, payload: ",
        MAC2STR(info->peer_addr.data()),
        info->payload.size()
    );
    for (uint8_t byte: info->payload) {
        printf("%02X ", byte);
    }
    printf("\r\n");
}

void print_esp_now_recv_info(const struct ReceiveInfo *info) {
    printf(
        "recv address from: " MACSTR " to: " MACSTR ", len: %d, payload: ",
        MAC2STR(info->src_addr.data()),
        MAC2STR(info->des_addr.data()),
        info->payload.size()
    );
    for (uint8_t byte: info->payload) {
        printf("%02X ", byte);
    }
    printf("\r\n");
}

void print_esp_now_messages(const peer_id_t peer_id, const std::vector<struct Message>& messages) {
    printf("id: 0x%02X\r\n", peer_id);
    for (struct Message message: messages) {
        printf("type = %d, data = ", peer_id, message.type);
        for (uint8_t byte : message.data) {
            printf("%02X, ", byte);
        }
        printf("\r\n");
    }
}

mac_addr_t id_to_mac(const peer_id_t peer_id) {
    mac_addr_t addr = PEER_BASE_MAC;
    addr[5] = peer_id;
    return addr;
}

peer_id_t mac_to_id(const mac_addr_t& mac_addr) {
    return mac_addr[5];
}

bool is_same_domain_peer(const mac_addr_t& mac_addr) {
    return std::equal(PEER_BASE_MAC.begin(), PEER_BASE_MAC.begin() + 5, mac_addr.begin());
}

bool is_same_group_peer(const peer_id_t a, const peer_id_t b) {
    return (a & 0xF0) == (b & 0xF0);
}

void peer_info_init(esp_now_peer_info_t *peer, const uint8_t *peer_addr, const void *priv) {
    peer->channel = 0;
    peer->ifidx = WIFI_IF_STA;
    peer->encrypt = false;
    peer->priv = (void*)priv;
    memcpy(peer->peer_addr, peer_addr, sizeof(mac_addr_t));
}

esp_err_t esp_now_add_peer_wrap(const mac_addr_t& peer_addr) {
    struct PeerState *priv = new struct PeerState;
    if (priv == nullptr) { return ESP_ERR_ESPNOW_NO_MEM; }
    priv->last_receive_beacon = millis();
    esp_now_peer_info_t peer;
    peer_info_init(&peer, peer_addr.data(), priv);
    esp_err_t result = esp_now_add_peer(&peer);
    if (result != ESP_OK) {
        delete priv;
        return result;
    }
    return ESP_OK;
}

esp_err_t esp_now_del_peer_wrap(const mac_addr_t& peer_addr) {
    xSemaphoreTake(esp_now_mutex, portMAX_DELAY);
    esp_now_peer_info_t peer;
    esp_err_t result = esp_now_get_peer(peer_addr.data(), &peer);
    if (result != ESP_OK) {
        xSemaphoreGive(esp_now_mutex);
        return result;
    }
    delete (struct PeerState*)(peer.priv);
    xSemaphoreGive(esp_now_mutex);
    return esp_now_del_peer(peer.peer_addr);
}

esp_err_t esp_now_send_wrap(const mac_addr_t& peer_addr, const std::vector<uint8_t>& payload) {
    xSemaphoreTake(esp_now_mutex, portMAX_DELAY);
    esp_err_t result = esp_now_send(peer_addr.data(), payload.data(), payload.size());
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

esp_err_t esp_now_update_peer_life(const mac_addr_t& peer_addr) {
    xSemaphoreTake(esp_now_mutex, portMAX_DELAY);
    esp_now_peer_info_t peer;
    esp_err_t result = esp_now_get_peer(peer_addr.data(), &peer);
    if (result != ESP_OK) {
        xSemaphoreGive(esp_now_mutex);
        return result;
    }
    struct PeerState* state = (struct PeerState*)(peer.priv);
    state->last_receive_beacon = millis();
    xSemaphoreGive(esp_now_mutex);
    return ESP_OK;
}

// NOTE: esp_now_del_peerと同時に実行されることは無いので排他制御は不要
bool esp_now_check_peer_life(esp_now_peer_info_t *peer, uint32_t timeout) {
    struct PeerState *state = (struct PeerState*)(peer->priv);
    if (millis() - state->last_receive_beacon > timeout) {
        return false;
    }
    return true;
}

esp_err_t esp_now_send_messages(const peer_id_t peer_id, const std::vector<struct Message>& messages) {
    std::vector<uint8_t> frame = Frame::message(messages);
    esp_err_t result;
    if (peer_id == BROADCAST_ID) {
        result = esp_now_send_wrap(BROADCAST_MAC, frame);
    } else {
        result = esp_now_send_wrap(id_to_mac(peer_id), frame);
    }
    return result;
}

bool esp_now_pairing(const peer_id_t peer_id, const mac_addr_t peer_addr, struct ReceiveInfo *info) {
    if (Frame::is_beacon(info->payload)) {
        esp_err_t update_result = esp_now_update_peer_life(info->src_addr);
        if (update_result == ESP_OK) {
            // nothing to do.
        } else if (update_result == ESP_ERR_ESPNOW_NOT_FOUND) {
            std::vector<uint8_t> pairing_request = Frame::pairing_request(info->src_addr);
            esp_err_t send_result = esp_now_send_wrap(BROADCAST_MAC, pairing_request);
            if (send_result != ESP_OK) { ESP_LOGE(TAG, "%s:%d unknown error. result = %d", __FILE__, __LINE__, send_result); }
        } else {
            ESP_LOGE(TAG, "%s:%d unknown error. result = %d", __FILE__, __LINE__, update_result);
        }
        return true;
    } else if (Frame::is_pairing_request(info->payload, peer_addr)) {
        std::vector<uint8_t> pairing_accept = Frame::pairing_accept(info->src_addr);
        esp_err_t result = esp_now_send_wrap(BROADCAST_MAC, pairing_accept);
        if (result != ESP_OK) { ESP_LOGE(TAG, "%s:%d unknown error. result = %d", __FILE__, __LINE__, result); }
        return true;
    } else if (Frame::is_pairing_accept(info->payload, peer_addr)) {
        if (peer_id == mac_to_id(info->src_addr)) {
            ESP_LOGE(TAG, "Duplicate ID detected. id: 0x%02X", mac_to_id(info->src_addr));
            return true;
        }
        esp_err_t result = esp_now_add_peer_wrap(info->src_addr);
        switch (result) {
            case ESP_OK:
                ESP_LOGI(TAG, "Added new peer. id: 0x%02X", mac_to_id(info->src_addr));
                break;
            case ESP_ERR_ESPNOW_EXIST:
                ESP_LOGW(TAG, "The peer is already exists. id: 0x%02X", mac_to_id(info->src_addr));
                break;
            default:
                ESP_LOGE(TAG, "%s:%d unknown error. result = %d", __FILE__, __LINE__, result);
                break;
        }
        return true;
    }
    return false;
}

void esp_now_recv_task(void* args) {
    struct ReceiveTaskParams *params = (struct ReceiveTaskParams*)args;
    struct ReceiveInfo *info;
    ESP_LOGI(TAG, "%s start. peer_id: 0x%02X, address: " MACSTR, __func__, params->peer_id, MAC2STR(params->peer_addr.data()));
    while (true) {
        if (xQueueReceive(esp_now_recv_info_queue, &info, portMAX_DELAY)) {
            // print_esp_now_recv_info(info);
            if (!is_same_domain_peer(info->src_addr) || !is_same_group_peer(params->peer_id, mac_to_id(info->src_addr))) {
                delete info;
                continue;
            }
            if (info->des_addr == BROADCAST_MAC) {
                bool result = esp_now_pairing(params->peer_id, params->peer_addr, info);
                if (result) { continue; }
            }
            std::vector<struct Message> messages;
            bool result = Frame::parse_message(info->payload, messages);
            esp_now_recv_cb(mac_to_id(info->src_addr), messages);
            delete info;
        }
    }
}

void esp_now_beacon_send_task(void* args) {
    TickType_t wake_time = xTaskGetTickCount();
    const std::vector<uint8_t> beacon = Frame::beacon();
    ESP_LOGI(TAG, "%s start.", __func__);
    while (true) {
        esp_now_send_wrap(BROADCAST_MAC, beacon);
        vTaskDelayUntil(&wake_time, ESP_NOW_BEACON_INTERVAL_MS);
    }
}

void esp_now_beacon_watch_task(void* args) {
    TickType_t wake_time = xTaskGetTickCount();
    uint32_t timeout = ESP_NOW_BEACON_INTERVAL_MS * 2;
    esp_now_peer_info_t peer;
    ESP_LOGI(TAG, "%s start.", __func__);
    while (true) {
        bool first = true;
        while (esp_now_fetch_peer(first, &peer) == ESP_OK) {
            first = false;
            struct PeerState *state = (struct PeerState*)(peer.priv);
            // NOTE: Broadcast用のアドレスは無視されるので分岐は不要
            // if (memcmp(peer->peer_addr, BROADCAST_MAC.data(), sizeof(mac_addr_t)) == 0) { continue; }
            if (!esp_now_check_peer_life(&peer, timeout)) {
                mac_addr_t peer_addr;
                memcpy(peer_addr.data(), peer.peer_addr, sizeof(mac_addr_t));
                esp_now_del_peer_wrap(peer_addr);
                ESP_LOGI(TAG, "Deleted peer. id: 0x%02X", mac_to_id(peer_addr));
            }
    }
        vTaskDelayUntil(&wake_time, ESP_NOW_BEACON_INTERVAL_MS * 3L);
    }
}

void __esp_now_recv_cb(const esp_now_recv_info_t *esp_now_info, const uint8_t *receiveData, int data_len) {
    struct ReceiveInfo *info = new struct ReceiveInfo;
    memcpy(info->src_addr.data(), esp_now_info->src_addr, sizeof(mac_addr_t));
    memcpy(info->des_addr.data(), esp_now_info->des_addr, sizeof(mac_addr_t));
    info->payload = std::vector<uint8_t>(receiveData, receiveData + data_len);
    xQueueSend(esp_now_recv_info_queue, &info, portMAX_DELAY);
}

void __esp_now_send_cb(const esp_now_send_info_t *tx_info, esp_now_send_status_t status) {
    xQueueSend(esp_now_send_cb_status_mailbox, &status, portMAX_DELAY);
}

void esp_now_task_init(uint8_t channel, peer_id_t peer_id) {
    esp_now_mutex = xSemaphoreCreateMutex();
    esp_now_recv_info_queue = xQueueCreate(5, sizeof(struct ReceiveInfo*));
    esp_now_send_cb_status_mailbox = xQueueCreate(1, sizeof(esp_now_send_status_t));

    WiFi.mode(WIFI_STA);
    ESP_ERROR_CHECK(esp_wifi_set_mac(WIFI_IF_STA, id_to_mac(peer_id).data()));
    WiFi.setSleep(false);
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous(true));
    ESP_ERROR_CHECK(esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE));
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous(false));
    ESP_ERROR_CHECK(esp_now_init());

    esp_now_peer_info_t peer;
    peer_info_init(&peer, BROADCAST_MAC.data(), nullptr);
    ESP_ERROR_CHECK(esp_now_add_peer(&peer));

    ESP_ERROR_CHECK(esp_now_register_send_cb(__esp_now_send_cb));
    ESP_ERROR_CHECK(esp_now_register_recv_cb(__esp_now_recv_cb));

    struct ReceiveTaskParams *recv_task_params = new struct ReceiveTaskParams;
    recv_task_params->peer_id = peer_id;
    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, recv_task_params->peer_addr.data()));

    xTaskCreate(
        esp_now_recv_task,
        TAG " Receive",
        ESP_NOW_RECV_TASK_STACK_SIZE,
        recv_task_params,
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

std::vector<uint8_t> Frame::pairing_accept(const mac_addr_t& mac_addr) {
    std::vector<uint8_t> frame = Frame::header(static_cast<uint8_t>(FrameCode::PairingAccept));
    frame.insert(frame.end(), mac_addr.begin(), mac_addr.end());
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

bool Frame::is_pairing_accept(const std::vector<uint8_t>& frame, const mac_addr_t& mac_addr) {
    if (frame.size() != (SOF.size() + 1 + sizeof(mac_addr_t))) { return false; }
    if (!Frame::is_start_sof(frame)) { return false; }
    if (static_cast<uint8_t>(FrameCode::PairingAccept) != frame[SOF.size()]) { return false; }
    bool is_match_mac_addr = std::equal(
        frame.begin() + SOF.size() + 1,
        frame.begin() + SOF.size() + 1 + sizeof(mac_addr_t),
        mac_addr.begin()
    );
    if (!is_match_mac_addr) { return false; }
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
