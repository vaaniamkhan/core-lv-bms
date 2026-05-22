#include <stdint.h>
#include <stdbool.h>
#include <stddef.h> 
#include <math.h>

#include "timeout.h"
#include "rtt.h"

#include "PackMonitor.h"
#include "BMS.h"
#include "ADES.h"
#include "config.h"
#include "FaultManager.h"
#include "AppCAN.h"

float max_temp = TEMP_IRR_LOW_C;
float min_temp = TEMP_IRR_HIGH_C;
float avg_temp = 0;
float sum_temp; 

float max_cell_volt = CELL_VOLT_IRR_LOW_V;
float min_cell_volt = CELL_VOLT_IRR_HIGH_V;
float sum_cell_volt = 0;
float avg_cell_volt = 0;
float cell_volt_diff = 0;

float chip_volt = 0;

float cell_volt_arr[NUM_CELLS];
float chip_volt_arr[NUM_CHIPS];
float cell_temp_arr[NUM_THERMS];

static uint16_t raw_cell_volts[NUM_CELLS];
static uint16_t raw_chip_volts[NUM_CHIPS];
static uint16_t raw_temps[NUM_THERMS];

static core_timeout_t cell_volt_irr_timeout;
static core_timeout_t temp_irr_timeout;
static core_timeout_t out_of_juice_timeout;
static core_timeout_t voltage_diff_timeout;
static core_timeout_t overtemp_timeout;
static core_timeout_t chip_volt_irr_timeout; // required?

static void timeout_callback (core_timeout_t *timeout);

static void get_cell_volts();
static void get_chip_volt();
static void get_cell_volt_diff();
static bool get_cell_temps();


void PackMonitor_init()
{
    cell_volt_irr_timeout.module = NULL;
    cell_volt_irr_timeout.ref = FAULT_CELL_VOLT_IRR;
    cell_volt_irr_timeout.timeout = CELL_VOLT_IRR_TIMEOUT_MS;
    cell_volt_irr_timeout.callback = timeout_callback;
    cell_volt_irr_timeout.single_shot = 0;
    core_timeout_insert(&cell_volt_irr_timeout);

    temp_irr_timeout.module = NULL;
    temp_irr_timeout.ref = FAULT_TEMP_IRR;
    temp_irr_timeout.timeout = TEMP_IRR_TIMEOUT_MS;
    temp_irr_timeout.callback = timeout_callback;
    temp_irr_timeout.single_shot = 0;
    core_timeout_insert(&temp_irr_timeout);

    out_of_juice_timeout.module = NULL;
    out_of_juice_timeout.ref = FAULT_OUT_OF_JUICE;
    out_of_juice_timeout.timeout = OUT_OF_JUICE_TIMEOUT_MS;
    out_of_juice_timeout.callback = timeout_callback;
    out_of_juice_timeout.single_shot = 0;
    core_timeout_insert(&out_of_juice_timeout);

    voltage_diff_timeout.module = NULL;
    voltage_diff_timeout.ref = FAULT_VOLTAGE_DIFF;
    voltage_diff_timeout.timeout = CELL_DIFF_TIMEOUT_MS;
    voltage_diff_timeout.callback = timeout_callback;
    voltage_diff_timeout.single_shot = 0;
    core_timeout_insert(&voltage_diff_timeout);

    overtemp_timeout.module = NULL;
    overtemp_timeout.ref = FAULT_OVERTEMP;
    overtemp_timeout.timeout = OVERTEMP_TIMEOUT_MS;
    overtemp_timeout.callback = timeout_callback;
    overtemp_timeout.single_shot = 0;
    core_timeout_insert(&overtemp_timeout);

    chip_volt_irr_timeout.module = NULL;
    chip_volt_irr_timeout.ref = FAULT_CHIP_VOLT_IRR;
    chip_volt_irr_timeout.timeout = CHIP_VOLT_IRR_TIMEOUT_MS;
    chip_volt_irr_timeout.callback = timeout_callback;
    chip_volt_irr_timeout.single_shot = 0;
    core_timeout_insert(&chip_volt_irr_timeout);

}

bool PackMonitor_task_update() 
{
    chip_volt = 0;
    sum_cell_volt = 0;
    sum_temp = 0;
    cell_volt_diff = 0;
    max_temp = TEMP_IRR_LOW_C;
    min_temp = TEMP_IRR_HIGH_C;
    max_cell_volt = CELL_VOLT_IRR_LOW_V;
    min_cell_volt = CELL_VOLT_IRR_HIGH_V;
    ADES_collect_all(raw_cell_volts, raw_chip_volts, raw_temps);

    get_cell_volts();
    get_chip_volt();
    get_cell_volt_diff();
    get_cell_temps(); // returns bool

    avg_cell_volt = sum_cell_volt/NUM_CELLS;
    avg_temp = sum_temp/NUM_THERMS;

    if (!CAN_send_pack_data()) return false;

    return true;
}

