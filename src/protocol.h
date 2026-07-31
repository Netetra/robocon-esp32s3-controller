#pragma once

#include <Arduino.h>

const uint16_t BEACON_INTERVAL = 500; // milli second.
const uint16_t BEACON_DEADLINE = 50;

const uint8_t BROADCAST_MAC[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

const uint8_t SOF = 0x55;

enum class FrameCode: uint8_t {
    Beacon = 0x01,
    PairingRequest = 0x02,
    PairingAccept = 0x03
};

const uint8_t BEACON_FRAME[] = { SOF, static_cast<uint8_t>(FrameCode::Beacon) };
const uint8_t PAIRING_REQUEST_FRAME[] = { SOF, static_cast<uint8_t>(FrameCode::PairingRequest) };
const uint8_t PAIRING_ACCEPT_FRAME[] = { SOF, static_cast<uint8_t>(FrameCode::PairingAccept) };
