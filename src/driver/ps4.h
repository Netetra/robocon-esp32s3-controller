#pragma once

#include <Arduino.h>

#define PS4_OUT_REPORT_ID 0x05
#define PS4_OUT_REPORT_MAGIC_NUMBER 0xFF

struct PS4OutReport {
    uint8_t report_id;
    uint8_t magic_number;
    uint16_t __reserved;
    uint8_t small_rumble;
    uint8_t big_rumble;
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t flash_on;
    uint8_t flash_off;
};

struct PS4Data {
    union {
        uint8_t raw;
        struct {
            uint8_t touchpad: 1;
            uint8_t ps: 1;
            uint8_t __reserved: 6;
    } bits;
    } buttons;
};

void ps4_report_parser(uint8_t *buffer, struct GamepadData* gamepad, struct PS4Data *ps4);

bool ps4_set_state(struct PS4OutReport *state);