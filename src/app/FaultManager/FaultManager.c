#include <stdint.h>
#include "FaultManager.h"
#include "AppGPIO.h"
#include "AppCAN.h"
#include "rtt.h"
#include "config.h"
#include "can.h"
#include "ChargeMonitor.h"

static uint64_t faultList = 0;
static uint64_t ignoreList = 0;
static uint64_t errorList = 0;

void FaultManager_set_fault(uint64_t faultCode) 
{
    if (!(faultList & faultCode)) {
        faultList |= faultCode;
        if (!(faultList & ignoreList)) {
            faultList |= FAULT_SHUTDOWN;
            //LV_shutdown();
            CAN_send_faults_errors(faultList, errorList);
        }
    }
}

void FaultManager_set_err(uint64_t errorCode, uint8_t tier) 
{
    rprintf("Err: %x, tier: %d\n", errorCode, tier);
    if (errorCode < 0x00008000) FaultManager_set_fault(FAULT_M17);
    if (!(errorList & errorCode)) {
        errorList |= errorCode;
        CAN_send_faults_errors(faultList, errorList);
    }
}
    
void FaultManager_task_update() {
    CAN_send_faults_errors(faultList, errorList);
}

uint64_t FaultManager_read() {
    return faultList;
}

void FaultManager_reset_voltage_faults() {
    faultList &= ~(FAULT_OUT_OF_JUICE | FAULT_VOLTAGE_DIFF);
    if (faultList == FAULT_SHUTDOWN) faultList = 0;
}

void FaultManager_LSSM(uint8_t lssmByte) {}
