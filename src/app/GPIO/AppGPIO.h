#pragma once

#include <stdbool.h>
#include "stm32g4xx_hal.h"
#include "core.h"

//heartbeat
#define HEARTBEAT_PORT GPIOB
#define HEARTBEAT_PIN GPIO_PIN_15

// LED 1
#define LED1_PORT GPIOC
#define LED1_PIN GPIO_PIN_1

// LED 2
#define LED2_PORT GPIOC
#define LED2_PIN GPIO_PIN_2

// STM enable pin
#define STM_ENA_PORT GPIOB
#define STM_ENA_PIN GPIO_PIN_1
#define GPIO_set_STM(state) core_GPIO_digital_write(STM_ENA_PORT, STM_ENA_PIN, state)

// Charger connected
#define CHG_IN_PORT GPIOB
#define CHG_IN_PIN GPIO_PIN_10

// Charging enable
#define CHG_ENA_PORT GPIOA
#define CHG_ENA_PIN GPIO_PIN_7
#define GPIO_set_charge_enable(state) core_GPIO_digital_write(CHG_ENA_PORT, CHG_ENA_PIN, state)

// LV enable pin
#define LV_ENA_PORT GPIOA
#define LV_ENA_PIN GPIO_PIN_6
#define GPIO_set_LV(state) core_GPIO_digital_write(LV_ENA_PORT, LV_ENA_PIN, state)

#define PG_PORT GPIOA
#define PG_PIN GPIO_PIN_5
#define GPIO_PG_state() core_GPIO_digital_read(PG_PORT, PG_PIN)

void GPIO_init();
void GPIO_Task_Update();
void LV_shutdown();
