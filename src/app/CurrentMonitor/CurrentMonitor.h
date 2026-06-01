#pragma once

#include <stdbool.h>

// Current sensor pin
#define CS_PORT GPIOB
#define CS_PIN GPIO_PIN_13

#define CS_REF_PORT GPIOB
#define CS_REF_PIN  GPIO_PIN_14

bool CurrentMonitor_init();
void CurrentMonitor_task_update();

extern bool CurrentMonitor_backfeeding;
