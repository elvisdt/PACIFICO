/*
 * SPDX-FileCopyrightText: 2021-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */



/****************************************************************************
*
* This demo showcases BLE GATT client. It can scan BLE devices and connect to one device.
* Run the gatt_server demo, the client demo will automatically connect to the gatt_server demo.
* Client demo will enable gatt_server's notify after connection. The two devices will then exchange
* data.
*
****************************************************************************/

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include "nvs.h"
#include "nvs_flash.h"

#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_gatt_defs.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

#include "gap.h"
#include "gatt_cli.h"
#include "ble_MK115B.h"

#define TAG "MAIN"

// static const uint8_t remote_addr_dev[] ={0xF0, 0XA3, 0xED, 0x3A, 0xE9, 0xFE};


void app_main(void)
{
    // Initialize NVS.
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        ESP_LOGE(TAG, "%s initialize controller failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) {
        ESP_LOGE(TAG, "%s enable controller failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_bluedroid_init();
    if (ret) {
        ESP_LOGE(TAG, "%s init bluetooth failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(TAG, "%s enable bluetooth failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    ret = init_gap_client();
    if (ret){
        ESP_LOGE(TAG, "%s gap register failed, error code = %x", __func__, ret);
        return;
    }

    ret =  init_gatt_client();
    if(ret){
        ESP_LOGE(TAG, "%s gattc register failed, error code = %x", __func__, ret);
        return;
    }

    ret = esp_ble_gattc_app_register(PROFILE_A_APP_ID);
    if (ret){
        ESP_LOGE(TAG, "%s gattc app register failed, error code = %x", __func__, ret);
    }
    esp_err_t local_mtu_ret = esp_ble_gatt_set_local_mtu(500);
    if (local_mtu_ret){
        ESP_LOGE(TAG, "set local  MTU failed, error code = %x", local_mtu_ret);
    }
    uint8_t last_state=0;
    uint16_t count =0;
    MK115_bc_data_t data_mk115 ={0};

    bool bt_scan_state = bt_gap_get_scann_state();
    bool bt_con_state =  bt_gap_get_conn_state();
    if (!bt_scan_state){
        ble_scanner_start();
        ESP_LOGI("MAIN", "START SCAN");
    }

    vTaskDelay(pdMS_TO_TICKS(5000));
    
    while (1){
        
        bt_scan_state = bt_gap_get_scann_state();
        bt_con_state =  bt_gap_get_conn_state();
        printf("\n");
        ESP_LOGE("MAIN", "STATUS, SCAN: %d, CONN: %d", bt_scan_state,bt_con_state);

        if (bt_scan_state==false && bt_con_state==false){
            vTaskDelay(pdMS_TO_TICKS(10000));
            ble_scanner_start();
            ESP_LOGI("MAIN", "START SCAN");
        }

        
        
        if (bt_con_state){
            vTaskDelay(pdMS_TO_TICKS(1000));
            last_state = !last_state;

            if(count<3){
                count ++;
                ESP_LOGE("MAIN", "check ON/OFF: ");
                uint8_t s_data[] = {0xB0, 0X03, 0x00};
                gatt_write_bt_data(BT_CHAR_UUID_READ_DEV_INFO,s_data, sizeof(s_data));
                vTaskDelay(pdMS_TO_TICKS(2000));
            }
            // uint8_t send_data[]={0xB2, 0X03, 0x01, last_state};
            // int bt_wr_state =  gatt_write_bt_data(BT_CHAR_UUID_SET_DEV_PARAM,send_data, sizeof(send_data));
            
            uint8_t send_data[] = {0xB0, 0X08, 0x00};
            int bt_wr_state =  gatt_write_bt_data(BT_CHAR_UUID_READ_DEV_INFO,send_data, sizeof(send_data));
            ESP_LOGW(TAG, "RET CHANGUE STATUS : %d\n", bt_wr_state);
            
            // break;
            vTaskDelay(pdMS_TO_TICKS(1000));
            uint8_t send_d1[] = {0xB0, 0X05, 0x00};
            bt_wr_state =  gatt_write_bt_data(BT_CHAR_UUID_READ_DEV_INFO,send_d1, sizeof(send_d1));
            ESP_LOGW(TAG, "RET CHANGUE STATUS : %d\n", bt_wr_state);
            vTaskDelay(pdMS_TO_TICKS(2000));

            mk115_get_copy_data(&data_mk115);
            print_MK115_bc_data(&data_mk115);
        }
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
