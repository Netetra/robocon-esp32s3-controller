#include <freertos/FreeRTOS.h>

#include "usbh_core.h"
#include "general.h"
#include "ps4.h"

#define DEV_FORMAT "/dev/dualshock%d"

const uint8_t TARGET_INTERFACE = 3;

extern uint32_t g_devinuse;
extern struct usbh_gamepad g_gamepad_class[CONFIG_GAMEPAD_MAX];

static int usbh_ps4_connect(struct usbh_hubport *hport, uint8_t intf);
static int usbh_ps4_disconnect(struct usbh_hubport *hport, uint8_t intf);

static const struct usbh_class_driver ps4_class_driver = {
    .driver_name = "PS4",
    .connect = usbh_ps4_connect,
    .disconnect = usbh_ps4_disconnect
};

CLASS_INFO_DEFINE const struct usbh_class_info ps4_class_info = {
    .match_flags = USB_CLASS_MATCH_VID_PID,
    .bInterfaceClass = 0,
    .bInterfaceSubClass = 0,
    .bInterfaceProtocol = 0,
    .id_table = (const uint16_t[][2]) {
        { 0x054c, 0x09cc }
    },
    .class_driver = &ps4_class_driver
};

static int usbh_ps4_connect(struct usbh_hubport *hport, uint8_t intf) {
    if (intf != TARGET_INTERFACE) { return 0; }
    struct usbh_gamepad *ps4_class = usbh_gamepad_alloc();
    if (ps4_class == NULL) {
        USB_LOG_ERR("Failed to allocate class\r\n");
        return -USB_ERR_NOMEM;
    }
    ps4_class->hport = hport;
    ps4_class->intf = intf;
    hport->config.intf[intf].priv = ps4_class;
    struct usb_endpoint_descriptor *ep_desc;
    for (uint8_t i = 0; i < hport->config.intf[intf].altsetting[0].intf_desc.bNumEndpoints; i++) {
        ep_desc = &hport->config.intf[intf].altsetting[0].ep[i].ep_desc;
        if (ep_desc->bEndpointAddress & 0x80) {
            USBH_EP_INIT(ps4_class->ep_in, ep_desc);
        } else {
            USBH_EP_INIT(ps4_class->ep_out, ep_desc);
        }
    }
    snprintf(hport->config.intf[intf].devname, CONFIG_USBHOST_DEV_NAMELEN, DEV_FORMAT, ps4_class->minor);
    USB_LOG_INFO("Register Dualshock4 Class:%s\r\n", hport->config.intf[intf].devname);
    ps4_class->gamepad_type = GamepadType::PS4;
    usbh_gamepad_run(ps4_class);
    return 0;
}

static int usbh_ps4_disconnect(struct usbh_hubport *hport, uint8_t intf) {
    struct usbh_gamepad *ps4_class = (struct usbh_gamepad *)hport->config.intf[intf].priv;
    if (ps4_class) {
        if (ps4_class->ep_in) { usbh_kill_urb(&ps4_class->ep_in_urb); }
        if (ps4_class->ep_out) { usbh_kill_urb(&ps4_class->ep_out_urb); }
        if (hport->config.intf[intf].devname[0] != '\0') {
            vTaskSuspendAll();
            USB_LOG_INFO("Unregister Dualshock4 Class:%s\r\n", hport->config.intf[intf].devname);
            usbh_gamepad_stop(ps4_class);
            xTaskResumeAll();
        }
        usbh_gamepad_free(ps4_class);
    }
    return 0;
}

int8_t fix_joystick_range(uint8_t value) {
    int16_t tmp = value;
    return tmp - 128;
}

void ps4_report_parser(uint8_t *buffer, struct GamepadData* gamepad, struct PS4Data * ps4) {
    gamepad->joystick_left.x = fix_joystick_range(buffer[1]);
    gamepad->joystick_left.y = fix_joystick_range(buffer[2]);
    gamepad->joystick_right.x =fix_joystick_range(buffer[3]);
    gamepad->joystick_right.y =fix_joystick_range(buffer[4]);

    gamepad->dpad = static_cast<enum Dpad>(buffer[5] & 0x0F);
    gamepad->buttons.bits.west = (buffer[5] & 0x10) != 0;
    gamepad->buttons.bits.south = (buffer[5] & 0x20) != 0;
    gamepad->buttons.bits.east = (buffer[5] & 0x40) != 0;
    gamepad->buttons.bits.north = (buffer[5] & 0x80) != 0;

    gamepad->buttons.bits.shoulder_left = (buffer[6] & 0x01) != 0;
    gamepad->buttons.bits.shoulder_right = (buffer[6] & 0x02) != 0;
    gamepad->buttons.bits.trigger_left = (buffer[6] & 0x04) != 0;
    gamepad->buttons.bits.trigger_right = (buffer[6] & 0x08) != 0;
    gamepad->buttons.bits.start = (buffer[6] & 0x10) != 0;
    gamepad->buttons.bits.select = (buffer[6] & 0x20) != 0;
    gamepad->buttons.bits.joystick_left = (buffer[6] & 0x40) != 0;
    gamepad->buttons.bits.joystick_right = (buffer[6] & 0x80) != 0;

    gamepad->trigger_left = buffer[8];
    gamepad->trigger_right = buffer[9];
}

bool ps4_set_state(struct PS4OutReport *state) {
    for (uint8_t i = 0; i < CONFIG_GAMEPAD_MAX; i++) {
        struct usbh_gamepad* gamepad_class = &(g_gamepad_class[i]);
        uint8_t devno = gamepad_class->minor;
        if ((g_devinuse & (1U << devno)) == 0) { continue; }
        if (gamepad_class->is_active || !gamepad_class->is_connected) { continue; }
        memcpy(gamepad_class->buffer, state, sizeof(struct PS4OutReport));
        usbh_int_urb_fill(
            &gamepad_class->ep_out_urb,
            gamepad_class->hport,
            gamepad_class->ep_out,
            gamepad_class->buffer,
            sizeof(struct PS4OutReport),
            0,
            NULL,
            NULL
        );
        int result = usbh_submit_urb(&gamepad_class->ep_out_urb);
        if (result != 0) {
            USB_LOG_WRN("Failed to submit URB.\r\n");
            return false;
        }
    }
    return true;
}