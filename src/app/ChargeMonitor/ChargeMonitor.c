#include <stdbool.h>
#include <stdint.h>

#include "gpio.h"
#include "config.h"
#include "can.h"

#include "AppGPIO.h"
#include "ChargeMonitor.h"
#include "PackMonitor.h"
#include "FaultManager.h"
#include "AppCAN.h"

#include "ADES.h"
#include "M17.h"

static ChargeState_e state;
static uint16_t bal_arr[NUM_CHIPS] = {0};
static unsigned long settling_start = 0;
static uint8_t samples = 0;
static bool chg_state = false;

bool ChargeMonitor_init()
{
    core_GPIO_digital_write(CHG_ENA_PORT, CHG_ENA_PIN, false);

    ChargeMonitor_set_state(ChargeState_DISCONNECTED);
    return false;
}

bool ChargeMonitor_task_update()
{
    if (core_GPIO_digital_read(CHG_IN_PORT, CHG_IN_PIN) != chg_state) {
        if (++samples >= CHG_IN_SAMPLES) {
            chg_state = !chg_state;
            state = chg_state ? ChargeState_CONNECTED : ChargeState_DISCONNECTED;
        }
    }
    else samples = 0;

    switch(state)
    {
        case ChargeState_DISCONNECTED:
            break;

        case ChargeState_CONNECTED:
            if (min_cell_volt > max_cell_volt) break;
            ChargeMonitor_set_state(ChargeState_CONNECTED_CHARGING);
            break;

        case ChargeState_CONNECTED_CHARGING:
        // if (min_cell >= CELL_FULL_MIN_V) sec_bus.chg_request.chg_charge_request_max_current = SCALE(min_cell, 4.0f, 4.35f, 10.0f, 0.5f);
            if (max_cell_volt >= CELL_FULL_MAX_V) ChargeMonitor_set_state(ChargeState_CONNECTED_SETTLING);
            break;

        case ChargeState_CONNECTED_SETTLING:
            if ((HAL_GetTick() - settling_start) < BAL_SETTLING_TIME_MS) break;

            if (min_cell_volt >= CELL_FULL_MIN_V) ChargeMonitor_set_state(ChargeState_CONNECTED_COMPLETE);    
            else {
                for (int cell = 0; cell < NUM_CELLS; cell++) {
                    if (cell_volt_arr[cell] >= CELL_FULL_MIN_V) bal_arr[0] |= 1 << cell;
                }
                if (!ADES_init_balancing(bal_arr)) return false;
                ChargeMonitor_set_state(ChargeState_CONNECTED_BALANCING);
            }
            break;

        case ChargeState_CONNECTED_BALANCING:
        ;
            bool done = true;
            uint16_t rxBuf[NUM_CHIPS];                                                                     
            if (!M17_read_ADES_reg(ADES_READALL, ADES_BALCTRL, rxBuf, NUM_CHIPS)) return false;
            // Check the CBACTIVE section of the BALCTRL register to determine state of balancing
            uint8_t cb_active = (rxBuf[0] >> 14) & 0b11;
            // rprintf("Chip %d, BALCTRL: %x, CB_ACTIVE: %x\n", chip, rxBuf[chip], cb_active);

            // if (cb_active == 0b00) {            // If cell balancing is disabled
                // FaultManager_set_err(ERR_BALANCING_NO_INIT, 0);
                // ChargeMonitor_set_state(ChargeState_CONNECTED_FAULTED);
            if (cb_active == 0b01) {       // If balancing is still in progress
                done = false;
            }
            else if (cb_active == 0b11) {       // If it's faulted
                FaultManager_set_err(ERR_BALANCING_FAULT, 0);
                ChargeMonitor_set_state(ChargeState_CONNECTED_FAULTED);
            }

            if (done) ChargeMonitor_set_state(ChargeState_CONNECTED_CHARGING);
            break;

        case ChargeState_CONNECTED_COMPLETE:
            break;

        case ChargeState_CONNECTED_FAULTED:
            break;
    }

    return true;
}

void ChargeMonitor_set_state(ChargeState_e _state) 
{
    if (state == ChargeState_CONNECTED_BALANCING && _state != ChargeState_CONNECTED_BALANCING) {
        ADES_stop_balancing();
    }
    if (state != ChargeState_CONNECTED_FAULTED) state = _state;

    switch (state)
    {
        case ChargeState_DISCONNECTED:
            break;
        case ChargeState_CONNECTED:
            break;
        case ChargeState_CONNECTED_CHARGING:
            if ((min_cell_volt >= CELL_FULL_MIN_V) && (max_cell_volt > min_cell_volt)) ChargeMonitor_set_state(ChargeState_CONNECTED_COMPLETE);
            else core_GPIO_digital_write(CHG_ENA_PORT, CHG_ENA_PIN, true);
            break;
        case ChargeState_CONNECTED_SETTLING:
            settling_start = HAL_GetTick();
            core_GPIO_digital_write(CHG_ENA_PORT, CHG_ENA_PIN, true);
            break;

        case ChargeState_CONNECTED_BALANCING:
            core_GPIO_digital_write(CHG_ENA_PORT, CHG_ENA_PIN, true);
            break;

        case ChargeState_CONNECTED_COMPLETE:
            core_GPIO_digital_write(CHG_ENA_PORT, CHG_ENA_PIN, true);
            break;

        case ChargeState_CONNECTED_FAULTED:
            FaultManager_set_fault(FAULT_CHARGER);
            core_GPIO_digital_write(CHG_ENA_PORT, CHG_ENA_PIN, true);
            break;
    }
}

ChargeState_e ChargeMonitor_get_state()
{
    return state;
}
