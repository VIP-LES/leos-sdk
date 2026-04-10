#pragma once
#include "hardware/i2c.h"

typedef struct {
    i2c_inst_t *i2c;
    uint8_t address;
    uint8_t config;
} mcp342x_dev_t;

typedef enum {
    MCP342X_CH1 = 0,
    MCP342X_CH2 = 1,
    MCP342X_CH3 = 2,
    MCP342X_CH4 = 3
} mcp342x_channel_t;

typedef enum {
    MCP342X_GAIN_1 = 0,
    MCP342X_GAIN_2 = 1,
    MCP342X_GAIN_4 = 2,
    MCP342X_GAIN_8 = 3
} mcp342x_gain_t;

typedef enum {
    MCP342X_RES_12 = 0,
    MCP342X_RES_14 = 1,
    MCP342X_RES_16 = 2
} mcp342x_resolution_t;

typedef enum {
    MCP342X_ONE_SHOT = 0,
    MCP342X_CONTINUOUS = 1
} mcp342x_mode_t;

int mcp342x_init(mcp342x_dev_t *dev, i2c_inst_t *block, uint sda, uint scl, uint8_t addr);

int mcp342x_set_channel(mcp342x_dev_t *dev, mcp342x_channel_t channel);

int mcp342x_set_gain(mcp342x_dev_t *dev, mcp342x_gain_t gain);

int mcp342x_set_resolution(mcp342x_dev_t *dev, mcp342x_resolution_t resolution);

int mcp342x_set_mode(mcp342x_dev_t *dev, mcp342x_mode_t mode);

int mcp342x_read_raw(mcp342x_dev_t *dev, int32_t *value, bool *ready);

int mcp342x_read_blocking(mcp342x_dev_t *dev, int32_t *value);

int mcp342x_read_voltage(mcp342x_dev_t *dev, float *voltage);