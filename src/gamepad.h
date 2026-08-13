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
        void set_neutral();
    private:
        struct GamepadData current_data;
};

bool gamepad_poll(struct GamepadData *gamepad_data);
void cherryusb_task_init(uint8_t led_pin);
void print_gamepad_state(struct GamepadData *gamepad_data);

enum class LedBlinkInterval { // milli second.
    DISCONNECTED = 100,
    CONNECTED = 1000,
};
