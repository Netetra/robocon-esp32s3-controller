#include "usbh_core.h"
#include "general.h"

struct usbh_gamepad g_gamepad_class[CONFIG_GAMEPAD_MAX];
uint32_t g_devinuse = 0;

struct usbh_gamepad* usbh_gamepad_alloc(void) {
    uint8_t devno;
    for (devno = 0; devno < CONFIG_GAMEPAD_MAX; devno++) {
        if ((g_devinuse & (1U << devno)) == 0) {
            g_devinuse |= (1U << devno);
            memset(&g_gamepad_class[devno], 0, sizeof(struct usbh_gamepad));
            g_gamepad_class[devno].minor = devno;
            return &g_gamepad_class[devno];
        }
    }
    return NULL;
}

void usbh_gamepad_free(struct usbh_gamepad *gamepad_class) {
    uint8_t devno = gamepad_class->minor;
    if (devno < 32) { g_devinuse &= ~(1U << devno); }
    memset(gamepad_class, 0, sizeof(struct usbh_gamepad));
}

__WEAK void usbh_gamepad_run(struct usbh_gamepad *gamepad_class) {}
__WEAK void usbh_gamepad_stop(struct usbh_gamepad *gamepad_class) {}
