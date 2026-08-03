#pragma once

#include <Arduino.h>
#include <esp_now.h>
#include <array>

#define TAG "protocol"

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
        static std::vector<uint8_t> beacon() {
            return Frame::header(static_cast<uint8_t>(FrameCode::Beacon));
        }
        static std::vector<uint8_t> pairing_request(mac_addr_t& mac_addr) {
            std::vector<uint8_t> frame = Frame::header(static_cast<uint8_t>(FrameCode::PairingRequest));
            frame.insert(frame.end(), mac_addr.begin(), mac_addr.end());
            return frame;
        }
        static std::vector<uint8_t> pairing_accept(mac_addr_t& mac_addr, uint8_t peer_id) {
            std::vector<uint8_t> frame = Frame::header(static_cast<uint8_t>(FrameCode::PairingAccept));
            frame.insert(frame.end(), mac_addr.begin(), mac_addr.end());
            frame.push_back(peer_id);
            return frame;
        }
        static std::vector<uint8_t> message(std::vector<struct Message>& messages) {
            std::vector<uint8_t> frame = Frame::header(static_cast<uint8_t>(FrameCode::PairingAccept));
            for (struct Message message: messages) {
                frame.push_back(message.type);
                frame.push_back(message.data.size());
                frame.insert(frame.end(), message.data.begin(), message.data.end());
            }
            return frame;
        }
        static std::vector<uint8_t> custom(uint8_t frame_type, std::vector<uint8_t>& data) {
            std::vector<uint8_t> frame = Frame::header(static_cast<uint8_t>(FrameCode::PairingAccept));
            frame.insert(frame.end(), data.begin(), data.end());
            return frame;
        }
        static bool is_beacon(std::vector<uint8_t>& frame) {
            if (frame.size() != (SOF.size() + 1)) { return false; }
            if (!Frame::is_start_sof(frame)) { return false; }
            return static_cast<uint8_t>(FrameCode::Beacon) == frame[SOF.size()];
        }
        static bool is_pairing_request(std::vector<uint8_t>& frame, mac_addr_t& mac_addr) {
            if (frame.size() != (SOF.size() + 1 + sizeof(mac_addr_t))) { return false; }
            if (!Frame::is_start_sof(frame)) { return false; }
            bool is_match_mac_addr = std::equal(
                frame.begin() + SOF.size() + 1,
                frame.begin() + SOF.size() + 1 + sizeof(mac_addr_t),
                mac_addr.begin()
            );
            if (!is_match_mac_addr) { return false; }
            return true;
        }
        static bool is_pairing_accept(std::vector<uint8_t>& frame, mac_addr_t& mac_addr, uint8_t *peer_id) {
            if (frame.size() != (SOF.size() + 1 + sizeof(mac_addr_t) + 1)) { return false; }
            if (!Frame::is_start_sof(frame)) { return false; }
            bool is_match_mac_addr = std::equal(
                frame.begin() + SOF.size() + 1,
                frame.begin() + SOF.size() + 1 + sizeof(mac_addr_t),
                mac_addr.begin()
            );
            if (!is_match_mac_addr) { return false; }
            *peer_id = frame[SOF.size() + 1 + sizeof(mac_addr_t)];
            return true;
        }
    private:
        static std::vector<uint8_t> header(uint8_t frame_type) {
            std::vector<uint8_t> header;
            header.insert(header.end(), SOF.begin(), SOF.end());
            header.push_back(frame_type);
            return header;
        }
        static bool is_start_sof(std::vector<uint8_t>& frame) {
            return std::equal(SOF.begin(), SOF.end(), frame.begin());
        }
};
