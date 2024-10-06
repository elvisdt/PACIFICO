/*
 * gatt_cli.h
 *
 *  Created on: 4 jul. 2024
 *      Author: Elvis de la torre
 */
#ifndef _BLE_GATTC_MULTI_CON_H_
#define _BLE_GATTC_MULTI_CON_H_


/**==================================================
 * INCLUDES
 * =================================================== */


#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_gatt_defs.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"
#include "freertos/FreeRTOS.h"

/**==================================================================
 * DEFINITIONS
 * ================================================================== */

#define BT_TIME_SACAN_INTERVAL      0X100
#define BT_TIME_SACAN_WINDOWN       0X50
#define BT_TIME_SCAN_DURATION       30      // SEG


#define REMOTE_SERVICE_UUID_01     0xFFB0
#define REMOTE_SERVICE_UUID_02     0xAA00


#define BT_A_CHAR_UUID_READ_DEV_INFO      0xFFB0 //0xFFB0 
#define BT_A_CHAR_UUID_SET_DEV_PARAM      0xFFB1 //0xFFB1 
#define BT_A_CHAR_UUID_DEV_REP_INFO       0xFFB2 //0xFFB2


#define BT_B_CHAR_UUID_OTA_CTRL      0xAA01 //0xAA01 
#define BT_B_CHAR_UUID_OTA_DATA      0xAA02 //0xAA02 
#define BT_B_CHAR_UUID_NEC_INFO      0xAA03 //0xAA03




// NUm  max off UUID char 
#define MAX_CHAR_COUNT      5

/* register three profiles, each profile corresponds to one connection,
   which makes it easy to handle each connection event */
#define PROFILE_NUM 2

#define PROFILE_A_APP_ID 0
#define PROFILE_B_APP_ID 1

#define INVALID_HANDLE   0

/**==================================================================
 * STRUCTURES
 * ================================================================== */

/**==================================================================
 * EXTERNAL VARIABLES
 * ================================================================== */


/**==================================================================
 * HEADER FUNCTIONS
 * ================================================================== */



/// @brief Aux function to print char
/// @param uuid 
/// @return char to uuid
char* esp_log_uuid(esp_bt_uuid_t *uuid);

/// @brief init scan function 
/// @param  
void bt_start_scan(void);


int gatt_write_A_data( uint16_t uuid_char16, uint8_t *data, size_t length);
int gatt_write_B_data( uint16_t uuid_char16, uint8_t *data, size_t length);

bool bt_st_conn_dev_a();
bool bt_st_conn_dev_b();

bool bt_get_serv_dev_a();
bool bt_get_serv_dev_b();

/**==================================================================
 * MAIN FUNC
 * ================================================================== */
esp_err_t init_bt_gattc_app(void);






#endif /*_BLE_GATTC_MULTI_CON_H_*/