#pragma once

#include "driver/general.h"
#include "driver/ps4.h"

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
    protected:
        struct GamepadData current_data;
};

class PS4 : public Gamepad {
    public:
        PS4();
        bool ps();
        bool touchpad();
        void update_ps4(struct PS4Data *ps4_data);
    protected:
        struct PS4Data current_ps4_data;
};

bool gamepad_poll(enum GamepadType *gamepad_type, void* buffer);
void cherryusb_task_init(uint8_t led_pin);
void print_gamepad_state(struct GamepadData *gamepad_data);

enum class LedBlinkInterval { // milli second.
    DISCONNECTED = 100,
    CONNECTED = 1000,
};
