#include <freertos/FreeRTOS.h>
#include <Arduino.h>
#include "driver/general.h"
#include "driver/ps4.h"
#include "gamepad.h"

#define TAG "Gamepad"

const uint32_t STACK_SIZE = 2048;
const uint8_t PRIORITY = 0;

extern volatile uint16_t led_blink_interval;
extern uint32_t g_devinuse;
extern struct usbh_gamepad g_gamepad_class[CONFIG_GAMEPAD_MAX];
volatile uint16_t led_blink_interval = static_cast<uint16_t>(LedBlinkInterval::DISCONNECTED);
TaskHandle_t led_blink_task_handle;

void print_gamepad_state(struct GamepadData *gamepad_data) {
    printf(
        "\033[2K LStick: x = %4d y = %4d, RStick: x = %4d y = %4d\r\n",
        gamepad_data->joystick_left.x,
        gamepad_data->joystick_left.y,
        gamepad_data->joystick_right.x,
        gamepad_data->joystick_right.y
    );
    printf("\033[2K LTrigger: %3d, RTrigger: %3d\r\n", gamepad_data->trigger_left, gamepad_data->trigger_right);
    printf(
        "\033[2K Start: %d, Select: %d, LStick: %d, RStick: %d\r\n",
        gamepad_data->buttons.bits.start,
        gamepad_data->buttons.bits.select,
        gamepad_data->buttons.bits.joystick_left,
        gamepad_data->buttons.bits.joystick_right
    );
    printf(
        "\033[2K north: %d, east: %d, south: %d, west: %d\r\n",
        gamepad_data->buttons.bits.north,
        gamepad_data->buttons.bits.east,
        gamepad_data->buttons.bits.south,
        gamepad_data->buttons.bits.west
    );
    printf(
        "\033[2K Left shoulder: %d trigger: %d, Right shouler: %d, trigger: %d\r\n",
        gamepad_data->buttons.bits.shoulder_left,
        gamepad_data->buttons.bits.trigger_left,
        gamepad_data->buttons.bits.shoulder_right,
        gamepad_data->buttons.bits.trigger_right
    );
    printf(
        "\033[2K Dpad up: %d, down: %d, left: %d, right: %d\r\n",
        gamepad_data->dpad == Dpad::LeftUp || gamepad_data->dpad == Dpad::Up || gamepad_data->dpad == Dpad::RightUp,
        gamepad_data->dpad == Dpad::LeftDown || gamepad_data->dpad == Dpad::Down || gamepad_data->dpad == Dpad::RightDown,
        gamepad_data->dpad == Dpad::LeftUp || gamepad_data->dpad == Dpad::Left || gamepad_data->dpad == Dpad::LeftDown,
        gamepad_data->dpad == Dpad::RightUp || gamepad_data->dpad == Dpad::Right || gamepad_data->dpad == Dpad::RightDown
    );
    printf("\r\033[6A");
}

void led_blink_task(void* args) {
    const uint8_t led_pin = *((uint8_t*)args);
    bool state = false;
    ESP_LOGI(TAG, "LED Blink task initialized. using GPIO%d", led_pin);
    while (true) {
        gpio_set_level(static_cast<gpio_num_t>(led_pin), state);
        state = !state;
        vTaskDelay(led_blink_interval);
    }
}

void led_blink_task_init(const uint8_t _led_pin) {
    uint8_t* led_pin = new uint8_t(_led_pin);
    pinMode(*led_pin, OUTPUT);
    gpio_set_direction(static_cast<gpio_num_t>(*led_pin), GPIO_MODE_OUTPUT);
    xTaskCreate(
        led_blink_task,
        TAG,
        STACK_SIZE,
        (void*)led_pin,
        PRIORITY,
        &led_blink_task_handle
    );
}

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

bool gamepad_poll(struct GamepadData *gamepad_data) {
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
            return false;
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
                ps4_report_parser(gamepad_class->buffer, gamepad_data);
                break;
            default:
                continue;
        }
        return true;
    }
    return false;
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

void cherryusb_task_init(uint8_t led_pin) {
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    usbh_initialize(0, ESP_USBH_BASE);
    led_blink_task_init(led_pin);
}

Gamepad::Gamepad() {
    this->set_neutral();
}

void Gamepad::update(struct GamepadData *gamepad_data) {
    if (gamepad_data == NULL) { return; }
    memcpy(&this->current_data, gamepad_data, sizeof(struct GamepadData));
}

int8_t Gamepad::joystick_left_x() {
    return this->current_data.joystick_left.x;
}

int8_t Gamepad::joystick_left_y() {
    return this->current_data.joystick_left.y;
}

int8_t Gamepad::joystick_right_x() {
    return this->current_data.joystick_right.x;
}

int8_t Gamepad::joystick_right_y() {
    return this->current_data.joystick_right.y;
}

uint8_t Gamepad::trigger_left_value() {
    return this->current_data.trigger_left;
}

uint8_t Gamepad::trigger_right_value() {
    return this->current_data.trigger_right;
}

bool Gamepad::north() {
    return this->current_data.buttons.bits.north == 1;
}

bool Gamepad::east() {
    return this->current_data.buttons.bits.east == 1;
}

bool Gamepad::south() {
    return this->current_data.buttons.bits.south == 1;
}

bool Gamepad::west() {
    return this->current_data.buttons.bits.west == 1;
}

bool Gamepad::up() {
    return
        this->current_data.dpad == Dpad::LeftUp ||
        this->current_data.dpad == Dpad::Up ||
        this->current_data.dpad == Dpad::RightUp;
}

bool Gamepad::right() {
    return
        this->current_data.dpad == Dpad::RightUp ||
        this->current_data.dpad == Dpad::Right ||
        this->current_data.dpad == Dpad::RightDown;
}

bool Gamepad::down() {
    return
        this->current_data.dpad == Dpad::LeftDown ||
        this->current_data.dpad == Dpad::Down ||
        this->current_data.dpad == Dpad::RightDown;
}

bool Gamepad::left() {
    return
        this->current_data.dpad == Dpad::LeftUp ||
        this->current_data.dpad == Dpad::Left ||
        this->current_data.dpad == Dpad::LeftDown;
}

bool Gamepad::start() {
    return this->current_data.buttons.bits.start == 1;
}

bool Gamepad::select() {
    return this->current_data.buttons.bits.select == 1;
}

bool Gamepad::joystick_left() {
    return this->current_data.buttons.bits.joystick_left == 1;
}

bool Gamepad::joystick_right() {
    return this->current_data.buttons.bits.joystick_right == 1;
}

bool Gamepad::shoulder_left() {
    return this->current_data.buttons.bits.shoulder_left == 1;
}

bool Gamepad::shoulder_right() {
    return this->current_data.buttons.bits.shoulder_right == 1;
}

bool Gamepad::trigger_left() {
    return this->current_data.buttons.bits.trigger_left == 1;
}

bool Gamepad::trigger_right() {
    return this->current_data.buttons.bits.trigger_right == 1;
}

void Gamepad::set_neutral() {
    this->current_data.joystick_left = { 0, 0 };
    this->current_data.joystick_left = { 0, 0 };
    this->current_data.trigger_left = 0;
    this->current_data.trigger_right = 0;
    this->current_data.buttons.raw = 0;
    this->current_data.dpad = Dpad::Neutral;
}
