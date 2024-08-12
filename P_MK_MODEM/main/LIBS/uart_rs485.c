#include "uart_rs485.h"
#include "esp_log.h"

/*************************************************************
 *  DEFINES 
**************************************************************/
#define TAG "RS485"

#define DEBUG_RS485     0
#define ID_MODBUS_SEN     0xCC    // direccion de esclavo ICM 4


/*************************************************************
 *  VARIABLES
**************************************************************/
// Base de datos para checksum rs485
const uint16_t  CrcTable[] = {
        0X0000, 0XC0C1, 0XC181, 0X0140, 0XC301, 0X03C0, 0X0280, 0XC241,
        0XC601, 0X06C0, 0X0780, 0XC741, 0X0500, 0XC5C1, 0XC481, 0X0440,
        0XCC01, 0X0CC0, 0X0D80, 0XCD41, 0X0F00, 0XCFC1, 0XCE81, 0X0E40,
        0X0A00, 0XCAC1, 0XCB81, 0X0B40, 0XC901, 0X09C0, 0X0880, 0XC841,
        0XD801, 0X18C0, 0X1980, 0XD941, 0X1B00, 0XDBC1, 0XDA81, 0X1A40,
        0X1E00, 0XDEC1, 0XDF81, 0X1F40, 0XDD01, 0X1DC0, 0X1C80, 0XDC41,
        0X1400, 0XD4C1, 0XD581, 0X1540, 0XD701, 0X17C0, 0X1680, 0XD641,
        0XD201, 0X12C0, 0X1380, 0XD341, 0X1100, 0XD1C1, 0XD081, 0X1040,
        0XF001, 0X30C0, 0X3180, 0XF141, 0X3300, 0XF3C1, 0XF281, 0X3240,
        0X3600, 0XF6C1, 0XF781, 0X3740, 0XF501, 0X35C0, 0X3480, 0XF441,
        0X3C00, 0XFCC1, 0XFD81, 0X3D40, 0XFF01, 0X3FC0, 0X3E80, 0XFE41,
        0XFA01, 0X3AC0, 0X3B80, 0XFB41, 0X3900, 0XF9C1, 0XF881, 0X3840,
        0X2800, 0XE8C1, 0XE981, 0X2940, 0XEB01, 0X2BC0, 0X2A80, 0XEA41,
        0XEE01, 0X2EC0, 0X2F80, 0XEF41, 0X2D00, 0XEDC1, 0XEC81, 0X2C40,
        0XE401, 0X24C0, 0X2580, 0XE541, 0X2700, 0XE7C1, 0XE681, 0X2640,
        0X2200, 0XE2C1, 0XE381, 0X2340, 0XE101, 0X21C0, 0X2080, 0XE041,
        0XA001, 0X60C0, 0X6180, 0XA141, 0X6300, 0XA3C1, 0XA281, 0X6240,
        0X6600, 0XA6C1, 0XA781, 0X6740, 0XA501, 0X65C0, 0X6480, 0XA441,
        0X6C00, 0XACC1, 0XAD81, 0X6D40, 0XAF01, 0X6FC0, 0X6E80, 0XAE41,
        0XAA01, 0X6AC0, 0X6B80, 0XAB41, 0X6900, 0XA9C1, 0XA881, 0X6840,
        0X7800, 0XB8C1, 0XB981, 0X7940, 0XBB01, 0X7BC0, 0X7A80, 0XBA41,
        0XBE01, 0X7EC0, 0X7F80, 0XBF41, 0X7D00, 0XBDC1, 0XBC81, 0X7C40,
        0XB401, 0X74C0, 0X7580, 0XB541, 0X7700, 0XB7C1, 0XB681, 0X7640,
        0X7200, 0XB2C1, 0XB381, 0X7340, 0XB101, 0X71C0, 0X7080, 0XB041,
        0X5000, 0X90C1, 0X9181, 0X5140, 0X9301, 0X53C0, 0X5280, 0X9241,
        0X9601, 0X56C0, 0X5780, 0X9741, 0X5500, 0X95C1, 0X9481, 0X5440,
        0X9C01, 0X5CC0, 0X5D80, 0X9D41, 0X5F00, 0X9FC1, 0X9E81, 0X5E40,
        0X5A00, 0X9AC1, 0X9B81, 0X5B40, 0X9901, 0X59C0, 0X5880, 0X9841,
        0X8801, 0X48C0, 0X4980, 0X8941, 0X4B00, 0X8BC1, 0X8A81, 0X4A40,
        0X4E00, 0X8EC1, 0X8F81, 0X4F40, 0X8D01, 0X4DC0, 0X4C80, 0X8C41,
        0X4400, 0X84C1, 0X8581, 0X4540, 0X8701, 0X47C0, 0X4680, 0X8641,
        0X8201, 0X42C0, 0X4380, 0X8341, 0X4100, 0X81C1, 0X8081, 0X4040 };


