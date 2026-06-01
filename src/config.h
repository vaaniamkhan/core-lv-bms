/*** ARCHITECTURE LAYOUT ***/
#define NUM_CHIPS 1
#define NUM_CELLS 6
#define NUM_THERMS 6

// Thermistor constants
#define THERM_R1_R 10000.0f
#define THERM_NOMINAL_R 10000.0f

/*** ADES175x CONSTANTS ***/
#define MAX_CELLS_PER_CHIP 14
#define MAX_THERMS_PER_CHIP 6
#define MAX_CHIPS 32
#define ADES_CELL_ADC_RANGE_V 5.0f
#define ADES_THERM_V 1.8f
#define ADES_AUX_ADC_RANGE_V 1.8f
#define ADES_VBLK_ADC_RANGE_V 65.0f
#define ADES_ADC_RANGE 16383.0f

/*** SPI CONFIG ***/
#define SPI_INIT_WAIT_MS 100

/*** CAN CONFIG ***/
#define CAN_SEC FDCAN1

/*** TIMEOUTS ***/
#define ADES_WAKE_TIMEOUT_MS 1000
#define ADES_TX_CONFIRM_TIMEOUT_MS 50           // Amount of time to wait after sending a message to see it in the UART RX queue
#define ADES_TX_TIMEOUT_MS 100                  // Amount of time to wait after sending a register read command to receive the data
#define ADES_RX_TIMEOUT_MS 1000                 // Timeout for receiving a message from the ADES after transmitting a register read
#define ADES_SCAN_TIMEOUT_MS 2000               // Amount of time to wait for the data to be ready after requesting a scan
#define M17_LDQ_TIMEOUT_MS 500
#define OUT_OF_JUICE_TIMEOUT_MS 5000
#define CELL_VOLT_IRR_TIMEOUT_MS 5000
#define TEMP_IRR_TIMEOUT_MS 5000
#define OVERTEMP_TIMEOUT_MS 5000
#define CELL_DIFF_TIMEOUT_MS 5000
#define CHIP_VOLT_IRR_TIMEOUT_MS 5000

// Change?
#define OVERCURRENT_TIMEOUT_MS 5000             
#define CURRENT_IRR_TIMEOUT_MS 5000

/*** MAX17851 CONFIG ***/
#define M17_MAX_RET 3 // Maximum number of times the STM can unsuccessfully configure a parameter on the MAX17851

/*** MEASUREMENTS ***/
#define CELL_VOLT_IRR_LOW_V 2.0f
#define CELL_MIN_V 3.1f
#define CELL_FULL_MIN_V 4.34f
#define CELL_FULL_MAX_V 4.35f
//#define CELL_FULL_MIN_V 3.73f
//#define CELL_FULL_MAX_V 3.8f
#define CELL_VOLT_IRR_HIGH_V 4.5f
#define TEMP_IRR_LOW_C 0.0f
#define TEMP_IRR_HIGH_C 100.0f
#define TEMP_MAX_C 60.0f
#define VBLK_IRR_LOW_V 30.0f
#define VBLK_IRR_HIGH_V 70.0f
#define CELL_MAX_DIFF_V 0.5f
#define CURRENT_IRR_I 500
#define OVERCURRENT_POS_I 350
#define OVERCURRENT_NEG_I -41
#define AUX_SETTLING_TIME_COEFF 500 //This value is a coefficient, multiplied by 6us.

/*** BALANCING ***/
#define BAL_UV_THRESHOLD_V CELL_FULL_MIN_V - 0.03f
#define BAL_UV_THRESHOLD_PACKED ((uint16_t)((BAL_UV_THRESHOLD_V * ADES_ADC_RANGE)/ADES_CELL_ADC_RANGE_V) << 2);
#define BAL_EXP 60
#define BAL_DUTY_CYCLE_COEFF 0xa   // Duty cycle in units of 6.25%, with 0x0 = 6.25% and 0xF = 100%
// When balancing, every nth sample will be replaced with an ADC calibration to keep measurement accurate while pack heats up.
// 0x0 = Disabled, 0x1 = 2 (every other) cycles, 0x2 = 4 cycles, 0x3 = 8, 0x4 = 12, 0x5 = 16, 0x6 = 24
#define BAL_CAL_PERIOD 0x4
#define BAL_SETTLING_TIME_MS 10000

/*** CURRENT ***/
#define CS_OFFSET_VOLTAGE 2.5f
#define CS_GAIN 250.0f
#define CS_IRR_L -200.0f
#define CS_SHUTOFF_LOW -0.08f
#define CS_SHUTOFF_HIGH 0.08f
#define CS_SHUTOFF_COUNT 5000
#define CS_BACKFEED_THRESHOLD (-1.0f)
#define CS_BACKFEED_COUNT 500
