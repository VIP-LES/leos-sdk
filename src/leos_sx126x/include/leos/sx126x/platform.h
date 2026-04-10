#pragma once

// Default definitions

#define SX1262_LORA_DEFAULT_STOP_TIMER_ON_PREAMBLE SX1262_BOOL_FALSE       /**< disable stop timer on preamble */
#define SX1262_LORA_DEFAULT_REGULATOR_MODE SX1262_REGULATOR_MODE_DC_DC_LDO /**< only ldo */
#define SX1262_LORA_DEFAULT_PA_CONFIG_DUTY_CYCLE 0x02                      /**< set +17dBm power */
#define SX1262_LORA_DEFAULT_PA_CONFIG_HP_MAX 0x03                          /**< set +17dBm power */
#define SX1262_LORA_DEFAULT_TX_DBM 17                                      /**< +17dBm */
#define SX1262_LORA_DEFAULT_RAMP_TIME SX1262_RAMP_TIME_10US                /**< set ramp time 10 us */
#define SX1262_LORA_DEFAULT_SF SX1262_LORA_SF_9                            /**< sf9 */
#define SX1262_LORA_DEFAULT_BANDWIDTH SX1262_LORA_BANDWIDTH_125_KHZ        /**< 125khz */
#define SX1262_LORA_DEFAULT_CR SX1262_LORA_CR_4_5                          /**< cr4/5 */
#define SX1262_LORA_DEFAULT_LOW_DATA_RATE_OPTIMIZE SX1262_BOOL_FALSE       /**< disable low data rate optimize */
#define SX1262_LORA_DEFAULT_RF_FREQUENCY 480000000U                        /**< 480000000Hz */
#define SX1262_LORA_DEFAULT_SYMB_NUM_TIMEOUT 0                             /**< 0 */
#define SX1262_LORA_DEFAULT_SYNC_WORD 0x3444U                              /**< public network */
#define SX1262_LORA_DEFAULT_RX_GAIN 0x94                                   /**< common rx gain */
#define SX1262_LORA_DEFAULT_OCP 0x38                                       /**< 140 mA */
#define SX1262_LORA_DEFAULT_PREAMBLE_LENGTH 12                             /**< 12 */
#define SX1262_LORA_DEFAULT_HEADER SX1262_LORA_HEADER_EXPLICIT             /**< explicit header */
#define SX1262_LORA_DEFAULT_BUFFER_SIZE 255                                /**< 255 */
#define SX1262_LORA_DEFAULT_CRC_TYPE SX1262_LORA_CRC_TYPE_ON               /**< crc on */
#define SX1262_LORA_DEFAULT_INVERT_IQ SX1262_BOOL_FALSE                    /**< disable invert iq */
#define SX1262_LORA_DEFAULT_CAD_SYMBOL_NUM SX1262_LORA_CAD_SYMBOL_NUM_2    /**< 2 symbol */
#define SX1262_LORA_DEFAULT_CAD_DET_PEAK 24                                /**< 24 */
#define SX1262_LORA_DEFAULT_CAD_DET_MIN 10                                 /**< 10 */
#define SX1262_LORA_DEFAULT_START_MODE SX1262_START_MODE_WARM              /**< warm mode */
#define SX1262_LORA_DEFAULT_RTC_WAKE_UP SX1262_BOOL_TRUE                   /**< enable rtc wake up */

typedef struct {
    int pin_sck;
    int pin_mosi;
    int pin_miso;
    int pin_cs;
    int pin_busy;
    int pin_irq;
} leos_sx126x_hw_t;

static leos_sx126x_hw_t* LEOS_RADIO_HW_1;
static leos_sx126x_hw_t* LEOS_RADIO_HW_2;