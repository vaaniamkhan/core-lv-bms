#include "CurrentMonitor.h"

#include <stdbool.h>
#include <stdint.h>

#include "adc.h"
#include "can.h"
#include "filter.h"
#include "timeout.h"
#include "rtt.h"
#include "gpio.h"

#include "FaultManager.h"
#include "config.h"
#include "common_macros.h"
#include "AppCAN.h"
#include "AppGPIO.h"

core_filter_t filt;

static core_timeout_t overcurrent_timeout;
static core_timeout_t current_irr_timeout;
static int count = 0;

static void timeout_callback (core_timeout_t *timeout);

bool CurrentMonitor_init()
{
    if (!core_ADC_init(ADC2)) return false;
    core_ADC_setup_pin(CS_PORT, CS_PIN, 1);
    core_ADC_setup_pin(CS_REF_PORT, CS_REF_PIN, 1);

    core_filter_exp_lowpass_init(0.5f, &filt);

    overcurrent_timeout.module = NULL;
    overcurrent_timeout.ref = FAULT_OVERCURRENT;
    overcurrent_timeout.timeout = OVERCURRENT_TIMEOUT_MS;
    overcurrent_timeout.callback = timeout_callback;
    overcurrent_timeout.single_shot = 0;
    core_timeout_insert(&overcurrent_timeout);

    current_irr_timeout.module = NULL;
    current_irr_timeout.ref = FAULT_CURRENT_IRR;
    current_irr_timeout.timeout = CURRENT_IRR_TIMEOUT_MS;
    current_irr_timeout.callback = timeout_callback;
    current_irr_timeout.single_shot = 0;
    core_timeout_insert(&current_irr_timeout);

}   

void CurrentMonitor_task_update()
{
    uint16_t raw_cs = 1919;
    uint16_t raw_ref = 1919;
    core_ADC_read_channel(CS_PORT, CS_PIN, &raw_cs);
    core_ADC_read_channel(CS_REF_PORT, CS_REF_PIN, &raw_ref);
    rprintf("Raw CS: %d, Raw ref: %d, Raw current: %d\n", raw_cs, raw_ref, (raw_cs - raw_ref));
    float current = ((raw_cs - raw_ref) * 0.00899277f) - 0.0207157f;
    if ((current > CS_SHUTOFF_LOW) && (current < CS_SHUTOFF_HIGH)) count++;
    else count = ((count > 100) ? (count - 10) : 0);
    if (count > CS_SHUTOFF_COUNT) core_GPIO_digital_write(STM_ENA_PORT, STM_ENA_PIN, 0);

    // calibrate current

    // if (current < CS_IRR_L) {
    //     FaultManager_set_fault(FAULT_CURRENT_IRR);
    //     if (!CAN_pack_and_send_current(core_filter_update(current, &filt))) return false;
    // }
    // if ((current > OVERCURRENT_POS_I) || (current < OVERCURRENT_NEG_I)) FaultManager_set_fault(FAULT_OVERCURRENT);

    if (current > CS_IRR_L) {
        core_timeout_reset(&current_irr_timeout);
        CAN_pack_and_send_current(core_filter_update(current, &filt));
    }
    if ((current < OVERCURRENT_POS_I) && (current > OVERCURRENT_NEG_I)) core_timeout_reset(&overcurrent_timeout);
}

static void timeout_callback (core_timeout_t *timeout) {FaultManager_set_fault(timeout->ref);}
