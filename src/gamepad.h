#pragma once

#include "driver/general.h"

#define CONFIG_PRINT_RAW_REPORT_DATA 0

struct Gamepad* gamepad_poll();
void cherryusb_task_init();
void print_gamepad_state(struct Gamepad *gamepad);
