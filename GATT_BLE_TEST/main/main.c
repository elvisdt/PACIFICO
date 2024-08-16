/*
 * SPDX-FileCopyrightText: 2021-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

/****************************************************************************
*
* This demo showcases BLE GATT server. It can send adv data, be connected by client.
* Run the gatt_client demo, the client demo will automatically connect to the gatt_server demo.
* Client demo will enable gatt_server's notify after connection. The two devices will then exchange
* data.
*
****************************************************************************/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_bt.h"

#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gatt_common_api.h"

#include "sdkconfig.h"
#include "ble_main.h"
#include "main.h"

#define TAG "MAIN"

#define NAMESPACE_NVS    "storage"
#define KEY_CTRL_LIST    "ctrl-list"



nvs_handle_t storage_nvs_handle;

static list_ctrl_t ctrl_nec_list={0};
size_t ctrl_list_size = sizeof(list_ctrl_t);


int m_get_data_ctrl(uint8_t *data, size_t *len) {
    *len = ctrl_nec_list.total * sizeof(ctrl_btn_t);
    if (*len > NUM_MAX_CTRL * sizeof(ctrl_btn_t)) {
        *len = NUM_MAX_CTRL * sizeof(ctrl_btn_t);
    }
    
    memcpy(data, ctrl_nec_list.list, *len);
    return 0; 
}


void Init_NVS_Keys()
{
    /*------------------------------------------------*/
    ESP_LOGI(TAG, "Init NVS keys of data");

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    nvs_open(NAMESPACE_NVS,NVS_READWRITE,&storage_nvs_handle);
    err = nvs_get_blob(storage_nvs_handle, KEY_CTRL_LIST, &ctrl_nec_list, &ctrl_list_size);
    if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        nvs_set_blob(storage_nvs_handle, KEY_CTRL_LIST, &ctrl_nec_list, sizeof(ctrl_nec_list));
    }

    // print all chrars
    for (size_t i = 0; i < ctrl_nec_list.total; i++)
    {
        ESP_LOGI("NVS","ctrl[%d]-> addr:%04X, cmd:%04X",i+1,ctrl_nec_list.list[i].addr, ctrl_nec_list.list[i].cmd);
    }
    
    ESP_LOGI(TAG, "NVS keys recovered succsesfull\r\n");
    return;
}



void app_main(void)
{
    ESP_LOGI(TAG, "Init NVS keys of data");
    uint8_t data[NUM_MAX_CTRL * sizeof(ctrl_btn_t)];
    size_t len=0;

    


    Init_NVS_Keys();
    init_ble_serv_main();
    uint16_t cont = 0;
    while (1)
    {
       if (ble_get_gatt_conn()){
            cont ++;
            vTaskDelay(pdMS_TO_TICKS(5000));
           
           /* 
            char data_char[20];
            sprintf(data_char,"HOLA : %d",cont);
            int  ret1 = send_data_perfile(0, (uint8_t *)data_char, strlen(data_char));
            ESP_LOGI(TAG ,"RET: %d, char: %s",ret1, data_char);
            */
            m_get_data_ctrl(data, &len);
            printf("Data length: %zu\n", len);
            ESP_LOG_BUFFER_HEX(TAG,data,len);
       }


        

       vTaskDelay(pdMS_TO_TICKS(5000));
    }
    

}









