#include <stdbool.h>
#include <stdint.h>

#include "gpio.h"
#include "config.h"
#include "can.h"

#include "AppGPIO.h"
#include "ChargeMonitor.h"
#include "PackMonitor.h"
#include "FaultManager.h"
#include "CurrentMonitor.h"
#include "AppCAN.h"

#include "ADES.h"
#include "M17.h"

#define PM_STATE_RUNNING            0
#define PM_STATE_CHARGER_CONNECTED  1
#define PM_STATE_CHARGER_CHARGING   2
#define PM_STATE_CHARGER_SETTLING   3
#define PM_STATE_CHARGER_BALANCING  4
#define PM_STATE_CHARGER_COMPLETE   5
#define PM_STATE_BACKFEED_CHARGING  6
#define PM_STATE_BACKFEED_SETTLING  7
#define PM_STATE_BACKFEED_BALANCING 8
#define PM_STATE_BACKFEED_STOPPED   9
#define PM_STATE_FAULTED            10

#define FAULT_CHARGER_IGNORE_LIST (FAULT_OUT_OF_JUICE | FAULT_VOLTAGE_DIFF | FAULT_SHUTDOWN)

static uint8_t pm_state = PM_STATE_RUNNING;
static uint16_t bal_arr[NUM_CHIPS] = {0};
static unsigned long settling_start = 0;
static uint8_t samples = 0;
static bool chg_state = false;

static uint32_t elapsed = 0, state_ts = 0;

bool ChargeMonitor_init()
{
    core_GPIO_digital_write(CHG_ENA_PORT, CHG_ENA_PIN, false);

    return false;
}

static void new_state(uint8_t state) {
    if (FaultManager_read() & (~FAULT_CHARGER_IGNORE_LIST)) {
        // If anything except OUT_OF_JUICE is set, force the state to RUNNING
        // and disable LV output
        rprintf("Forcing RUNNING due to fault\n");
        state = PM_STATE_RUNNING;
        GPIO_set_LV(0);
    }
    if (state == PM_STATE_RUNNING) {
        GPIO_set_LV(!(FaultManager_read() & FAULT_SHUTDOWN));
    }
    else if (state == PM_STATE_FAULTED) GPIO_set_LV(0);
    else if (state == PM_STATE_CHARGER_CONNECTED) GPIO_set_LV(1);
    GPIO_set_charge_enable(pm_state == PM_STATE_CHARGER_CHARGING);
    if (pm_state != state) {
        state_ts = HAL_GetTick();
        if ((pm_state == PM_STATE_CHARGER_BALANCING) || (pm_state == PM_STATE_BACKFEED_BALANCING)) ADES_stop_balancing();
    }
    pm_state = state;
}

static bool ChargeMonitor_settling(uint8_t state_complete, uint8_t state_balancing) {
    if (elapsed < BAL_SETTLING_TIME_MS) return true;
    
    if (min_cell_volt >= CELL_FULL_MIN_V) new_state(state_complete);
    else {
        for (int cell = 0; cell < NUM_CELLS; cell++) {
            if (cell_volt_arr[cell] >= CELL_FULL_MIN_V) bal_arr[0] |= 1 << cell;
        }
        rprintf("Balancing enabled for %x\n", bal_arr[0]);
        if (!ADES_init_balancing(bal_arr)) return false;
        new_state(state_balancing);
    }
    return true;
}

static bool ChargeMonitor_balancing(uint8_t state_done) {
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
        new_state(PM_STATE_FAULTED);
    }

    if (done) new_state(state_done);
    return true;
}

