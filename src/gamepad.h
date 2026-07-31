#pragma once

#include "driver/general.h"

#define CONFIG_PRINT_RAW_REPORT_DATA 0

class Gamepad {
    public:
        Gamepad();
        int8_t joystick_left_x();
        int8_t joystick_left_y();
        int8_t joystick_right_x();
        int8_t joystick_right_y();
        uint8_t trigger_left_value();
        uint8_t trigger_right_value();
        bool north();
        bool east();
        bool south();
        bool west();
        bool up();
        bool right();
        bool down();
        bool left();
        bool start();
        bool select();
        bool joystick_left();
        bool joystick_right();
        bool shoulder_left();
        bool shoulder_right();
        bool trigger_left();
        bool trigger_right();

        void update(struct GamepadData *gamepad_data);
    private:
        struct GamepadData current_data;
};

struct GamepadData* gamepad_poll();
void cherryusb_task_init();
void print_gamepad_state(struct GamepadData *gamepad_data);
