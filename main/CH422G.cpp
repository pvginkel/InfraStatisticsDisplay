#include "includes.h"

#include "CH422G.h"

#include "I2CConf.h"

constexpr uint8_t CH422G_REG_SYSTEM_PARAMETERS = 0x24;
constexpr uint8_t CH422G_REG_OUT = 0x38;

void ch422g_begin() {
    uint8_t write_buf = 0x01;
    ESP_ERROR_CHECK(i2c_master_write_to_device(I2C_MASTER_NUM, CH422G_REG_SYSTEM_PARAMETERS, &write_buf, 1,
                                               I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS));
}

void ch422g_write_output(uint8_t pins) {
    ESP_ERROR_CHECK(i2c_master_write_to_device(I2C_MASTER_NUM, CH422G_REG_OUT, &pins, 1,
                                               I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS));
}
