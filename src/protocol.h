#pragma once

#include <Arduino.h>
#include <esp_now.h>
#include <array>

using mac_addr_t = std::array<uint8_t, ESP_NOW_ETH_ALEN>;

const uint32_t ESP_NOW_BEACON_INTERVAL_MS = 500;

const mac_addr_t BROADCAST_MAC = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

static constexpr std::array<uint8_t, 3> SOF = { 0x55, 0x5A, 0xAA };

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
        static std::vector<uint8_t> pairing_request(mac_addr_t& mac_addr);
        static std::vector<uint8_t> pairing_accept(mac_addr_t& mac_addr, uint8_t peer_id);
        static std::vector<uint8_t> message(std::vector<struct Message>& messages);
        static std::vector<uint8_t> custom(uint8_t frame_type, std::vector<uint8_t>& data);
        static bool is_beacon(std::vector<uint8_t>& frame);
        static bool is_pairing_request(std::vector<uint8_t>& frame, mac_addr_t& mac_addr);
        static bool is_pairing_accept(std::vector<uint8_t>& frame, mac_addr_t& mac_addr, uint8_t *peer_id);
        static bool parse_message(std::vector<uint8_t>& frame, std::vector<struct Message>& messages);
    private:
        static std::vector<uint8_t> header(uint8_t frame_type);
        static bool is_start_sof(std::vector<uint8_t>& frame);
};
