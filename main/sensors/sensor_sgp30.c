#include "sensor_sgp30.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"

#include "driver/i2c.h"
#include "hal/i2c_types.h"

#include "globals.h"


/* Parameters for the I2C interface */
#define I2C_SLAVE_ADDR              0x58

#define I2C_MASTER_NUM              I2C_NUM_0
#define I2C_MASTER_TX_BUF_DISABLE   0                    
#define I2C_MASTER_RX_BUF_DISABLE   0
#define I2C_MASTER_READ             1
#define I2C_MASTER_WRITE            0

#define I2C_ACK_EN                  1  // ack request to slave 
#define I2C_ACK                     0  // ack for each byte read 
#define I2C_NACK                    1  // nack for the last byte


/* SGP30 sensor commands */
static uint8_t INIT_AIR_QUALITY[2] = { 0x20, 0x03 };    // init command
static uint8_t MEASURE_AIR_QUALITY[2] = { 0x20, 0x08 }; // measurement command


/* Master sends a 2-bytes command to slave */
static void sgp30_i2c_command (i2c_cmd_handle_t* cmd, uint8_t* command) {
    /* Master (ESP micro) sends the address and operation mode to slave (SGP30) */
    i2c_master_start(*cmd);
    i2c_master_write_byte(*cmd, I2C_SLAVE_ADDR << 1 | I2C_MASTER_WRITE, I2C_ACK_EN);

    /* Then, sends the command */
    i2c_master_write_byte(*cmd, command[0], I2C_ACK_EN);
    i2c_master_write_byte(*cmd, command[1], I2C_ACK_EN);

    /* Finally, ends the operation */
    i2c_master_stop(*cmd);
}

esp_err_t sgp30_config (void) {

    int i2c_master_port = I2C_MASTER_NUM;
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER, // ESP micro is master
        .sda_io_num = CONFIG_I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = CONFIG_I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = CONFIG_I2C_MASTER_FREQ_HZ,
    };

    if(i2c_param_config(i2c_master_port, &conf) != ESP_OK) {
        ESP_LOGE(TAG_SGP30, "Error SGP30 sensor: i2c_param_config()");
        return ESP_FAIL;
    }

    if(i2c_driver_install(i2c_master_port, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0) != ESP_OK) {
        ESP_LOGE(TAG_SGP30, "Error SGP30 sensor: i2c_driver_install()");
        return ESP_FAIL;
    }

    return ESP_OK;

}

esp_err_t sgp30_init (void) {
    /* Send the init command */
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    sgp30_i2c_command(&cmd, INIT_AIR_QUALITY);
    ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS));
    i2c_cmd_link_delete(cmd);

    vTaskDelay(pdMS_TO_TICKS(5));

    return ESP_OK;
}

uint16_t sgp30_co2_reading (void) {

    uint8_t co2_ppm_h, co2_ppm_l, crc;

    i2c_cmd_handle_t cmd;
    
    /* Send measurement command */
    cmd = i2c_cmd_link_create();
    sgp30_i2c_command(&cmd, MEASURE_AIR_QUALITY);
    ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS));
    i2c_cmd_link_delete(cmd);

    vTaskDelay(pdMS_TO_TICKS(20));

    /* Receive the CO2 result split into two bytes */
    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, I2C_SLAVE_ADDR << 1 | I2C_MASTER_READ, I2C_ACK_EN);
    i2c_master_read_byte(cmd, &co2_ppm_h, I2C_ACK); // CO2 MSB
    i2c_master_read_byte(cmd, &co2_ppm_l, I2C_ACK); // CO2 LSB
    i2c_master_read_byte(cmd, &crc, I2C_NACK);
    i2c_master_stop(cmd);
    ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS));
    i2c_cmd_link_delete(cmd);

    return co2_ppm_h << 8 | co2_ppm_l;
    
}
