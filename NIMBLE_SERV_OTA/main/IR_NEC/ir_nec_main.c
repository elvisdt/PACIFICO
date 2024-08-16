/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "driver/rmt_tx.h"
#include "driver/rmt_rx.h"
#include "ir_nec_encoder.h"

#include "time.h"
#include "esp_timer.h"

#include "ir_nec_main.h"

/** ===================================================================
 *  DEFINITIONS
 * ====================================================================*/

#define TAG_IR  "IR-NEC"

/** ===================================================================
 *  FUNCTIONS
 * ====================================================================*/

/**
 * @brief Saving NEC decode results
 */
static uint16_t s_nec_code_address;
static uint16_t s_nec_code_command;

static bool active_scan_rep=false;

rmt_channel_handle_t rx_channel = NULL;
rmt_channel_handle_t tx_channel = NULL;

rmt_transmit_config_t transmit_config;
rmt_encoder_handle_t nec_encoder = NULL;

/**
 * @brief Check whether a duration is within expected range
 */
static inline bool nec_check_in_range(uint32_t signal_duration, uint32_t spec_duration)
{
    return (signal_duration < (spec_duration + EXAMPLE_IR_NEC_DECODE_MARGIN)) &&
           (signal_duration > (spec_duration - EXAMPLE_IR_NEC_DECODE_MARGIN));
}

/**
 * @brief Check whether a RMT symbol represents NEC logic zero
 */
static bool nec_parse_logic0(rmt_symbol_word_t *rmt_nec_symbols)
{
    return nec_check_in_range(rmt_nec_symbols->duration0, NEC_PAYLOAD_ZERO_DURATION_0) &&
           nec_check_in_range(rmt_nec_symbols->duration1, NEC_PAYLOAD_ZERO_DURATION_1);
}

/**
 * @brief Check whether a RMT symbol represents NEC logic one
 */
static bool nec_parse_logic1(rmt_symbol_word_t *rmt_nec_symbols)
{
    return nec_check_in_range(rmt_nec_symbols->duration0, NEC_PAYLOAD_ONE_DURATION_0) &&
           nec_check_in_range(rmt_nec_symbols->duration1, NEC_PAYLOAD_ONE_DURATION_1);
}

/**
 * @brief Decode RMT symbols into NEC address and command
 */
static bool nec_parse_frame(rmt_symbol_word_t *rmt_nec_symbols)
{
    rmt_symbol_word_t *cur = rmt_nec_symbols;
    uint16_t address = 0;
    uint16_t command = 0;
    bool valid_leading_code = nec_check_in_range(cur->duration0, NEC_LEADING_CODE_DURATION_0) &&
                              nec_check_in_range(cur->duration1, NEC_LEADING_CODE_DURATION_1);
    if (!valid_leading_code) {
        return false;
    }
    cur++;
    for (int i = 0; i < 16; i++) {
        if (nec_parse_logic1(cur)) {
            address |= 1 << i;
        } else if (nec_parse_logic0(cur)) {
            address &= ~(1 << i);
        } else {
            return false;
        }
        cur++;
    }
    for (int i = 0; i < 16; i++) {
        if (nec_parse_logic1(cur)) {
            command |= 1 << i;
        } else if (nec_parse_logic0(cur)) {
            command &= ~(1 << i);
        } else {
            return false;
        }
        cur++;
    }
    // save address and command
    s_nec_code_address = address;
    s_nec_code_command = command;
    return true;
}

/**
 * @brief Check whether the RMT symbols represent NEC repeat code
 */
static bool nec_parse_frame_repeat(rmt_symbol_word_t *rmt_nec_symbols)
{
    return nec_check_in_range(rmt_nec_symbols->duration0, NEC_REPEAT_CODE_DURATION_0) &&
           nec_check_in_range(rmt_nec_symbols->duration1, NEC_REPEAT_CODE_DURATION_1);
}




