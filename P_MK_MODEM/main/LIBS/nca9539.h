/*
 * nca9539.h
 *
 *  Created on: 4 jul. 2024
 *      Author: Elvis de la torre
 */

#ifndef _NCA9539_H_
#define _NCA9539_H_

#include "freertos/FreeRTOS.h"
#include "driver/i2c.h"
//#include "driver/i2c_master.h"
#include "esp_log.h"

/*****************************************************************
 * DEFINITIONS
 *****************************************************************/
/* DEIFINTIONS SLAVE PARAMETERS*/
#define I2C_SLAVE_01_ADDR   0x74
#define I2C_SLAVE_02_ADDR   0x76

#define I2C_SLAVE_01_PIN_RESET  GPIO_NUM_11
#define I2C_SLAVE_02_PIN_RESET  GPIO_NUM_10

#define I2C_SLAVE_01_PIN_INT    GPIO_NUM_12
#define I2C_SLAVE_02_PIN_INT    GPIO_NUM_13

/* DEFINITIONS MASTER PARAMETRES*/

#define I2C_MASTER_SCL_IO GPIO_NUM_14    /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO GPIO_NUM_21    /*!< gpio number for I2C master data  */

#define I2C_MASTER_NUM I2C_NUM_0 /*!< I2C port number for master dev */
#define I2C_MASTER_FREQ_HZ 100000 /*!< I2C master clock frequency */
#define I2C_MASTER_TX_BUF_DISABLE 0 /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE 0 /*!< I2C master doesn't need buffer */
#define ACK_CHECK_EN 0x1     /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS 0x0    /*!< I2C master will not check ack from slave */
#define ACK_VAL 0x0          /*!< I2C ack value */
#define NACK_VAL 0x1         /*!< I2C nack value */





// PCA9555 defines
#define NXP_INPUT      0x00
#define NXP_OUTPUT     0x02
#define NXP_INVERT     0x04
#define NXP_CONFIG     0x06
/*****************************************************************
 * STRUCTURES
 *****************************************************************/

// Enum with names of ports ED0 - ED15
typedef enum {
    A0, A1, A2, A3, A4, A5, A6, A7,
    B0, B1, B2, B3, B4, B5, B6, B7
}nca9539_pin_t;



typedef struct {
    uint8_t address;
    union {
        struct {
            uint8_t cfgreg_low;    // low order byte
            uint8_t cfgreg_high;   // high order byte
        };
        uint16_t cfg_register;           // 16 bits presentation
    };
    union {
        struct {
            uint8_t valreg_low;            // low order byte
            uint8_t valreg_high;           // high order byte
        };
        uint16_t value_register;
    };
    int error;
}NCA9539_I2C;

/*****************************************************************
 * HEADER FUCNTIONS
 *****************************************************************/

NCA9539_I2C * Nca539_i2c_init(uint8_t address);
void Nca9539_set_pin_mode(NCA9539_I2C* dev, uint8_t pin, uint8_t IOMode);
void Nca9539_set_block_mode(NCA9539_I2C* dev, uint16_t IOMode);
uint8_t Nca9539_read_pin(NCA9539_I2C* dev, uint8_t pin);
uint16_t Nca9539_read_block(NCA9539_I2C* dev);
void Nca9539_write_pin(NCA9539_I2C* dev, uint8_t pin, uint8_t value);
void Nca9539_write_block(NCA9539_I2C* dev, uint16_t value);

void Nca9539_reset(int pin);
void init_gpios_i2c();

#endif /* NCA9539_H_ */
