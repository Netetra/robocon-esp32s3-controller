#pragma once

#include <Arduino.h>
#include <esp_now.h>

using peer_id_t = uint8_t;
using mac_addr_t = std::array<uint8_t, ESP_NOW_ETH_ALEN>;

const mac_addr_t BROADCAST_MAC = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
const mac_addr_t PEER_BASE_MAC = { 0x02,  'n',  'n',  'c',  't', 0x00 }; // [5]: peer_id
const peer_id_t BROADCAST_ID = 0xFF;
const uint32_t ESP_NOW_BEACON_INTERVAL_MS = 500;
const std::array<uint8_t, 2> SOF = { 0x55, 0xAA };

struct PeerState {
    uint32_t last_receive_beacon;
};

struct SendInfo {
    mac_addr_t peer_addr;
    std::vector<uint8_t> payload;
    TaskHandle_t caller;
};

struct ReceiveInfo {
    mac_addr_t src_addr;
    mac_addr_t des_addr;
    std::vector<uint8_t> payload;
};

struct ReceiveTaskParams {
    peer_id_t peer_id;
    mac_addr_t peer_addr;
};

void esp_now_task_init(uint8_t channel, peer_id_t peer_id);

enum class FrameCode: uint8_t {
    Beacon = 0x01,
    PairingRequest = 0x02,
    PairingAccept = 0x03,
    Message = 0x04
};

struct Message {
    uint8_t type;
    std::vector<uint8_t> data;
};

class Frame {
    public:
        static std::vector<uint8_t> beacon();
        static std::vector<uint8_t> pairing_request(const mac_addr_t& mac_addr);
        static std::vector<uint8_t> pairing_accept(const mac_addr_t& mac_addr);
        static std::vector<uint8_t> message(const std::vector<struct Message>& messages);
        static bool is_beacon(const std::vector<uint8_t>& frame);
        static bool is_pairing_request(const std::vector<uint8_t>& frame, const mac_addr_t& mac_addr);
        static bool is_pairing_accept(const std::vector<uint8_t>& frame, const mac_addr_t& mac_addr);
        static bool parse_message(const std::vector<uint8_t>& frame, std::vector<struct Message>& messages);
    private:
        static std::vector<uint8_t> header(uint8_t frame_type);
        static bool is_start_sof(const std::vector<uint8_t>& frame);
};

esp_err_t esp_now_send_messages(const peer_id_t peer_id, const std::vector<struct Message>& messages);
void print_esp_now_messages(const peer_id_t peer_id, const std::vector<struct Message>& messages);