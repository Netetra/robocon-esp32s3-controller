#include <freertos/FreeRTOS.h>
#include <Arduino.h>
#include "usbh_core.h"
#include "driver/general.h"
#include "led_blink.h"

const char* TAG = "Gamepad";
const uint32_t STACK_SIZE = 8196;
const uint8_t PRIORITY = 5;

extern volatile uint16_t led_blink_interval;
extern uint32_t g_devinuse;
extern struct usbh_gamepad g_gamepad_class[CONFIG_GAMEPAD_MAX];

TaskHandle_t gamepad_poll_task_handle;

void usbh_gamepad_callback(void *arg, int nbytes) {
    struct usbh_gamepad *gamepad_class = (struct usbh_gamepad*)arg;
    if (nbytes == -USB_ERR_NAK) {
        gamepad_class->is_active = false;
        return;
    }
    if (nbytes < 0) {
        USB_LOG_ERR("Unknown error. nbytes = %d\r\n", nbytes);
        return;
    }
    for (int i = 0; i < nbytes; i++) {
        printf("0x%02X ", gamepad_class->buffer[i]);
    }
    printf("\r\n");
    gamepad_class->is_active = false;
}

void gamepad_poll_task(void*) {
    while (true) {
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
                continue;
            }
            gamepad_class->is_active = true;
        }
        vTaskDelay(1);
    }
}

void gamepad_poll_task_init() {
    xTaskCreateUniversal(
        gamepad_poll_task,
        "GamePad Poll",
        STACK_SIZE,
        NULL,
        PRIORITY,
        &gamepad_poll_task_handle,
        CONFIG_ARDUINO_RUNNING_CORE
    );
}

extern "C" void usbh_gamepad_run(struct usbh_gamepad *gamepad_class) {
    gamepad_class->buffer = (uint8_t*)heap_caps_aligned_alloc(
        CONFIG_USB_ALIGN_SIZE,
        gamepad_class->ep_in->wMaxPacketSize,
        MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL
    );
    if (gamepad_class->buffer == NULL) {
        USB_LOG_ERR("malloc failed");
        return;
    }

    switch (gamepad_class->gamepad_type) {
        case GamepadType::Xbox:
            break;
        case GamepadType::PS4:
            break;
    }

    led_blink_interval = static_cast<uint16_t>(LedBlinkInterval::CONNECTED);
    gamepad_class->is_connected = true;
}

extern "C" void usbh_gamepad_stop(struct usbh_gamepad *gamepad_class) {
    heap_caps_free(gamepad_class->buffer);
    led_blink_interval = static_cast<uint16_t>(LedBlinkInterval::DISCONNECTED);
    gamepad_class->is_connected = false;
}