bool PowerManager_state_machine() {
    elapsed = HAL_GetTick() - state_ts;
    rprintf("Initial state %d, PG %d, backfeed %d\n", pm_state, GPIO_PG_state(), CurrentMonitor_backfeeding);
    rprintf("Min cell %d, max cell %d\n", (int)(1000*min_cell_volt), (int)(1000*max_cell_volt));
    if (FaultManager_read() & (~FAULT_CHARGER_IGNORE_LIST)) new_state(PM_STATE_RUNNING);

    switch(pm_state) {
        case PM_STATE_RUNNING:
            if (!GPIO_PG_state()) {
                rprintf("Charger detected\n");
                new_state(PM_STATE_CHARGER_CONNECTED);
            } else if (CurrentMonitor_backfeeding) new_state(PM_STATE_BACKFEED_CHARGING);
            GPIO_set_LV(!(FaultManager_read()));
            break;

        case PM_STATE_CHARGER_CONNECTED:
            GPIO_set_LV(1);
            if (min_cell_volt > max_cell_volt) break;
            // Charge controller has asserted PG. In this state, LV is enabled.
            // If the power from the user is cycled, the controller will
            // deassert PG and assert BATFET. In this case, when power is
            // reapplied, current will flow directly into the battery and the
            // state machine will transition to CHARGER_BACKFEED.
            if (elapsed > 5000) {
                rprintf("Exiting connected\n");
                if (CurrentMonitor_backfeeding) {
                    new_state(PM_STATE_BACKFEED_CHARGING);
                } else if (GPIO_PG_state()) {
                    // Charger has been disconnected
                    new_state(PM_STATE_RUNNING);
                } else {
                    // Charger has not been disconnected and the user has not
                    // initiated backfeeding. Enable charge controller
                    new_state(PM_STATE_CHARGER_CHARGING);
                }
            }
            break;

        case PM_STATE_CHARGER_CHARGING:
        // if (min_cell >= CELL_FULL_MIN_V) sec_bus.chg_request.chg_charge_request_max_current = SCALE(min_cell, 4.0f, 4.35f, 10.0f, 0.5f);
            if (GPIO_PG_state()) new_state(PM_STATE_RUNNING);
            if (max_cell_volt >= CELL_FULL_MAX_V) new_state(PM_STATE_CHARGER_SETTLING);
            break;

        case PM_STATE_CHARGER_SETTLING:
            if (GPIO_PG_state()) new_state(PM_STATE_RUNNING);
            if (!ChargeMonitor_settling(PM_STATE_CHARGER_COMPLETE, PM_STATE_CHARGER_BALANCING)) return false;
            break;

        case PM_STATE_CHARGER_BALANCING:
            if (GPIO_PG_state()) new_state(PM_STATE_RUNNING);
            if (!ChargeMonitor_balancing(PM_STATE_CHARGER_CHARGING)) return false;
            break;
        
        case PM_STATE_CHARGER_COMPLETE:
            if (GPIO_PG_state()) new_state(PM_STATE_RUNNING);
            break;

        case PM_STATE_BACKFEED_CHARGING:
            if (max_cell_volt >= CELL_FULL_MAX_V) {
                // Turn off LV switch to stop current flow. This will cause the
                // charge controller to reassert PG.
                GPIO_set_LV(0);
                new_state(PM_STATE_BACKFEED_SETTLING);
            }
            if (!CurrentMonitor_backfeeding) {
                // Power disconnected
                new_state(PM_STATE_RUNNING);
            }
            break;

        case PM_STATE_BACKFEED_SETTLING:
            if (GPIO_PG_state()) new_state(PM_STATE_RUNNING);
            if (!ChargeMonitor_settling(PM_STATE_BACKFEED_STOPPED, PM_STATE_BACKFEED_BALANCING)) return false;
            break;

        case PM_STATE_BACKFEED_BALANCING:
            if (GPIO_PG_state()) new_state(PM_STATE_RUNNING);
            if (!ChargeMonitor_balancing(PM_STATE_BACKFEED_STOPPED)) return false;
            break;

        case PM_STATE_BACKFEED_STOPPED:
            if (GPIO_PG_state()) new_state(PM_STATE_RUNNING);
            break;

        case PM_STATE_FAULTED:
            break;
    }
    rprintf("Final state %d\n", pm_state);

    return true;
}

/*void ChargeMonitor_set_state(ChargeState_e _state) 
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
}*/

uint8_t PowerManager_get_state() {
    return pm_state;
}

bool ChargeMonitor_is_balancing() {
    return (pm_state == PM_STATE_CHARGER_BALANCING) || (pm_state == PM_STATE_BACKFEED_BALANCING);
}

bool ChargeMonitor_is_charging() {
    return (pm_state == PM_STATE_CHARGER_CHARGING) || (pm_state == PM_STATE_BACKFEED_CHARGING);
}
