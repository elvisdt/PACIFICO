/*
 * gatt_cli.h
 *
 *  Created on: 4 jul. 2024
 *      Author: Elvis de la torre
 */
#ifndef _BLE_GATT_CLI_H_
#define _BLE_GATT_CLI_H_


#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_gatt_defs.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"


#include "gap.h"

#define BT_SERVICE_UUID             0xFFB0 //0x00FF


#define MAX_CHAR_COUNT      5

#define BT_CHAR_UUID_READ_DEV_INFO      0xFFB0 //0xFFB0 
#define BT_CHAR_UUID_SET_DEV_PARAM      0xFFB1 //0xFFB1 
#define BT_CHAR_UUID_DEV_REP_INFO       0xFFB2 //0xFFB2



#define PROFILE_NUM      1

#define PROFILE_A_APP_ID 0

#define INVALID_HANDLE   0


struct gattc_profile_inst {
    esp_gattc_cb_t gattc_cb;
    uint16_t gattc_if;
    uint16_t app_id;
    uint16_t conn_id;
    uint16_t service_start_handle;
    uint16_t service_end_handle;
    uint16_t char_handle;
    esp_bd_addr_t remote_bda;
};

struct gatt_chars_info{
    uint16_t gattc_if;
    uint16_t conn_id;
    uint16_t char_handle;
    uint16_t char_uuid_16;
    uint8_t  status;
};


extern struct gattc_profile_inst gl_profile_tab[PROFILE_NUM];

extern struct gatt_chars_info gl_chars_info[MAX_CHAR_COUNT];



int gatt_write_bt_data( uint16_t uuid_char16, uint8_t *data, size_t length);

esp_err_t init_gatt_client();


#endif