#pragma once

#include "usbh_core.h"

#define CONFIG_GAMEPAD_MAX 1
#define BUFFER_SIZE 1024

struct JoyStick { int8_t x; int8_t y; };

enum class Dpad: uint8_t {
    Up = 0,
    RightUp = 1,
    Right = 2,
    RightDown = 3,
    Down = 4,
    LeftDown = 5,
    Left = 6,
    LeftUp = 7,
    Neutral = 8
};

union Buttons {
    uint16_t raw;
    struct {
        uint16_t north: 1;
        uint16_t east: 1;
        uint16_t south: 1;
        uint16_t west: 1;
        uint16_t joystick_left: 1;
        uint16_t joystick_right: 1;
        uint16_t shoulder_left: 1;
        uint16_t shoulder_right: 1;
        uint16_t trigger_left: 1;
        uint16_t trigger_right: 1;
        uint16_t start: 1;
        uint16_t select: 1;
        uint16_t __reserved : 4;
    } bits;
};

struct __attribute__((packed)) GamepadData {
    struct JoyStick joystick_left;
    struct JoyStick joystick_right;
    uint8_t trigger_left;
    uint8_t trigger_right;
    union Buttons buttons;
    enum Dpad dpad;
};

enum class GamepadType {
    PS4
};

struct usbh_gamepad {
    struct usbh_hubport *hport;
    struct usb_endpoint_descriptor *ep_in;
    struct usb_endpoint_descriptor *ep_out;
    struct usbh_urb ep_in_urb;
    struct usbh_urb ep_out_urb;
    uint8_t intf;
    uint8_t minor;
    enum GamepadType gamepad_type;
    bool is_connected;
    bool is_active;
    bool is_receive;
    int nbytes;
    uint8_t *buffer;
};

struct usbh_gamepad* usbh_gamepad_alloc(void);
void usbh_gamepad_free(struct usbh_gamepad *gamepad_class);

#ifdef __cplusplus
extern "C" {
#endif
void usbh_gamepad_run(struct usbh_gamepad *gamepad_class);
void usbh_gamepad_stop(struct usbh_gamepad *gamepad_class);
#ifdef __cplusplus
}
#endif
