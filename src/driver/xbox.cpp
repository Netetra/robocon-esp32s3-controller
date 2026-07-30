#include "usbh_core.h"
#include "../led_blink.h"
#include "general.h"

#define DEV_FORMAT "/dev/xbox%d"

static int usbh_xbox_connect(struct usbh_hubport *hport, uint8_t intf);
static int usbh_xbox_disconnect(struct usbh_hubport *hport, uint8_t intf);

static const struct usbh_class_driver xbox_class_driver = {
    .driver_name = "Xbox",
    .connect = usbh_xbox_connect,
    .disconnect = usbh_xbox_disconnect
};

CLASS_INFO_DEFINE const struct usbh_class_info xbox_class_info = {
    .match_flags = USB_CLASS_MATCH_VID_PID | USB_CLASS_MATCH_INTF_CLASS | USB_CLASS_MATCH_INTF_SUBCLASS | USB_CLASS_MATCH_INTF_PROTOCOL,
    .bInterfaceClass = USB_DEVICE_CLASS_VEND_SPECIFIC,
    .bInterfaceSubClass = 71,
    .bInterfaceProtocol = 208,
    .id_table = (const uint16_t[][2]) {
        { 0x045e, 0x0b12 }
    },
    .class_driver = &xbox_class_driver
};

static int usbh_xbox_connect(struct usbh_hubport *hport, uint8_t intf) {
    if (intf != 0) { return 0; }
    struct usbh_gamepad *xbox_class = usbh_gamepad_alloc();
    if (xbox_class == NULL) {
        USB_LOG_ERR("Failed to allocate gamepad_class\r\n");
        return -USB_ERR_NOMEM;
    }
    xbox_class->hport = hport;
    xbox_class->intf = intf;
    hport->config.intf[intf].priv = xbox_class;
    struct usb_endpoint_descriptor *ep_desc;
    for (uint8_t i = 0; i < hport->config.intf[intf].altsetting[0].intf_desc.bNumEndpoints; i++) {
        ep_desc = &hport->config.intf[intf].altsetting[0].ep[i].ep_desc;
        if (ep_desc->bEndpointAddress & 0x80) {
            USBH_EP_INIT(xbox_class->ep_in, ep_desc);
        } else {
            USBH_EP_INIT(xbox_class->ep_out, ep_desc);
        }
    }
    snprintf(hport->config.intf[intf].devname, CONFIG_USBHOST_DEV_NAMELEN, DEV_FORMAT, xbox_class->minor);
    USB_LOG_INFO("Register Xbox Class:%s\r\n", hport->config.intf[intf].devname);
    xbox_class->gamepad_type = GamepadType::Xbox;
    usbh_gamepad_run(xbox_class);
    return 0;
}

static int usbh_xbox_disconnect(struct usbh_hubport *hport, uint8_t intf) {
    struct usbh_gamepad *xbox_class = (struct usbh_gamepad *)hport->config.intf[intf].priv;
    if (xbox_class) {
        if (xbox_class->ep_in) { usbh_kill_urb(&xbox_class->ep_in_urb); }
        if (xbox_class->ep_out) { usbh_kill_urb(&xbox_class->ep_out_urb); }
        if (hport->config.intf[intf].devname[0] != '\0') {
            // usb_osal_thread_schedule_other();
            USB_LOG_INFO("Unregister Xbox Class:%s\r\n", hport->config.intf[intf].devname);
            usbh_gamepad_stop(xbox_class);
        }
        usbh_gamepad_free(xbox_class);
    }
    return 0;
}