/*************************************************************
 * AUXILIAR FUNCTIONS
**************************************************************/


/** 
    @brief Genera el checksum del protocolo rs485 - en base al
            array de elementos 0xXX; /data_b y de tamaño /size_b
            y guarda esos valores en /result  
 */
void rs485_update_checksum(uint8_t *data_b, int size_b, uint8_t *result){
    uint16_t crc = 0xFFFF;
    uint8_t temp_data;
    for(int i=0; i<size_b ; i++) {
        temp_data = data_b[i];
        int pos = (crc ^ temp_data) & 0xFF;
        crc = (crc >> 8)^CrcTable[pos];
    }
    uint8_t low_cs = (uint8_t)(crc%256);
    uint8_t high_cs= (uint8_t)(crc/256);

    result[0] = low_cs;
    result[1] = high_cs;

    return;
}

uint16_t byte_to_uint16_rs485(uint8_t *bytes_t){
    if(bytes_t == NULL) {
        return -1;
    }
    uint16_t valor_actual = (uint16_t)((bytes_t[0] << 8) | bytes_t[1]);
    return valor_actual;
}

void copy_bytes(const uint8_t* source, uint8_t* destiny, size_t len){
    int j = 0;
    for(j = 0; j < (int) len; j++){
        destiny[j] = source [j];
    }
    return;
}

/*************************************************************
 * MAIN FUNCTIONS
**************************************************************/

/** 
    @brief  Configuramos los pines del ESP para comunicarse con el
            modulo RS485 (baudrate, serial port number, parity char)
 */
esp_err_t rs485_config(RS485_UART_T rs485_uart){
    // GPIO RS485 CONFIGURATION
    uart_config_t uart_config_rs485 = {
        .baud_rate = rs485_uart.baud_rate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 122,
        .source_clk = UART_SCLK_APB,
    };


    // Install UART driver (we don't need an event queue here)
    ESP_ERROR_CHECK(uart_driver_install(rs485_uart.uart_num, BUF_SIZE_RS485 * 2, 0, 20, NULL, 0));
    // Configure UART parameters
    ESP_ERROR_CHECK(uart_param_config(rs485_uart.uart_num, &uart_config_rs485));
    // Set UART pins as per KConfig settings
    ESP_ERROR_CHECK(uart_set_pin(rs485_uart.uart_num, rs485_uart.gpio_uart_tx, rs485_uart.gpio_uart_rx, rs485_uart.gpio_uart_rts,  rs485_uart.gpio_uart_cts));
    // Set RS485 half duplex mode
    ESP_ERROR_CHECK(uart_set_mode(rs485_uart.uart_num, UART_MODE_RS485_HALF_DUPLEX));
    // Set read timeout of UART TOUT feature
    ESP_ERROR_CHECK(uart_set_rx_timeout(rs485_uart.uart_num, ECHO_READ_TOUT));
    
    return ESP_OK;
}


