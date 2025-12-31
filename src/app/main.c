#include "main.h" // not used?

#include <stdbool.h>
#include <stdio.h>

#include "can.h"
#include "clock.h" // not used?
#include "gpio.h"
#include "error_handler.h" // not used?
#include "boot.h"
#include "core_config.h" // not used?

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include <stm32g4xx_hal.h>

#include "BMS.h"
#include "AppCAN.h"

#define TASK_PRIORITY_HEARTBEAT (tskIDLE_PRIORITY + 1)
#define CAN_RX_PRIORITY (tskIDLE_PRIOIRITY + 2)
#define CAN_TX_PRIORITY (tskIDLE_PRIOIRITY + 2)

void hardfault_error_handler();

void heartbeat_task(void *pvParameters) {
    (void) pvParameters;
    while(true) {
        core_GPIO_toggle_heartbeat();
        vTaskDelay(100 * portTICK_PERIOD_MS);
    }
}

int main(void) {
    HAL_Init();

    // Drivers
    core_heartbeat_init(GPIOA, GPIO_PIN_5);
    core_GPIO_set_heartbeat(GPIO_PIN_RESET);

    if (!core_clock_init()) error_handler();
    if (!core_CAN_init(CORE_BOOT_FDCAN, 1000000)) error_handler();
    core_boot_init();

    int err = xTaskCreate(heartbeat_task, "heartbeat", 1000, NULL, 4, NULL);
    if (err != pdPASS) {
        error_handler(); // change error handler?
    }

    NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);

    // hand control over to FreeRTOS
    vTaskStartScheduler();

    // we should not get here ever
    error_handler(); // change error handler?
    return 1;
}

// Called when stack overflows from rtos
// Not needed in header, since included in FreeRTOS-Kernel/include/task.h
void vApplicationStackOverflowHook( TaskHandle_t xTask, char *pcTaskName) {
    (void) xTask;
    (void) pcTaskName;

    error_handler();
}

void hardfault_error_handler() { // use vs template's error_handler()?
    while(1) {
        core_GPIO_toggle_heartbeat();
        for (unsigned long long i = 0; i < 200000; i++); // max val?
    }
}