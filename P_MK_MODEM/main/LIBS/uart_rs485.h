#ifndef __RS485_ESP32_
//--------------------------------------------------
#define __RS485_ESP32_
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include "esp_timer.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_err.h"

/*************************************************************
 *  DEFINES
 **************************************************************/

// CTS is not used in RS485 Half-Duplex Mode
// RTS for RS485 Half-Duplex Mode manages DE/~RE
// RS485_RTS

// Timeout threshold for UART = number of symbols (~10 tics) with unchanged state on receive pin
#define ECHO_READ_TOUT (3) // 3.5T * 8 = 28 ticks, TOUT=3 -> ~24..33 ticks
#define PACKET_READ_TICS (pdMS_TO_TICKS(100))

#define BUF_SIZE_RS485 1024 // 100
/*************************************************************
 *  STRUCTURES
 **************************************************************/
/**
 * @brief Estrucutra para configurar los perifericos del rs485
 */
typedef struct
{
    gpio_num_t gpio_uart_tx;  /*!< Pin de transmisión UART */
    gpio_num_t gpio_uart_rx;  /*!< Pin de recepción UART */
    gpio_num_t gpio_uart_rts; /*!< Pin de recepción UART */
    gpio_num_t gpio_uart_cts; /*!< Pin de recepción UART */
    uart_port_t uart_num;     /*!< Número de puerto UART */
    int baud_rate;            /*!< Velocidad en baudios de UART */
} RS485_UART_T;

/**
 * @brief Estrucutra de data de lectura
 */
typedef struct
{
    uint32_t msn;            /*addr 4-5*/
    uint32_t test_num;       /*addr 8-9*/
    uint8_t  test_ref[16];   /*addr 10-17*/
    uint32_t date;           /*addr 24-25*/
    int16_t  temp;           /*addr 33 */
    int16_t  rh;             /*addr 34*/
    uint16_t flow_ind;       /*addr 37*/
    uint32_t part_count[8];  /*addr 40-55*/
    uint16_t res_code[8];    /*addr 56-64*/
    uint32_t calib_d;        /*addr 117-118*/
    uint32_t calib_l;        /*addr 119-120*/
} icm_485_data_t;


/**
 * @brief Estrucutra de data tests
 */
typedef struct
{
    uint32_t  tst_number;           /*addr 8-9*/
    uint8_t   tst_reference[16];    /*addr 10-17*/
    uint16_t  tst_duration;         /*addr 18*/
    uint16_t  tst_format;           /*addr 19*/
    uint16_t  tst_mode;             /*addr 20*/
    uint16_t  command;              /*addr 21*/
    uint32_t  tst_interval;         /*addr 22-23*/
    uint32_t  date_time;            /*addr 24-25*/
} icm_tests_t;


/**
 * @brief Estrucutra de data sensors
 */
typedef struct
{
    uint16_t  temp;             /*addr 33*/
    uint16_t  rh;               /*addr 34*/
    uint16_t  flow_ind;         /*addr 37*/
    uint32_t  part_count[8];    /*addr 40-55*/
    uint16_t  res_code[8];      /*addr 56-63*/
} icm_sens_t;

/**
 * @brief Estrucutra de data sensors
 */
typedef struct
{
    uint32_t calib_d;        /*addr 117-118*/
    uint32_t calib_l;        /*addr 119-120*/
} icm_calib_t;

/*************************************************************
 *  VARIABLES
 **************************************************************/
// extern RS485_UART_T rs485_uart;

/*************************************************************
 *  FUNCTIONS
 **************************************************************/

esp_err_t rs485_config(RS485_UART_T rs485_uart);
esp_err_t rs485_deinit(RS485_UART_T rs485_uart);

void rs485_update_checksum(uint8_t data_b[], int size_b, uint8_t *result);
uint16_t byte_to_uint_rs485(uint8_t *bytes_t);

int rs485_send_command(uart_port_t uart_num,uint8_t *command, int length, bool enable_checksum, uint8_t *response_rs485);
int rs485_get_sens_ndata(uart_port_t uart_num, uint8_t sensor_id, uint16_t data_i, uint16_t data_n, uint8_t *buffer, uint16_t *output);

//--------------------------------------------------
int icm_read_temp_hum(uart_port_t uart_num, float *temp, float *hum);
int icm_read_particles(uart_port_t uart_num, uint16_t output[8]);

int icm_read_msn_data(uart_port_t uart_num, uint32_t* msn_result);
int icm_read_test_data(uart_port_t uart_num, icm_tests_t* result);
int icm_read_sens_data(uart_port_t uart_num, icm_sens_t* result);
int icm_read_calib_data(uart_port_t uart_num, icm_calib_t* result);


int icm_read_data(uart_port_t uart_num, icm_485_data_t *icm_data);

#endif // __RS485_ESP32_