esp_err_t rs485_deinit(RS485_UART_T rs485_uart) {
    // Desinstala el controlador UART
    uart_driver_delete(rs485_uart.uart_num);
    
    // Configura los pines GPIO utilizados por la UART como entradas
    gpio_reset_pin(rs485_uart.gpio_uart_tx);
    gpio_reset_pin(rs485_uart.gpio_uart_rx);
    gpio_reset_pin(rs485_uart.gpio_uart_cts);

    gpio_set_direction(rs485_uart.gpio_uart_tx, GPIO_MODE_INPUT);
    gpio_set_direction(rs485_uart.gpio_uart_rx, GPIO_MODE_INPUT);
    gpio_set_direction(rs485_uart.gpio_uart_cts, GPIO_MODE_INPUT);

    return ESP_OK;
}



int rs485_send_command(uart_port_t uart_num,uint8_t *command, int length, bool enable_checksum, uint8_t *response_rs485){
    uint8_t check_sum[2];
    uint64_t _delay_max = (uint64_t)(600 * 1000); // us

    rs485_update_checksum(command,length,check_sum);
    uart_flush(uart_num);
    vTaskDelay(pdMS_TO_TICKS(50));

#if DEBUG_RS485
    printf("Send: ");
    for(int i = 0; i < length; i++){
        printf("0x%02x ",command[i]);
    }
    for(int i = 0; i<2; i++){
        printf("0x%02x ",check_sum[i]);
    }
    printf("\n");
#endif  

    uart_write_bytes(uart_num, command, length);
    uart_write_bytes(uart_num, check_sum, 2);
    
    uart_wait_tx_done(uart_num, pdMS_TO_TICKS(10));
    vTaskDelay(pdMS_TO_TICKS(10));

    int length_buffer = 0;
    uint64_t actual_time = esp_timer_get_time();
    uint64_t limit_time = actual_time + _delay_max;
    while((length_buffer < length) && (actual_time < limit_time)){   
        uart_get_buffered_data_len(uart_num,(size_t*)&length_buffer);
        vTaskDelay(pdMS_TO_TICKS(10));
        actual_time = esp_timer_get_time();
    }
    
    if(actual_time >= limit_time){
        // ESP_LOGE("RS485","Module, not response");
        return -1;    
    }

    int lrs = uart_read_bytes(uart_num,response_rs485, BUF_SIZE_RS485,PACKET_READ_TICS);

#if DEBUG_RS485
    if(lrs > 0) {
        printf("Reciv: ");
        for(int i = 0; i < lrs; i++) {
            printf("0x%02X ", response_rs485[i]);
        }
        printf("\n");
    } 
#endif

    if(!enable_checksum){
        return -2;
    }

    uint8_t temp_response[lrs-2];
    copy_bytes(response_rs485,temp_response,lrs-2);
    rs485_update_checksum(temp_response,lrs-2,check_sum);
    if((check_sum[0] == response_rs485[lrs-2]) && (check_sum[1] == response_rs485[lrs-1])){;
        return 0;
    }
    return 1;
}



/**
    @brief Obtenemos datos despues de comunicarnos con el sensor
    @param sensor_id
**/

int rs485_get_sens_ndata(uart_port_t uart_num,uint8_t sensor_id, uint16_t data_i, uint16_t data_n, uint8_t* buffer, uint16_t* output){
    uint8_t input_rs485[6];
    // i.e. : <0xCC> <0x04> <0x00> <0x00> <0x00> <0x7D>
    input_rs485[0] = (uint8_t)sensor_id;       
    input_rs485[1] = 0x03;                  /** TODO: Indicamos modeo de lectura  0x04 o 0x03 */ 
    input_rs485[2] = (data_i >> 8) & 0xFF;  // start register high
    input_rs485[3] = data_i & 0xFF;         // start register low
    input_rs485[4] = (data_n >> 8) & 0xFF;  // number of registers high
    input_rs485[5] = data_n & 0xFF;         // number of registers low

    int ret = rs485_send_command(uart_num,input_rs485, 6, true, buffer);
    // ESP_LOGW("SEND","ret: %d",ret);
    if(ret < 0){ 
        return ret;
    }
    // 0 o 1
    int n_response = (int)buffer[2]/2;
    uint8_t data_leida[2];
    data_leida[0] = 0; data_leida[1]=0;
    for (int i = 0; i< n_response; i++){
        data_leida[0] = buffer[i*2+3];
        data_leida[1] = buffer[i*2+4];
        output[i] = byte_to_uint16_rs485(data_leida);
    }
    return ret;
}




