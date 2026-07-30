#pragma once

#include "usbh_core.h"

#define CONFIG_GAMEPAD_MAX 1
#define BUFFER_SIZE 1024

enum class GamepadType {
    Xbox,
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