/**
 * @brief Decode RMT symbols into NEC scan code and print the result
 */
static int example_parse_nec_frame(rmt_symbol_word_t *rmt_nec_symbols, size_t symbol_num)
{
    static size_t last_symbol = 0;
    if ((last_symbol == symbol_num) && symbol_num == 2){
        return -1;
    }
    
    last_symbol = symbol_num;
    printf("NEC Parse frame \r\n");
    /*
    for (size_t i = 0; i < symbol_num; i++) {
        printf("{%d:%d},{%d:%d}\r\n", rmt_nec_symbols[i].level0, rmt_nec_symbols[i].duration0,
               rmt_nec_symbols[i].level1, rmt_nec_symbols[i].duration1);
    }
    */
    // printf("---NEC frame end: ");

    // decode RMT symbols
    switch (symbol_num) {


        case 34: // NEC normal frame
            if (nec_parse_frame(rmt_nec_symbols)) {
                printf("Address=%04X, Command=%04X\r\n\r\n", s_nec_code_address, s_nec_code_command);
            }
            return 1;

            break;
        case 2: // NEC repeat frame
            if (nec_parse_frame_repeat(rmt_nec_symbols) ) {
                printf("Address=%04X, Command=%04X, repeat \r\n\r\n", s_nec_code_address, s_nec_code_command);
            }

            return 2;
            break;
        default:
            printf("Unknown NEC frame\r\n\r\n");
            break;
        }

    return 0;
}

static bool example_rmt_rx_done_callback(rmt_channel_handle_t channel, const rmt_rx_done_event_data_t *edata, void *user_data)
{
    BaseType_t high_task_wakeup = pdFALSE;
    QueueHandle_t receive_queue = (QueueHandle_t)user_data;

    // send the received RMT symbols to the parser task
    xQueueSendFromISR(receive_queue, edata, &high_task_wakeup);
    return high_task_wakeup == pdTRUE;
}





