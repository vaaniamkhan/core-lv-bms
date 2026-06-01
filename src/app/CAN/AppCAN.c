#include "AppCAN.h"

#include <stdbool.h>

#include "rtt.h"
#include "can.h"

#include "config.h"
#include "common_macros.h"

#include "ADES.h"
#include "PackMonitor.h"
#include "FaultManager.h"
#include "ChargeMonitor.h"
#include "AppGPIO.h"

SEC_BUS sec_bus;

void CAN_error_handler() {}

bool CAN_init()
{
    if (!core_CAN_init(CAN_SEC, 1000000)) return false;
    
    return true;
}

bool CAN_tx()
{
    if (!core_CAN_send_from_tx_queue_task(CAN_SEC)) return false;
    return true;
}

bool CAN_send_pack_data()
{
    bool ret = true;
    uint64_t packdata_msg = 0;

    sec_bus.pack_data.lvbms_chip_voltage = chip_volt;
    sec_bus.pack_data.lvbms_charge_state = (int)PowerManager_get_state();
    // sec_bus.pack_data.lvbms_drain_status_0 = drain_status_0;     // get drain status
    sensor_dbc_lvbms_pack_data_full_encode((uint8_t *)&packdata_msg, &sec_bus.pack_data, 8);
    if (!core_CAN_add_message_to_tx_queue(CAN_SEC, SENSOR_DBC_LVBMS_PACK_DATA_FRAME_ID, 8, packdata_msg)) ret = false;

    uint64_t voltages_msg = 0;
    sec_bus.voltages.lvbms_voltages_0 = cell_volt_arr[0];
    sec_bus.voltages.lvbms_voltages_1 = cell_volt_arr[1];
    sec_bus.voltages.lvbms_voltages_2 = cell_volt_arr[2];
    sec_bus.voltages.lvbms_voltages_3 = cell_volt_arr[3];
    sec_bus.voltages.lvbms_voltages_4 = cell_volt_arr[4];
    sec_bus.voltages.lvbms_voltages_5 = cell_volt_arr[5];
    sensor_dbc_lvbms_voltages_full_encode((uint8_t *)&voltages_msg, &sec_bus.voltages, 8);
    if (!core_CAN_add_message_to_tx_queue(CAN_SEC, SENSOR_DBC_LVBMS_VOLTAGES_FRAME_ID, 8, voltages_msg)) ret = false;

    uint64_t temps_msg = 0;
    sec_bus.temperatures.lvbms_temperatures_0 = cell_temp_arr[0];
    sec_bus.temperatures.lvbms_temperatures_1 = cell_temp_arr[1];
    sec_bus.temperatures.lvbms_temperatures_2 = cell_temp_arr[2];
    sec_bus.temperatures.lvbms_temperatures_3 = cell_temp_arr[3];
    sec_bus.temperatures.lvbms_temperatures_4 = cell_temp_arr[4];
    sec_bus.temperatures.lvbms_temperatures_5 = cell_temp_arr[5];
    sensor_dbc_lvbms_temperatures_full_encode((uint8_t *)&temps_msg, &sec_bus.temperatures, 8);
    if (!core_CAN_add_message_to_tx_queue(CAN_SEC, SENSOR_DBC_LVBMS_TEMPERATURES_FRAME_ID, 8, temps_msg)) ret = false;

    return ret;
}

bool CAN_pack_and_send_current(float current)   // not currently being sent
{
    int64_t msg = sensor_dbc_lvbms_current_lvbms_inst_current_filt_encode(current);
    if (!core_CAN_add_message_to_tx_queue(CAN_SEC, SENSOR_DBC_LVBMS_CURRENT_FRAME_ID, 8, msg)) return false;
    return true;
}

bool CAN_send_faults_errors(uint32_t faultList, uint32_t errorCodes)
{
    if (!core_CAN_add_message_to_tx_queue(CAN_SEC, SENSOR_DBC_LVBMS_FAULT_VECTOR_FRAME_ID, 8, faultList)) return false;
    if (!core_CAN_add_message_to_tx_queue(CAN_SEC, SENSOR_DBC_LVBMS_ERROR_CODES_FRAME_ID, 8, errorCodes)) return false;
    return true;
}
