#include <freertos/FreeRTOS.h>
#include <Arduino.h>
#include "task/led_blink.h"
#include "driver/general.h"
#include "driver/ps4.h"
#include "gamepad.h"

#define TAG "Gamepad"

extern volatile uint16_t led_blink_interval;
extern uint32_t g_devinuse;
extern struct usbh_gamepad g_gamepad_class[CONFIG_GAMEPAD_MAX];
struct Gamepad gamepad;

void usbh_gamepad_callback(void *arg, int nbytes) {
    struct usbh_gamepad *gamepad_class = (struct usbh_gamepad*)arg;
    if (nbytes == -USB_ERR_NAK) {
        gamepad_class->is_active = false;
        return;
    }
    if (nbytes < 0) {
        USB_LOG_ERR("Unknown error. nbytes = %d\r\n", nbytes);
        gamepad_class->is_active = false;
        return;
    }
    gamepad_class->is_receive = true;
    gamepad_class->is_active = false;
    gamepad_class->nbytes = nbytes;
}

struct Gamepad* gamepad_poll() {
    for (uint8_t i = 0; i < CONFIG_GAMEPAD_MAX; i++) {
        struct usbh_gamepad* gamepad_class = &(g_gamepad_class[i]);
        uint8_t devno = gamepad_class->minor;
        if ((g_devinuse & (1U << devno)) == 0) { continue; }
        if (gamepad_class->is_active || !gamepad_class->is_connected) { continue; }
        usbh_int_urb_fill(
            &gamepad_class->ep_in_urb,
            gamepad_class->hport,
            gamepad_class->ep_in,
            gamepad_class->buffer,
            gamepad_class->ep_in->wMaxPacketSize,
            0,
            usbh_gamepad_callback,
            gamepad_class
        );
        int result = usbh_submit_urb(&gamepad_class->ep_in_urb);
        if (result != 0) {
            USB_LOG_WRN("Failed to submit URB.\r\n");
            return NULL;
        }
        gamepad_class->is_receive = false;
        gamepad_class->is_active = true;
        while (gamepad_class->is_active) { vTaskDelay(1); }

        if (!gamepad_class->is_receive) { continue; }

        #if CONFIG_PRINT_RAW_REPORT_DATA
        for (int i = 0; i < gamepad_class->nbytes; i++) {
            printf("%02X ", gamepad_class->buffer[i]);
        }
        printf("\r\n");
        #endif

        switch (gamepad_class->gamepad_type) {
            case GamepadType::PS4:
                ps4_report_parser(gamepad_class->buffer, &gamepad);
                break;
            default:
                continue;
        }
        return &gamepad;
    }
    return NULL;
}

extern "C" void usbh_gamepad_run(struct usbh_gamepad *gamepad_class) {
    gamepad_class->buffer = (uint8_t*)heap_caps_aligned_alloc(
        CONFIG_USB_ALIGN_SIZE,
        gamepad_class->ep_in->wMaxPacketSize,
        MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL
    );
    if (gamepad_class->buffer == NULL) {
        ESP_LOGE(TAG, "malloc failed");
        return;
    }

    switch (gamepad_class->gamepad_type) {
        case GamepadType::PS4:
            break;
    }

    led_blink_interval = static_cast<uint16_t>(LedBlinkInterval::CONNECTED);
    gamepad_class->is_connected = true;
}

extern "C" void usbh_gamepad_stop(struct usbh_gamepad *gamepad_class) {
    gamepad_class->is_connected = false;
    heap_caps_free(gamepad_class->buffer);
    led_blink_interval = static_cast<uint16_t>(LedBlinkInterval::DISCONNECTED);
}

void cherryusb_task_init() {
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    usbh_initialize(0, ESP_USBH_BASE);
}

void print_gamepad_state(struct Gamepad *gamepad) {
    printf(
        "\033[2K LStick: x = %4d y = %4d, RStick: x = %4d y = %4d\r\n",
        gamepad->joystick_left.x,
        gamepad->joystick_left.y,
        gamepad->joystick_right.x,
        gamepad->joystick_right.y
    );
    printf("\033[2K LTrigger: %3d, RTrigger: %3d\r\n", gamepad->trigger_left, gamepad->trigger_right);
    printf(
        "\033[2K Start: %d, Select: %d, LStick: %d, RStick: %d\r\n",
        gamepad->buttons.bits.start,
        gamepad->buttons.bits.select,
        gamepad->buttons.bits.joystick_left,
        gamepad->buttons.bits.joystick_right
    );
    printf(
        "\033[2K north: %d, east: %d, south: %d, west: %d\r\n",
        gamepad->buttons.bits.north,
        gamepad->buttons.bits.east,
        gamepad->buttons.bits.south,
        gamepad->buttons.bits.west
    );
    printf(
        "\033[2K Left shoulder: %d trigger: %d, Right shouler: %d, trigger: %d\r\n",
        gamepad->buttons.bits.shoulder_left,
        gamepad->buttons.bits.trigger_left,
        gamepad->buttons.bits.shoulder_right,
        gamepad->buttons.bits.trigger_right
    );
    printf(
        "\033[2K Dpad up: %d, down: %d, left: %d, right: %d\r\n",
        gamepad->dpad == Dpad::LeftUp || gamepad->dpad == Dpad::Up || gamepad->dpad == Dpad::RightUp,
        gamepad->dpad == Dpad::LeftDown || gamepad->dpad == Dpad::Down || gamepad->dpad == Dpad::RightDown,
        gamepad->dpad == Dpad::LeftUp || gamepad->dpad == Dpad::Left || gamepad->dpad == Dpad::LeftDown,
        gamepad->dpad == Dpad::RightUp || gamepad->dpad == Dpad::Right || gamepad->dpad == Dpad::RightDown
    );
    printf("\r\033[6A");
}
