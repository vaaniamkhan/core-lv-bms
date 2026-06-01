#include "ADES.h"

// Standard includes
#include <stdint.h>
#include <stdbool.h>

// Core includes
#include "gpio.h"
#include "rtt.h"

// Common includes
#include "config.h"
#include "common_macros.h"

// App includes
#include "AppGPIO.h"
#include "FaultManager.h"
#include "BMS.h"
#include "ChargeMonitor.h"
#include "PackMonitor.h"

// Driver includes
#include "M17.h"

static void poop_loop();
static bool scan();

bool ADES_init()
{
    rprintf("Begin ADES init\n");
    rprintf("num_active_chips: %d\n", NUM_CHIPS);
    
    if (!M17_write_ADES_reg(ADES_WRITEALL, ADES_DEVCFG1, ALIVECNT | ALERTEN | UARTCFG_EXT)) return false;
    rprintf("Init config\n");
    
    // Enable the block voltage measurement and every chip that's activated
    if (!M17_write_ADES_reg(ADES_WRITEDEV(0), ADES_MEASUREEN1, (MASK(NUM_CELLS) | BLOCKEN))) return false;
    rprintf("Init block voltages\n");

    // Enable the thermistor measurements on every chip that's activated

    if (!M17_write_ADES_reg(ADES_WRITEDEV(0), ADES_MEASUREEN2, MASK(NUM_THERMS))) return false;
    rprintf("Init therm voltages\n");

    // Set all AUX/GPIO pins to analog in
    if (!M17_write_ADES_reg(ADES_WRITEALL, ADES_AUXGPIOCFG, 0x0000)) return false;
    rprintf("Init aux inputs\n");

    // Amount of time the THRM pin is high for before sampling AUX with ADC. Needed to charge filter capacitor 
    // on the thermistors before measuring. This value is a coefficient, multiplied by 6us.
    if (!M17_write_ADES_reg(ADES_WRITEALL, ADES_AUXTIMEREG, AUX_SETTLING_TIME_COEFF)) return false;
    rprintf("Init aux settling time\n");
    poop_loop();
    rprintf("Finished ADES init\n");
    return true;
}

bool ADES_collect_all(uint16_t *raw_cell_voltages, uint16_t *raw_chip_voltages, uint16_t *raw_temps)
{
    if (!scan()) { return false; };
    uint8_t voltages_read = 0;
    uint8_t temps_read = 0;
    uint8_t rxLen = 0;
    for (int chip = 0; chip < NUM_CHIPS; chip++)

    rxLen = NUM_CELLS;
    if (!M17_read_ADES_block(0, ADES_CELLREG(0), raw_cell_voltages + voltages_read, rxLen)) return false;
    voltages_read += rxLen;  // Increment pointer to how many cells have been read
    
    rxLen = NUM_CELLS;
    if (!M17_read_ADES_block(0, ADES_AUXREG(0), raw_temps + temps_read, rxLen)) return false;
    temps_read += rxLen;    // Increment pointer to how many temps have been read

    if (!M17_read_ADES_reg(ADES_READALL, ADES_BLOCKREG, raw_chip_voltages, NUM_CHIPS)) return false;

    return true;
}

bool ADES_force_balance(uint8_t chip, uint8_t cell, bool reset)
{
    if (reset) {
        if (!M17_write_ADES_reg(ADES_WRITEDEV(chip), ADES_BALSWCTRL, (0))) return false;
    }
    else {
        // BALSWEN
        if (!M17_write_ADES_reg(ADES_WRITEDEV(chip), ADES_BALSWCTRL, (1 << cell))) return false;
    }
    // CBDUTY
    if (!M17_write_ADES_reg(ADES_WRITEDEV(chip), ADES_BALCTRL, (MASK(4) << 4))) return false;
    // CBEXP1
    if (!M17_write_ADES_reg(ADES_WRITEDEV(chip), ADES_BALEXP(0), 5)) return false;
    // CBMODE
    if (!M17_write_ADES_reg(ADES_WRITEDEV(chip), ADES_BALCTRL, (0b010 << 11) | (MASK(4) << 4))) return false;
    return true;
}