/*************************************************************
 *  MODBUS AUX FUNTIONS
**************************************************************/
int icm_read_particles(uart_port_t uart_num, uint16_t output[8]) {
    // read registers 56 to 63 (8)
    uint8_t  sensor_id = ID_MODBUS_SEN;     // ID del sensor (204)
    uint16_t addr_start = 56;               
    uint16_t n_addr = 8;                    // of register to read (56-63)
    
    uint8_t buffer[BUF_SIZE_RS485];         // Buffer to save answer to sensor
    uint16_t reg_data[8]={0};               // Array to data read of sensor
    
    int ret = rs485_get_sens_ndata(uart_num, sensor_id, addr_start, n_addr, buffer, reg_data);
    if (ret >= 0) {
        ESP_LOGI(TAG,"Successful read of registers 56-63");
        for (int i = 0; i < 8; i++) {
            printf("addr: %d-> %d\n", addr_start + i, reg_data[i]);
            
            // Convert signed to unsigned
            output[i] = (uint16_t)reg_data[i];
        }
    } else {
        ESP_LOGE(TAG, "Error: %d while reading registers 56-63", ret);
    }
    return ret;
}

int icm_read_temp_hum(uart_port_t uart_num,float *temp, float *hum) {
    // read registers 33 and 34 (8)
    uint8_t  sensor_id = ID_MODBUS_SEN;   // ID del sensor
    uint16_t addr_start = 33;           
    uint16_t n_addr = 2;                // # of register to read (33-34)
    
    uint8_t buffer[BUF_SIZE_RS485];     // Buffer to save answer to sensor
    uint16_t reg_data[2]={0};               // Array to data read of sensor
    
    int ret = rs485_get_sens_ndata(uart_num, sensor_id, addr_start, n_addr, buffer, reg_data);
    if (ret >= 0) {
        ESP_LOGI(TAG,"Successful read of registers 33-34");
        for (int i = 0; i <n_addr; i++) {
            printf("addr: %d-> %d\n", addr_start + i, reg_data[i]);
        }
        *temp=(float)reg_data[0]/100; // div factor 100
        *hum =(float)reg_data[1]/100; // div factor 100
    } else {
        ESP_LOGE(TAG, "Error: %d while reading registers 33-34", ret);
    }
    return ret;
}

/*---------------------------------------------------------------*/
int icm_read_msn_data(uart_port_t uart_num, uint32_t* msn_result) {
    // read registers 4-5
    const uint8_t  sensor_id = ID_MODBUS_SEN;           // ID_MODBUS_SEN; 
    const uint16_t addr_start = 4;                      // ADDR START
    const uint16_t n_addr = 2;                          // NUMBER OF ADDRS to read (33-34)
    
    uint8_t buffer[BUF_SIZE_RS485];             // Buffer to save answer to sensor
    uint16_t reg_data[5]={0};                   // Array to data read of sensor
    
    int ret = rs485_get_sens_ndata(uart_num, sensor_id, addr_start, n_addr, buffer, reg_data);
    if (ret >= 0) {
        ESP_LOGI(TAG,"Successful read of registers %u-%u",addr_start, addr_start+n_addr);
        ESP_LOG_BUFFER_HEX(TAG,reg_data,n_addr*2);
        *msn_result = ((uint32_t)reg_data[0] << 16) | (uint32_t)reg_data[1]; //4-5
    } else {
        ESP_LOGE(TAG, "Error: %d while reading registers %u-%u", ret,addr_start, addr_start+n_addr);
    }
    return ret;
}

