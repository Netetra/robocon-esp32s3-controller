#include "usbh_core.h"
#include "../led_blink.h"
#include "general.h"

#define DEV_FORMAT "/dev/dualshock%d"

static int usbh_ps4_connect(struct usbh_hubport *hport, uint8_t intf);
static int usbh_ps4_disconnect(struct usbh_hubport *hport, uint8_t intf);

static const struct usbh_class_driver ps4_class_driver = {
    .driver_name = "PS4",
    .connect = usbh_ps4_connect,
    .disconnect = usbh_ps4_disconnect
};

CLASS_INFO_DEFINE const struct usbh_class_info ps4_class_info = {
    .match_flags = USB_CLASS_MATCH_VID_PID | USB_CLASS_MATCH_INTF_CLASS | USB_CLASS_MATCH_INTF_SUBCLASS | USB_CLASS_MATCH_INTF_PROTOCOL,
    .bInterfaceClass = USB_DEVICE_CLASS_HID,
    .bInterfaceSubClass = 0,
    .bInterfaceProtocol = 0,
    .id_table = (const uint16_t[][2]) {
        { 0x0000, 0x0000 }
    },
    .class_driver = &ps4_class_driver
};

static int usbh_ps4_connect(struct usbh_hubport *hport, uint8_t intf) {
    if (intf != 0) { return 0; }
    struct usbh_gamepad *ps4_class = usbh_gamepad_alloc();
    if (ps4_class == NULL) {
        USB_LOG_ERR("Failed to allocate gamepad_class\r\n");
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
            // usb_osal_thread_schedule_other();
            USB_LOG_INFO("Unregister Dualshock4 Class:%s\r\n", hport->config.intf[intf].devname);
            usbh_gamepad_stop(ps4_class);
        }
        usbh_gamepad_free(ps4_class);
    }
    return 0;
}