bool ADES_init_balancing(uint16_t *arr)
{
    if (!M17_write_ADES_reg(ADES_WRITEDEV(0), ADES_BALSWCTRL, (MASK(14) & arr[0]))) return false;

    // Set balancing duty cycle (CBDUTY) and enable cell-balancing measurements (CBMEASEN)
    if (!M17_write_ADES_reg(ADES_WRITEALL, ADES_BALCTRL, (BAL_DUTY_CYCLE_COEFF << 4) | CBIIRINIT)) return false;

    // Set max balancing expiration time, to be used as a secondary method for stopping balancing
    if (!M17_write_ADES_reg(ADES_WRITEALL, ADES_BALEXP(0), BAL_EXP)) return false;

    // Set to balance until cells hit (BAL_UV_THRESHOLD_V), as defined in config.h.
    // Pack the float into a 14bit ADC value, with a reference voltage of 5V
    uint16_t packed_balance_v = (uint16_t)((BAL_UV_THRESHOLD_V * ADES_ADC_RANGE)/ADES_CELL_ADC_RANGE_V);
    // Left shift by 2 because the value occupies bits [15:2] of 16bit ADES register
    if (!M17_write_ADES_reg(ADES_WRITEALL, ADES_BALAUTOUVTHR, (packed_balance_v << 2))) return false;
    
    uint16_t read;
    M17_read_ADES_reg(ADES_READDEV(0), ADES_BALAUTOUVTHR, &read, 1);
    rprintf("From init balancing: %x\n", read);

    if (!M17_write_ADES_reg(ADES_WRITEALL, ADES_BALCTRL, (BAL_DUTY_CYCLE_COEFF << 4) | CBIIRINIT | CBMEASEN_MEAS_UVTHR)) return false;

    // When balancing, every nth sample will be replaced with an ADC calibration to keep measurement accurate while pack heats up.
    if (!M17_write_ADES_reg(ADES_WRITEALL, ADES_BALDLYCTRL, BAL_CAL_PERIOD)) return false;
    
    // Initiate auto group balancing by second, keep other things written previously (CBDUTY/CBMEASEN)
    if (!M17_write_ADES_reg(ADES_WRITEALL, ADES_BALCTRL, (BAL_DUTY_CYCLE_COEFF << 4) | CBMODE_AUTO_GRP_SEC | CBIIRINIT | CBMEASEN_MEAS_UVTHR)) return false;
    // M17_read_ADES_reg(ADES_READDEV(0), ADES_BALAUTOUVTHR, &read, 1);
    // rprintf("From init balancing BALAUTOUVTHR: %x\n", read);

    return true;
}

bool ADES_stop_balancing()
{
    if (!M17_write_ADES_reg(ADES_WRITEALL, ADES_BALCTRL, 0)) return false;
    return true;
}

static void poop_loop()
{
    for (int cell = 0; cell < NUM_CELLS; cell++) {
        ADES_force_balance(0, cell, false);
        HAL_Delay(POOP_LOOP_DELAY_MS);
        ADES_force_balance(0, 0, true);
    }
}

static bool scan()
{
    bool scan_done = false;
    unsigned long t = 0;
    uint8_t num_transmits = 0;
    uint16_t scanBuf[NUM_CHIPS];
    if (ChargeMonitor_is_balancing())
    {
        // if (!M17_write_ADES_reg(ADES_WRITEALL, ADES_SCANCTRL, AMENDFILT | RDFILT)) return false;
        // return true;
        // HAL_Delay(2);
        while (!scan_done)
        {
            // Make sure it isn't taking too long to scan
            if (HAL_GetTick() - t >= ADES_SCAN_TIMEOUT_MS) {
                if (num_transmits >= ADES_SCAN_MAX_TRANSMITS) {
                    FaultManager_set_fault(FAULT_ADES);
                    FaultManager_set_err(ERR_ADES_SCAN_TIMEOUT, 0);
                    return false;
                }
                if (!M17_write_ADES_reg(ADES_WRITEALL, ADES_BALDATA, CBSCAN)) return false;
                rprintf("Sent scan while balancing\n");
                num_transmits++;
                t = HAL_GetTick();
            }
            if (!M17_read_ADES_reg(ADES_READALL, ADES_BALDATA, scanBuf, NUM_CHIPS)) return false;
            scan_done = true;
            if (!(scanBuf[0] & DATARDY_M)) scan_done = false;
        }
    }
    else
    {
        while (!scan_done)
        {
            // Make sure it isn't taking too long to scan
            if (HAL_GetTick() - t >= ADES_SCAN_TIMEOUT_MS) {
                if (num_transmits >= ADES_SCAN_MAX_TRANSMITS) {
                    FaultManager_set_fault(FAULT_ADES);
                    FaultManager_set_err(ERR_ADES_SCAN_TIMEOUT, 0);
                    return false;
                }
                // if (!M17_write_ADES_reg(ADES_WRITEALL, ADES_SCANCTRL, AMENDFILT | RDFILT | SCAN)) return false;
                if (!M17_write_ADES_reg(ADES_WRITEALL, ADES_SCANCTRL,SCAN)) return false;
                rprintf("Sent scan\n");
                num_transmits++;
                t = HAL_GetTick();
            }
            if (!M17_read_ADES_reg(ADES_READALL, ADES_SCANCTRL, scanBuf, NUM_CHIPS)) return false;
            scan_done = true;
            if (!(scanBuf[0] & DATARDY)) scan_done = false;
        }
    }
    return true;
}