int icm_read_test_data(uart_port_t uart_num, icm_tests_t* result) {
    // read registers 4-5
    const uint8_t  sensor_id = ID_MODBUS_SEN;           // ID_MODBUS_SEN; 
    const uint16_t addr_start = 8;                      // ADDR START
    const uint16_t n_addr = 18;                       // NUMBER OF ADDRS to read (33-34)
    
    uint8_t buffer[BUF_SIZE_RS485];             // Buffer to save answer to sensor
    uint16_t reg_data[20]={0};                   // Array to data read of sensor
    /**
     * 0-1 -> 8-9   test num 
     * 2-9 -> 10-17 test ref
     * 10  -> 18    test duration
     * 11  -> 19    test format
     * 12  -> 20    test mode
     * 13  -> 21    command
     * 14-15 -> 22-23 test interval
     * 16-17 -> 24-25 date time
     */
    int ret = rs485_get_sens_ndata(uart_num, sensor_id, addr_start, n_addr, buffer, reg_data);
    if (ret >= 0) {
        ESP_LOGI(TAG,"Successful read of registers %u-%u",addr_start, addr_start+n_addr);
        ESP_LOG_BUFFER_HEX(TAG,reg_data,n_addr*2);
        
        result->tst_number = ((uint32_t)reg_data[0] << 16) | (uint32_t)reg_data[1]; //8-9
        for (int i = 0; i < 8; i++) {                                               //[10-17]
             result->tst_reference[2*i] = (reg_data[2+i] >> 8)&0xFF;                // byte superior
             result->tst_reference[2*i + 1] = reg_data[2+i] & 0xFF;                 // byte  
        }
        result->tst_duration = reg_data[10];
        result->tst_format = reg_data[11];
        result->tst_mode  = reg_data[12];
        result->command = reg_data[13];
        result->tst_interval = ((uint32_t)reg_data[14] << 16) | (uint32_t)reg_data[15];
        result->date_time = ((uint32_t)reg_data[16] << 16) | (uint32_t)reg_data[17];
    } else {
        ESP_LOGE(TAG, "Error: %d while reading registers %u-%u", ret,addr_start, addr_start+n_addr);
    }
    return ret;
}

int icm_read_sens_data(uart_port_t uart_num, icm_sens_t* result){
    const uint8_t  sensor_id = ID_MODBUS_SEN;           // ID_MODBUS_SEN; 
    const uint16_t addr_start = 33;                      // ADDR START
    const uint16_t n_addr = 31;                       // NUMBER OF ADDRS to read (33-34)
    
    uint8_t buffer[BUF_SIZE_RS485];             // Buffer to save answer to sensor
    uint16_t reg_data[35]={0};                   // Array to data read of sensor
    /**
     * 0   -> 33      temp
     * 1   -> 34      rh
     * 4   -> 37      flow_ind
     * 7   -> 40-55   part_count
     * 23  -> 56-63   res_code
     */
    int ret = rs485_get_sens_ndata(uart_num, sensor_id, addr_start, n_addr, buffer, reg_data);
    if (ret >= 0) {
        ESP_LOGI(TAG,"Successful read of registers %u-%u",addr_start, addr_start+n_addr);
        ESP_LOG_BUFFER_HEX(TAG,reg_data,n_addr*2);
        
        result->temp = reg_data[0];
        result->rh = reg_data[1];
        result->flow_ind = reg_data[4];
        for (int i = 0; i < 8; i++) {
            result->part_count[i] = ((uint32_t)reg_data[7 + i*2] << 16) | (uint32_t)reg_data[8 + i*2]; //(40-41, 42-43, ..., 54-55)
        }
        for (int i = 0; i < 8; i++) {
            result->res_code[i] = reg_data[23+i];
        }
    } else {
        ESP_LOGE(TAG, "Error: %d while reading registers %u-%u", ret,addr_start, addr_start+n_addr);
    }
    return ret;
}