static void get_cell_volts() 
{
    bool rational = true;
    bool low = false;
    float cell_volt = 0.0f;
    
    for (int cell = 0; cell < NUM_CELLS; cell++) 
    {
        cell_volt = (ADES_CELL_ADC_RANGE_V * (raw_cell_volts[cell] >> 2)/ADES_ADC_RANGE);

        rprintf("Cell %d: %d\n", cell, (int)(cell_volt * 1000));
        rprintf("Raw cell %d: %d\n", cell, raw_cell_volts[cell]);
        
        if (cell_volt > CELL_VOLT_IRR_HIGH_V || cell_volt < CELL_VOLT_IRR_LOW_V) rational = false;
        else {
            
            cell_volt_arr[cell] = cell_volt;
            
            if (cell_volt > max_cell_volt) max_cell_volt = cell_volt;
            else if (cell_volt < min_cell_volt) min_cell_volt = cell_volt;
            
            if (cell_volt <= CELL_VOLT_IRR_LOW_V) rational = false;
            
        }        
        sum_cell_volt += cell_volt_arr[cell];
    }
    
    rprintf("----------------------------------------\n\n");
    rprintf("Min: %d\n", (int)(min_cell_volt * 1000));
    rprintf("----------------------------------------\n\n");

    if (rational) core_timeout_reset(&cell_volt_irr_timeout);
    if (min_cell_volt >= CELL_MIN_V) core_timeout_reset(&out_of_juice_timeout);
}

static void get_cell_volt_diff()
{
    cell_volt_diff = max_cell_volt - min_cell_volt;
    if (cell_volt_diff <= CELL_MAX_DIFF_V) core_timeout_reset(&voltage_diff_timeout);
}

static void get_chip_volt()
{  
    chip_volt = (ADES_VBLK_ADC_RANGE_V * (raw_chip_volts[0] >> 2)/ADES_ADC_RANGE);
    rprintf("Chip: %d\n", (int)(chip_volt * 1000));

    // if ((chip_volt < VBLK_IRR_LOW_V) || (chip_volt > VBLK_IRR_HIGH_V)) FaultManager_set_fault(FAULT_CHIP_VOLT_IRR);
    if ((chip_volt > VBLK_IRR_LOW_V) || (chip_volt < VBLK_IRR_HIGH_V)) core_timeout_reset(&chip_volt_irr_timeout);
    else chip_volt_arr[0] = chip_volt;
        
    rprintf("----------------------------------------\n\n");
}

static bool get_cell_temps()
{
    bool rational = true;
    bool overtemp = false;
    
    for (int therm = 0; therm < NUM_THERMS; therm++) {
        
        float voltage = ADES_AUX_ADC_RANGE_V * (raw_temps[0] >> 2)/ADES_ADC_RANGE;
        float res = (voltage)/((ADES_THERM_V - voltage) / THERM_R1_R);          // Transfer function for Voltage -> Resistance
        float temp = 0.0f;
        if (res == 0) return false;
        else temp = (3892.0f)/(13.054-logf(THERM_NOMINAL_R/res)) - 273.15f;    // Transfer function for Resistance -> Temperature
        rprintf("Therm %d: %d\n", therm, (int)(voltage*1000));
        
        if (temp < TEMP_IRR_LOW_C || temp > (TEMP_IRR_HIGH_C)) rational = false;
        else {
            cell_temp_arr[therm] = temp;
            if (temp > max_temp) max_temp = temp;
            else if (temp < min_temp) min_temp = temp;
            if (temp > TEMP_MAX_C) overtemp = true;
        }
        sum_temp += temp;
    }

    // if (!rational) FaultManager_set_fault(FAULT_TEMP_IRR);
    // if (overtemp) FaultManager_set_fault(FAULT_OVERTEMP);
    
    if (rational) core_timeout_reset(&temp_irr_timeout);
    if (!overtemp) core_timeout_reset(&overtemp_timeout);
    rprintf("----------------------------------------\n\n");
    return true;
    
}

static void timeout_callback (core_timeout_t *timeout) {FaultManager_set_fault(timeout->ref);}
