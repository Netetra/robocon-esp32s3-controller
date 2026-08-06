#pragma once

#include <Arduino.h>
#include <esp_now.h>

using mac_addr_t = std::array<uint8_t, ESP_NOW_ETH_ALEN>;

const mac_addr_t BROADCAST_MAC = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
const uint32_t ESP_NOW_BEACON_INTERVAL_MS = 500;
const std::array<uint8_t, 3> SOF = { 0x55, 0x5A, 0xAA };

struct PeerState {
    uint8_t peer_id;
    uint32_t last_receive_beacon;
};

struct ReceiveInfo {
    mac_addr_t src_mac_addr;
    mac_addr_t des_mac_addr;
    std::vector<uint8_t> payload;
};

struct ReceiveParams {
    uint8_t peer_id;
    mac_addr_t mac_addr;
};

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
        static std::vector<uint8_t> pairing_accept(const mac_addr_t& mac_addr, uint8_t peer_id);
        static std::vector<uint8_t> message(const std::vector<struct Message>& messages);
        static std::vector<uint8_t> custom(uint8_t frame_type, const std::vector<uint8_t>& data);
        static bool is_beacon(const std::vector<uint8_t>& frame);
        static bool is_pairing_request(const std::vector<uint8_t>& frame, const mac_addr_t& mac_addr);
        static bool is_pairing_accept(const std::vector<uint8_t>& frame, const mac_addr_t& mac_addr, uint8_t *peer_id);
        static bool parse_message(const std::vector<uint8_t>& frame, std::vector<struct Message>& messages);
    private:
        static std::vector<uint8_t> header(uint8_t frame_type);
        static bool is_start_sof(const std::vector<uint8_t>& frame);
};

void esp_now_task_init(uint8_t channel, uint8_t peer_id);