int icm_read_calib_data(uart_port_t uart_num, icm_calib_t* result) {
    // read registers 119-120
    const uint8_t  sensor_id = ID_MODBUS_SEN;           // ID_MODBUS_SEN; 
    const uint16_t addr_start = 117;                      // ADDR START
    const uint16_t n_addr = 4;                          // NUMBER OF ADDRS to read (33-34)
    
    uint8_t buffer[BUF_SIZE_RS485];             // Buffer to save answer to sensor
    uint16_t reg_data[5]={0};                   // Array to data read of sensor
    
    int ret = rs485_get_sens_ndata(uart_num, sensor_id, addr_start, n_addr, buffer, reg_data);
    if (ret >= 0) {
        ESP_LOGI(TAG,"Successful read of registers %u-%u",addr_start, addr_start+n_addr);
        ESP_LOG_BUFFER_HEX(TAG,reg_data,n_addr*2);
        result->calib_d = ((uint32_t)reg_data[0] << 16) | (uint32_t)reg_data[1]; //4-5
        result->calib_l = ((uint32_t)reg_data[2] << 16) | (uint32_t)reg_data[3]; //
    } else {
        ESP_LOGE(TAG, "Error: %d while reading registers %u-%u", ret,addr_start, addr_start+n_addr);
    }
    return ret;
}

/*****************************************************************************
 * EXTRA FUNCTIONS TO GET SENSOR DATA
 *****************************************************************************/

int icm_read_data(uart_port_t uart_num, icm_485_data_t* icm_data) {

    // read registers 20 - 60 (40)
    uint8_t  sensor_id = ID_MODBUS_SEN;         // ID_MODBUS_SEN; 
    uint16_t addr_start = 0x00;                 // ADDR START
    uint16_t n_addr = 65;                        // NUMBER OF ADDRS to read (33-34)
    
    uint8_t buffer[BUF_SIZE_RS485];             // Buffer to save answer to sensor
    uint16_t reg_data[70]={0};                  // Array to data read of sensor
    
    int ret = rs485_get_sens_ndata(uart_num, sensor_id, addr_start, n_addr, buffer, reg_data);    
    if (ret >= 0) {
        ESP_LOGI(TAG,"Successful read of registers %u-%u",addr_start, addr_start+n_addr);
        for (int i = 0; i <n_addr; i++) {
            printf("addr: %d-> %u\n", addr_start + i, reg_data[i]);
        }
        
        icm_data->msn = ((uint32_t)reg_data[4] << 16) | (uint32_t)reg_data[5]; //4-5
        icm_data->test_num = ((uint32_t)reg_data[8] << 16) | (uint32_t)reg_data[9]; //8-9
        for (int i = 0; i < 8; i++) {
             icm_data->test_ref[2*i] = (reg_data[10+i] >> 8)&0xFF; // byte superior
             icm_data->test_ref[2*i + 1] = reg_data[10+i] & 0xFF; // byte  
        }

        icm_data->date = ((uint32_t)reg_data[24] << 16) | (uint32_t)reg_data[25];//24-25          //(data to modem)
        icm_data->temp = reg_data[33];   // (33)
        icm_data->rh = reg_data[34];            // (34)
        icm_data->flow_ind = (uint16_t)reg_data[37];//(37)

        for (int i = 0; i < 8; i++) {
            icm_data->part_count[i] = ((uint32_t)reg_data[40 + i*2] << 16) | (uint32_t)reg_data[41 + i*2]; //(40-41, 42-43, ..., 54-55)
        }
         for (int i = 0; i < 8; i++) {
            icm_data->res_code[i] = reg_data[56+i];
        }
    } else {
        ESP_LOGE(TAG, "Error: %d while reading registers %u-%u", ret,addr_start, addr_start+n_addr);
    }
    return ret;
}