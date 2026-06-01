#pragma once

#include <stdint.h>
#include <stdbool.h>

#define FAULT_SHUTDOWN              0x00000001
#define FAULT_M17                   0x00000002
#define FAULT_ADES                  0x00000004
#define FAULT_CELL_VOLT_IRR         0x00000008
#define FAULT_CHIP_VOLT_IRR         0x00000010
#define FAULT_TEMP_IRR              0x00000020
#define FAULT_OUT_OF_JUICE          0x00000040
#define FAULT_VOLTAGE_DIFF          0x00000080
#define FAULT_CHARGER               0x00000100
#define FAULT_OVERTEMP              0x00000200
#define FAULT_BALANCING             0x00000400
#define FAULT_OVERCURRENT           0x00000800
#define FAULT_CURRENT_IRR           0x00001000

/*** M17 & ADES ***/
#define ERR_TX_NOT_RECEIVED         0x00000001
#define ERR_TX_LOOPBACK_MISMATCH    0x00000002
#define ERR_NUM_CHIPS_MISMATCH      0x00000004
#define ERR_LSSM_BYTE               0x00000008
#define ERR_LSSM_COMM_ERR           0x00000010
#define ERR_ALERT_RX                0x00000020
#define ERR_ALERT_RX_OVERFLOW       0x00000040
#define ERR_LDQ_READ                0x00000080
#define ERR_LDQ_FULL                0x00000100
#define ERR_RX_TIMEOUT              0x00000200
#define ERR_NO_RX                   0x00000400
#define ERR_ADES_SCAN_TIMEOUT       0x00000800
#define ERR_RX_STATUS               0x00001000
#define ERR_TX_NOT_WRITTEN          0x00002000
#define ERR_NO_WAKE                 0x00004000

#define ERR_BALANCING_NO_INIT       0x00008000
#define ERR_BALANCING_FAULT         0x00010000
#define ERR_CHARGING_DISCONNECTED   0x00020000


void FaultManager_set_fault(uint64_t faultCode);
void FaultManager_set_err(uint64_t errorCode, uint8_t tier);
void FaultManager_LSSM(uint8_t lssmByte);
void FaultManager_task_update();
uint64_t FaultManager_read();
void FaultManager_reset_voltage_faults();
