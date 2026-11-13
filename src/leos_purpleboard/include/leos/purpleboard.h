#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "hardware/i2c.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum
    {
        PB_OK = 0,
        PB_SENSOR_NO_DETECT,
        PB_SENSOR_READ_ERROR,
        PB_PARAMETER_ERROR
    } leos_purpleboard_result_t;

    typedef struct leos_purpleboard_bme680_readings
    {
        bool valid;
        float temperature_c;
        float pressure_mb;
        float humidity;
        float altitude_m;
        uint32_t gas_resistance;    
    } leos_purpleboard_bme680_readings_t;

    typedef struct leos_purpleboard_tsl2591_readings
    {
        bool valid;
        float lux;
        uint16_t raw_visible;
        uint16_t raw_infrared;
        uint32_t raw_full_spectrum;
    } leos_purpleboard_tsl2591_readings_t;

    typedef struct leos_purpleboard_ltr390_readings
    {
        bool valid;
        uint32_t ltr390_uvs;
    } leos_purpleboard_ltr390_readings_t;

    typedef struct leos_purpleboard_pmsa003i_readings
    {
        bool valid;
        uint32_t pm10_env;
        uint32_t pm25_env;
        uint32_t pm100_env;
        uint32_t aqi_pm25_us;
        uint32_t aqi_pm100_us;
        uint32_t particles_03um;
        uint32_t particles_05um;
        uint32_t particles_10um;
        uint32_t particles_25um;
        uint32_t particles_50um;
        uint32_t particles_100um;
    } leos_purpleboard_pmsa003i_readings_t;

    // The state struct managing the purpleboard classes
    typedef struct leos_purpleboard leos_purpleboard_t;

    leos_purpleboard_result_t leos_purpleboard_init(i2c_inst_t *i2c, uint sda, uint scl, leos_purpleboard_t **out_pb);

    leos_purpleboard_result_t leos_purpleboard_read_bme680(leos_purpleboard_t *pb, leos_purpleboard_bme680_readings_t *sensor_data, bool *did_read, int *remaining_time_millis);
    leos_purpleboard_result_t leos_purpleboard_read_tsl2591(leos_purpleboard_t *pb, leos_purpleboard_tsl2591_readings_t *readings);
    leos_purpleboard_result_t leos_purpleboard_read_ltr390(leos_purpleboard_t *pb, leos_purpleboard_ltr390_readings_t *readings, bool *did_read);
    leos_purpleboard_result_t leos_purpleboard_read_pmsa003i(leos_purpleboard_t *pb, leos_purpleboard_pmsa003i_readings_t *readings);


    void leos_purpleboard_deinit(leos_purpleboard_t *pb);

#ifdef __cplusplus
}
#endif
