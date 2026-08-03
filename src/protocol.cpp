#include <Arduino.h>
#include <esp_now.h>
#include <array>
#include "protocol.h"

std::vector<uint8_t> Frame::beacon() {
    return Frame::header(static_cast<uint8_t>(FrameCode::Beacon));
}

std::vector<uint8_t> Frame::pairing_request(mac_addr_t& mac_addr) {
    std::vector<uint8_t> frame = Frame::header(static_cast<uint8_t>(FrameCode::PairingRequest));
    frame.insert(frame.end(), mac_addr.begin(), mac_addr.end());
    return frame;
}

std::vector<uint8_t> Frame::pairing_accept(mac_addr_t& mac_addr, uint8_t peer_id) {
    std::vector<uint8_t> frame = Frame::header(static_cast<uint8_t>(FrameCode::PairingAccept));
    frame.insert(frame.end(), mac_addr.begin(), mac_addr.end());
    frame.push_back(peer_id);
    return frame;
}

std::vector<uint8_t> Frame::message(std::vector<struct Message>& messages) {
    std::vector<uint8_t> frame = Frame::header(static_cast<uint8_t>(FrameCode::PairingAccept));
    for (struct Message message: messages) {
        frame.push_back(message.type);
        frame.push_back(message.data.size());
        frame.insert(frame.end(), message.data.begin(), message.data.end());
    }
    return frame;
}

std::vector<uint8_t> Frame::custom(uint8_t frame_type, std::vector<uint8_t>& data) {
    std::vector<uint8_t> frame = Frame::header(static_cast<uint8_t>(FrameCode::PairingAccept));
    frame.insert(frame.end(), data.begin(), data.end());
    return frame;
}

bool Frame::is_beacon(std::vector<uint8_t>& frame) {
    if (frame.size() != (SOF.size() + 1)) { return false; }
    if (!Frame::is_start_sof(frame)) { return false; }
    return static_cast<uint8_t>(FrameCode::Beacon) == frame[SOF.size()];
}

bool Frame::is_pairing_request(std::vector<uint8_t>& frame, mac_addr_t& mac_addr) {
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

bool Frame::is_pairing_accept(std::vector<uint8_t>& frame, mac_addr_t& mac_addr, uint8_t *peer_id) {
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

std::vector<uint8_t> Frame::header(uint8_t frame_type) {
    std::vector<uint8_t> header;
    header.insert(header.end(), SOF.begin(), SOF.end());
    header.push_back(frame_type);
    return header;
}

bool Frame::is_start_sof(std::vector<uint8_t>& frame) {
    return std::equal(SOF.begin(), SOF.end(), frame.begin());
}