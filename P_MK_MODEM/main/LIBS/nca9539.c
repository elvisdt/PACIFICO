/*
 * nca9539.c
 *
 *  Created on: 4 jul. 2024
 *      Author: Elvis de la torre
 */

#include "nca9539.h"
static const char *TAG = "NCA9539";


static esp_err_t I2CGetValue(uint8_t address, uint8_t reg, uint16_t *data) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (address << 1) | I2C_MASTER_WRITE, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, reg, ACK_CHECK_EN);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (address << 1) | I2C_MASTER_READ, ACK_CHECK_EN);
    i2c_master_read(cmd, (uint8_t*) data, 2, I2C_MASTER_LAST_NACK); // Lee 2 bytes
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

static esp_err_t I2CSetValue(uint8_t address, uint8_t reg, uint16_t data) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (address << 1) | I2C_MASTER_WRITE, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, reg, ACK_CHECK_EN);
    i2c_master_write(cmd, (uint8_t*) &data, 2, ACK_CHECK_EN); // Escribe 2 bytes
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

NCA9539_I2C * Nca539_i2c_init(uint8_t address) {
    NCA9539_I2C* dev = (NCA9539_I2C*) malloc(sizeof(NCA9539_I2C));
    if (dev == NULL) {
        ESP_LOGE(TAG, "Could not allocate memory for NCA9539");
        return NULL;
    }
    dev->address = address;
    dev->cfg_register = 0xFFFF; // default to all inputs
    dev->value_register = 0x0000;
    return dev;
}

void Nca9539_set_pin_mode(NCA9539_I2C* dev, uint8_t pin, uint8_t IOMode) {
    if (pin < 8) {
        if (IOMode == 0) { // input
            dev->cfg_register |= (1 << pin);
        } else { // output
            dev->cfg_register &= ~(1 << pin);
        }
    } else {
        if (IOMode == 0) { // input
            dev->cfg_register |= (1 << (pin - 8));
        } else { // output
            dev->cfg_register &= ~(1 << (pin - 8));
        }
    }
    I2CSetValue(dev->address, NXP_CONFIG, dev->cfg_register);
}

void Nca9539_set_block_mode(NCA9539_I2C* dev, uint16_t IOMode) {
    dev->cfg_register = IOMode;
    I2CSetValue(dev->address, NXP_CONFIG, dev->cfg_register);
}

uint8_t Nca9539_read_pin(NCA9539_I2C* dev, uint8_t pin) {
    uint16_t data;
    I2CGetValue(dev->address, NXP_INPUT, &data);
    if (pin < 8) {
        return (data & (1 << pin)) ? 1 : 0;
    } else {
        return (data & (1 << (pin - 8))) ? 1 : 0;
    }
}

uint16_t Nca9539_read_block(NCA9539_I2C* dev) {
    uint16_t data;
    I2CGetValue(dev->address, NXP_INPUT, &data);
    return data;
}

void Nca9539_write_pin(NCA9539_I2C* dev, uint8_t pin, uint8_t value) {
    if (pin < 8) {
        if (value == 0) {
            dev->value_register &= ~(1 << pin);
        } else {
            dev->value_register |= (1 << pin);
        }
    } else {
        if (value == 0) {
            dev->value_register &= ~(1 << (pin - 8));
        } else {
            dev->value_register |= (1 << (pin - 8));
        }
    }
    I2CSetValue(dev->address, NXP_OUTPUT, dev->value_register);
}

void Nca9539_write_block(NCA9539_I2C* dev, uint16_t value) {
    dev->value_register = value;
    I2CSetValue(dev->address, NXP_OUTPUT, dev->value_register);
}


/*************************************************************
 * GPIO FUNCITONS
 **************************************************************/
void Nca9539_reset(int pin){
    gpio_set_level(pin, 1);
    vTaskDelay(pdMS_TO_TICKS(50));
    gpio_set_level(pin,0);
    vTaskDelay(pdMS_TO_TICKS(500));
    gpio_set_level(pin,1);
    vTaskDelay(pdMS_TO_TICKS(50));
    return;
}

void init_gpios_i2c(){
    gpio_reset_pin(I2C_SLAVE_01_PIN_INT);
    gpio_reset_pin(I2C_SLAVE_01_PIN_RESET);

    gpio_reset_pin(I2C_SLAVE_02_PIN_INT);
    gpio_reset_pin(I2C_SLAVE_02_PIN_RESET);
    
    gpio_set_direction(I2C_SLAVE_01_PIN_INT,GPIO_MODE_INPUT);
    gpio_set_direction(I2C_SLAVE_02_PIN_INT,GPIO_MODE_INPUT);

    gpio_set_direction(I2C_SLAVE_01_PIN_RESET, GPIO_MODE_OUTPUT);
    gpio_set_direction(I2C_SLAVE_02_PIN_RESET, GPIO_MODE_OUTPUT);

    gpio_set_level(I2C_SLAVE_01_PIN_RESET,1); /* default value*/
    gpio_set_level(I2C_SLAVE_02_PIN_RESET,1); /* default value*/
    vTaskDelay(pdMS_TO_TICKS(100));
}
