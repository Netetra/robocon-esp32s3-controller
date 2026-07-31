#pragma once

#include <Arduino.h>

void ps4_report_parser(uint8_t *buffer, struct GamepadData* gamepad);
