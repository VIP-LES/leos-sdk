#include "leos/mcp342x.h"
#include "leos/log.h"
#include "pico/stdlib.h"

int mcp342x_init(mcp342x_dev_t *dev, i2c_inst_t *block, uint sda, uint scl, uint8_t addr) {
    if (dev == NULL) return -2;
    dev->i2c = block;
    dev->config = 0;
    i2c_init(block, 400000);
    gpio_set_function(sda, GPIO_FUNC_I2C);
    gpio_set_function(scl, GPIO_FUNC_I2C);
    gpio_pull_up(sda);
    gpio_pull_up(scl);
    dev->address = addr;
}

int mcp342x_set_channel(mcp342x_dev_t *dev, mcp342x_channel_t channel) {
    dev->config &= ~(0x3 << 5);          // clear bits [6:5]
    dev->config |= (channel & 0x3) << 5; // set

    return i2c_write_blocking(dev->i2c, dev->address, &dev->config, 1, false);
}

int mcp342x_set_gain(mcp342x_dev_t *dev, mcp342x_gain_t gain) {
    dev->config &= ~(0x3); // clear bits [1:0]
    dev->config |= (gain & 0x3);

    return i2c_write_blocking(dev->i2c, dev->address, &dev->config, 1, false);
}

int mcp342x_set_resolution(mcp342x_dev_t *dev, mcp342x_resolution_t resolution) {
    dev->config &= ~(0x3 << 2); // clear bits [3:2]
    dev->config |= (resolution & 0x3) << 2;

    return i2c_write_blocking(dev->i2c, dev->address, &dev->config, 1, false);
}

int mcp342x_set_mode(mcp342x_dev_t *dev, mcp342x_mode_t mode) {
    dev->config &= ~(1 << 4); // clear bit [4]
    dev->config |= (mode & 0x1) << 4;

    return i2c_write_blocking(dev->i2c, dev->address, &dev->config, 1, false);
}

int mcp342x_read_raw(mcp342x_dev_t *dev, int32_t *value, bool *ready) {
    uint8_t buf[3];

    int ret = i2c_read_blocking(dev->i2c, dev->address, buf, 3, false);
    if (ret < 0) {
        LOG_WARNING("No I2C ack for adc@%d", dev->address);
        return ret;
    }
    LOG_TRACE("adc@%d raw: %02X %02X %02X", dev->address, buf[0], buf[1], buf[2]);

    // RDY bit (bit 7 of config byte)
    if (ready) {
        *ready = !(buf[2] & 0x80);
    }

    // combine
    int16_t raw = (buf[0] << 8) | buf[1];

    // determine resolution from config
    uint8_t res = (dev->config >> 2) & 0x3;

    switch (res) {
    case MCP342X_RES_12:
        raw >>= 4;
        break;
    case MCP342X_RES_14:
        raw >>= 2;
        break;
    case MCP342X_RES_16:
    default:
        break;
    }

    *value = raw;
    return 0;
}

int mcp342x_read_blocking(mcp342x_dev_t *dev, int32_t *value) {
    bool ready = false;
    int ret;

    do {
        ret = mcp342x_read_raw(dev, value, &ready);
        if (ret < 0)
            return ret;
    } while (!ready);

    return 0;
}

int mcp342x_read_voltage(mcp342x_dev_t *dev, float *voltage) {
    int32_t raw;
    int ret = mcp342x_read_blocking(dev, &raw);
    if (ret < 0)
        return ret;

    uint8_t res = (dev->config >> 2) & 0x3;
    uint8_t gain = dev->config & 0x3;

    float lsb;

    switch (res) {
    case MCP342X_RES_12:
        lsb = 1e-3f;
        break; // 1 mV
    case MCP342X_RES_14:
        lsb = 250e-6f;
        break; // 250 µV
    case MCP342X_RES_16:
        lsb = 62.5e-6f;
        break; // 62.5 µV
    default:
        return -1;
    }

    int gain_val = 1 << gain; // 1,2,4,8

    *voltage = (raw * lsb) / gain_val;
    return 0;
}