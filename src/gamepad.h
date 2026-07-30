#pragma once

#include "usbh_core.h"

extern TaskHandle_t gamepad_poll_task_handle;

void gamepad_poll_task(void*);
void gamepad_poll_task_init();
