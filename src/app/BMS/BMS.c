#include "BMS.h"
#include <stdbool.h>

#include "gpio.h"
#include "clock.h"
#include "rtt.h"

#include "AppGPIO.h"

#include "M17.h"
#include "ADES.h"

/** Tasks:
    - Undervoltage + overcurrent (detected in driver?)
    - Handle startup + shutdown --> look at prev. BMS
*/

#define const OVERCURR_THRESHOLD
#define const UNDERVOLT_THRESHOLD

// Function stubs
uint16_t get_voltage(int cell);
uint16_t get_temp(int cell);

void battery_monitor(void) { // frequency?

    if (!bms_init()) return false;
    
}


bool bms_init() {

    if (!core_clock_init()) return false;
    core_RTT_init();
    GPIO_init();

    if (!M17_init()) return false;
    if (!ADES_init()) return false;
    
    return true;

}