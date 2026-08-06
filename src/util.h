#pragma once

#include <esp_mac.h>
#include <esp_now.h>
#include "gamepad.h"
#include "task/esp_now.h"

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

void print_peer_info(esp_now_peer_info_t *peer) {
    struct PeerState *peer_state = (struct PeerState*)(peer->priv);
    printf(
        "Peer { address = " MACSTR ", priv.peer_id = 0x%02X\r\n",
        MAC2STR(peer->peer_addr),
        peer_state->peer_id
    );
}