/*
 * nca9539.h
 *
 *  Created on: 4 jul. 2024
 *      Author: Elvis de la torre
 */
#ifndef _BLE_GAP_H_
#define _BLE_GAP_H_

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
#include "esp_log.h"
#include "freertos/FreeRTOS.h"


#define SCAN_DURATION   0x30// SEC



int bt_set_addr_dev(uint8_t new_addr[ESP_BD_ADDR_LEN]);
int bt_get_addr_dev(uint8_t new_addr[ESP_BD_ADDR_LEN]);


esp_err_t init_gap_client();

void ble_scanner_start(void);
void ble_scanner_stop(void);

bool bt_gap_get_scann_state();

bool bt_gap_get_conn_state();
int bt_gap_set_conn_state(bool state);

#endif