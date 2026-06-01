#include "BMS.h"
#include "core_config.h"
#include <stm32g4xx_hal.h>

#include <stdbool.h>
#include <stdint.h>

#include "gpio.h"
#include "clock.h"
#include "rtt.h"
#include "can.h"
#include "timeout.h"
#include "boot.h"
#include "watchdog.h"

#include "config.h"
#include "AppGPIO.h"
#include "AppCAN.h"
#include "FaultManager.h"
#include "PackMonitor.h"
#include "CurrentMonitor.h"
#include "ChargeMonitor.h"

#include "M17.h"
#include "ADES.h"

bool LVBMS_init()
{
    core_heartbeat_init(HEARTBEAT_PORT, HEARTBEAT_PIN);
    
    if (!core_clock_init()) return false;
    
    core_RTT_init();
    GPIO_init();

    if (!M17_init()) return false;
    if (!ADES_init()) return false;

    // rprintf("Here %08x\n", *((uint32_t*)0x20017ffc));
    // if ((*((uint32_t*)0x20017ffc)) & 0x10) {
    //     core_GPIO_digital_write(STM_ENA_PORT, STM_ENA_PIN, 0);
    // }

    PackMonitor_init();
    CAN_init();
    core_boot_init();
    ChargeMonitor_init();
    CurrentMonitor_init();
    core_timeout_start_all();
    core_watchdog_init(false, NULL); 
    return true;    
}

bool LVBMS_1Hz()
{
    if (ChargeMonitor_is_charging()) FaultManager_reset_voltage_faults();
    PackMonitor_task_update();
    PowerManager_state_machine();
    FaultManager_task_update();
    return true;
}

bool LVBMS_1kHz()
{
    CurrentMonitor_task_update();
    core_timeout_check_all();
    core_watchdog_refresh();
    return true;
}
