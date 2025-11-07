#pragma once

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
        PB_SENSOR_READ_DEGRADED,
        PB_PARAMETER_ERROR
    } leos_purpleboard_result_t;

    typedef struct leos_purpleboard_readings
    {
        float temperature_c;
        float pressure_mb;
        uint32_t uvs;
        uint32_t pm10_env;
        uint32_t pm25_env;
        uint32_t pm100_env;
        uint32_t aqi_pm25_us;
        uint32_t aqi_pm100_us;
        float light_lux;
    } leos_purpleboard_readings_t;

    // The state struct managing the purpleboard classes
    typedef struct leos_purpleboard leos_purpleboard_t;

    leos_purpleboard_result_t leos_purpleboard_init(i2c_inst_t *i2c, uint sda, uint scl, leos_purpleboard_t **out_pb);

    leos_purpleboard_result_t leos_purpleboard_read(leos_purpleboard_t *pb, leos_purpleboard_readings_t *sensor_data);

    void leos_purpleboard_deinit(leos_purpleboard_t *pb);

#ifdef __cplusplus
}
#endif