static void ir_nec_task(void *param ){

    ESP_LOGI(TAG_IR, "create RMT RX channel");
    rmt_rx_channel_config_t rx_channel_cfg = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = EXAMPLE_IR_RESOLUTION_HZ,
        .mem_block_symbols = 64, // amount of RMT symbols that the channel can store at a time
        .gpio_num = EXAMPLE_IR_RX_GPIO_NUM,
    };

    // RX_CHANELL
    rx_channel = NULL;
    ESP_ERROR_CHECK(rmt_new_rx_channel(&rx_channel_cfg, &rx_channel));


    ESP_LOGI(TAG_IR, "register RX done callback");
    // EXTERN VARIABLE
    QueueHandle_t receive_queue_nec = xQueueCreate(5, sizeof(rmt_rx_done_event_data_t));
    assert(receive_queue_nec);

    rmt_rx_event_callbacks_t cbs = {
        .on_recv_done = example_rmt_rx_done_callback,
    };

    ESP_ERROR_CHECK(rmt_rx_register_event_callbacks(rx_channel, &cbs, receive_queue_nec));

    // the following timing requirement is based on NEC protocol
    rmt_receive_config_t receive_config = {
        .signal_range_min_ns = 1250,     // the shortest duration for NEC signal is 560us, 1250ns < 560us, valid signal won't be treated as noise
        .signal_range_max_ns = 12000000, // the longest duration for NEC signal is 9000us, 12000000ns > 9000us, the receive won't stop early
    };

    ESP_LOGI(TAG_IR, "create RMT TX channel");
    rmt_tx_channel_config_t tx_channel_cfg = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = EXAMPLE_IR_RESOLUTION_HZ,
        .mem_block_symbols = 64, // amount of RMT symbols that the channel can store at a time
        .trans_queue_depth = 4,  // number of transactions that allowed to pending in the background, this example won't queue multiple transactions, so queue depth > 1 is sufficient
        .gpio_num = EXAMPLE_IR_TX_GPIO_NUM,
    };
    
    // TX_CHANELL
    tx_channel = NULL;
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_channel_cfg, &tx_channel));


    ESP_LOGI(TAG_IR, "modulate carrier to TX channel");
    rmt_carrier_config_t carrier_cfg = {
        .duty_cycle = 0.33,
        .frequency_hz = 38000, // 38KHz
    };
    ESP_ERROR_CHECK(rmt_apply_carrier(tx_channel, &carrier_cfg));


    // this example won't send NEC frames in a loop
    transmit_config.loop_count = 0;



    ESP_LOGI(TAG_IR, "install IR NEC encoder");
    ir_nec_encoder_config_t nec_encoder_cfg = {
        .resolution = EXAMPLE_IR_RESOLUTION_HZ,
    };
    
    nec_encoder = NULL;
    ESP_ERROR_CHECK(rmt_new_ir_nec_encoder(&nec_encoder_cfg, &nec_encoder));


    ESP_LOGI(TAG_IR, "enable RMT TX and RX channels");
    ESP_ERROR_CHECK(rmt_enable(tx_channel));
    ESP_ERROR_CHECK(rmt_enable(rx_channel));

    // save the received RMT symbols
    rmt_symbol_word_t raw_symbols[64]; // 64 symbols should be sufficient for a standard NEC frame
    rmt_rx_done_event_data_t rx_data;

    // ready to receive
    ESP_ERROR_CHECK(rmt_receive(rx_channel, raw_symbols, sizeof(raw_symbols), &receive_config));

    while (1) {
        if (xQueueReceive(receive_queue_nec, &rx_data, pdMS_TO_TICKS(1000)) == pdPASS) {
            // parse the receive symbols and print the result
            int ret = example_parse_nec_frame(rx_data.received_symbols, rx_data.num_symbols);
            
            if (ret ==1 ){
                ir_nec_scan_t data_scan={0};
                data_scan.nec.address = s_nec_code_address;
                data_scan.nec.command = s_nec_code_command;
                data_scan.time = (uint32_t)time(NULL);
                ESP_LOGI("IR-NEC","SCANN ADDR:%04X, CMD:%04X, time:%lu ", 
                        data_scan.nec.address,
                        data_scan.nec.command,
                        data_scan.time);
                
                if (active_scan_rep && (data_scan.nec.address != 0x00)){
                    if (xQueueSend(ir_nec_quemain, &data_scan, pdMS_TO_TICKS(10)) != pdPASS){
                        ESP_LOGE("IR-NEC","FAIL SEND");
                    };
                }
                
                
            }

            // start receive again
            ESP_ERROR_CHECK(rmt_receive(rx_channel, raw_symbols, sizeof(raw_symbols), &receive_config));
        
        }

    }
    vTaskDelete(NULL);
}


int init_ir_nec_task(){

    ESP_LOGI(TAG_IR, "=== INIT IR TASK ===");
    TaskHandle_t xHandle = NULL;
    xTaskCreate(ir_nec_task, "ir_task", 1024 * 4, NULL, 11, &xHandle);
    configASSERT( xHandle );

    if( xHandle == NULL ){
        ESP_LOGE(TAG_IR,"FAIL INIT TASK IR");
        return -1;
    }

    return 0; 
}


//=================================================//
int nec_sed_data_ctrl(ir_nec_scan_code_t nec_code_data){
    int ret = 0;
    ret = rmt_transmit(tx_channel, nec_encoder, &nec_code_data, sizeof(nec_code_data), &transmit_config);

    ESP_LOGW("IR","send data addr:%04X , cmd: %04X", nec_code_data.address, nec_code_data.command);
    if (ret!=0)
    {
        ESP_LOGE("IR","FAIL SEND TX, ret=%d", ret);
    }
    
    return ret;
}



void set_scan_ir_rep(bool mode){
    active_scan_rep = mode;
    return;
}