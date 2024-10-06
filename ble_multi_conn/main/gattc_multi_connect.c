/*
 * SPDX-FileCopyrightText: 2021-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */



/****************************************************************************
*
* This file is for gatt client. It can scan ble device, connect multiple devices,
* The gattc_multi_connect demo can connect three ble slaves at the same time.
* Modify the name of gatt_server demo named ESP_GATTS_DEMO_a,
* ESP_GATTS_DEMO_b and ESP_GATTS_DEMO_c,then run three demos,the gattc_multi_connect demo will connect
* the three gatt_server demos, and then exchange data.
* Of course you can also modify the code to connect more devices, we default to connect
* up to 4 devices, more than 4 you need to modify menuconfig.
*
****************************************************************************/

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include "nvs.h"
#include "nvs_flash.h"
#include "time.h"
#include "esp_timer.h"


#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_gatt_defs.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

#define GATTC_TAG "GATTC_MULTIPLE_DEMO"

#define  TAG_GATT_A "GATTC_A"
#define  TAG_GATT_B "GATTC_B"



#define REMOTE_SERVICE_UUID_01     0xFFB0
#define REMOTE_SERVICE_UUID_02     0x2020

#define REMOTE_NOTIFY_CHAR_UUID    0xFF01


#define MAX_CHAR_COUNT      5

/* register three profiles, each profile corresponds to one connection,
   which makes it easy to handle each connection event */
#define PROFILE_NUM 2

#define PROFILE_A_APP_ID 0
#define PROFILE_B_APP_ID 1


#define INVALID_HANDLE   0

/* Declare static functions */
static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
static void esp_gattc_cb(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);
static void gattc_profile_a_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);
static void gattc_profile_b_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);

char* esp_log_uuid(esp_bt_uuid_t *uuid);

static esp_bt_uuid_t remote_filter_service_uuid_01 = {
    .len = ESP_UUID_LEN_16,
    .uuid = {.uuid16 = REMOTE_SERVICE_UUID_01,},
};

static esp_bt_uuid_t remote_filter_service_uuid_02 = {
    .len = ESP_UUID_LEN_16,
    .uuid = {.uuid16 = REMOTE_SERVICE_UUID_02,},
};


static esp_bt_uuid_t notify_descr_uuid = {
    .len = ESP_UUID_LEN_16,
    .uuid = {.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG,},
};

static bool conn_device_a   = false;
static bool conn_device_b   = false;

static bool get_service_a   = false;
static bool get_service_b   = false;

static bool Isconnecting    = false;
static bool stop_scan_done  = false;



static esp_gattc_char_elem_t  *char_elem_result_a   = NULL;
static esp_gattc_descr_elem_t *descr_elem_result_a  = NULL;
static esp_gattc_char_elem_t  *char_elem_result_b   = NULL;
static esp_gattc_descr_elem_t *descr_elem_result_b  = NULL;


static const char remote_device_addr[2][6] = {{0xF0, 0XA3, 0xED, 0x3A, 0xE9, 0xFE},     // ADDR M3F
                                              {0x24, 0X6F, 0x28, 0x9D, 0x20, 0xDA}};    // ADDR CTRL


static esp_ble_scan_params_t ble_scan_params = {
    .scan_type              = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type          = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy     = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval          = 0x50,
    .scan_window            = 0x30,
    .scan_duplicate         = BLE_SCAN_DUPLICATE_DISABLE
};

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

static uint16_t char_count_A = 0;
static struct gatt_chars_info gl_chars_info_A[MAX_CHAR_COUNT];

static uint16_t char_count_B = 0;
static struct gatt_chars_info gl_chars_info_B[MAX_CHAR_COUNT];


/* One gatt-based profile one app_id and one gattc_if, this array will store the gattc_if returned by ESP_GATTS_REG_EVT */
static struct gattc_profile_inst gl_profile_tab[PROFILE_NUM] = {
    [PROFILE_A_APP_ID] = {
        .gattc_cb = gattc_profile_a_event_handler,
        .gattc_if = ESP_GATT_IF_NONE,       /* Not get the gatt_if, so initial is ESP_GATT_IF_NONE */
    },
    [PROFILE_B_APP_ID] = {
        .gattc_cb = gattc_profile_b_event_handler,
        .gattc_if = ESP_GATT_IF_NONE,       /* Not get the gatt_if, so initial is ESP_GATT_IF_NONE */
    },
};


static void start_scan(void) {
    static time_t t_scan = 0;
    
    time_t time_vol = time(NULL);
    if (difftime(time_vol, t_scan)>30)
    {
        t_scan = time_vol;
       ESP_LOGE(GATTC_TAG, "INIT SCANN");
        stop_scan_done = false;
        Isconnecting = false;
        uint32_t duration = 30;
        esp_ble_gap_start_scanning(duration);
    }
    return;
}

static void gattc_profile_a_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param)
{

    esp_ble_gattc_cb_param_t *p_data = (esp_ble_gattc_cb_param_t *)param;
    ESP_LOGE(TAG_GATT_A, "EVENT : %u", event);

    switch (event) {
    case ESP_GATTC_REG_EVT:
        ESP_LOGI(TAG_GATT_A, "REG_EVT");
        esp_err_t scan_ret = esp_ble_gap_set_scan_params(&ble_scan_params);
        if (scan_ret){
            ESP_LOGE(TAG_GATT_A, "set scan params error, error code = %x", scan_ret);
        }
        break;
    /* one device connect successfully, all profiles callback function will get the ESP_GATTC_CONNECT_EVT,
     so must compare the mac address to check which device is connected, so it is a good choice to use ESP_GATTC_OPEN_EVT. */
    case ESP_GATTC_CONNECT_EVT:
        ESP_LOGI(TAG_GATT_A, "ESP_GATTC_CONNECT_EVT conn_id %d, if %d", p_data->connect.conn_id, gattc_if);
        gl_profile_tab[PROFILE_A_APP_ID].conn_id = p_data->connect.conn_id;
        memcpy(gl_profile_tab[PROFILE_A_APP_ID].remote_bda, p_data->connect.remote_bda, sizeof(esp_bd_addr_t));
        ESP_LOGI(TAG_GATT_A, "REMOTE BDA:");
        esp_log_buffer_hex(TAG_GATT_A, gl_profile_tab[PROFILE_A_APP_ID].remote_bda, sizeof(esp_bd_addr_t));
        esp_err_t mtu_ret = esp_ble_gattc_send_mtu_req (gattc_if, p_data->connect.conn_id);
        if (mtu_ret){
            ESP_LOGE(TAG_GATT_A, "config MTU error, error code = %x", mtu_ret);
        }
        break;
    case ESP_GATTC_OPEN_EVT:
        if (param->open.status != ESP_GATT_OK){
            //open failed, ignore the first device, connect the second device
            ESP_LOGE(TAG_GATT_A, "connect device failed, status %d", p_data->open.status);
            conn_device_a = false;
            start_scan();
        }
        ESP_LOGI(TAG_GATT_A, "open success");
        break;

    // ADD CONFIG
    case ESP_GATTC_DIS_SRVC_CMPL_EVT:{
        if (param->dis_srvc_cmpl.status != ESP_GATT_OK){
            ESP_LOGE(TAG_GATT_A, "discover service failed, status %d", param->dis_srvc_cmpl.status);
            break;
        }
        ESP_LOGI(TAG_GATT_A, "discover service complete conn_id %d", param->dis_srvc_cmpl.conn_id);
        esp_ble_gattc_search_service(gattc_if, param->dis_srvc_cmpl.conn_id, &remote_filter_service_uuid_01);
        break;}

    case ESP_GATTC_CFG_MTU_EVT:
        if (param->cfg_mtu.status != ESP_GATT_OK){
            ESP_LOGE(TAG_GATT_A,"Config mtu failed");
        }
        ESP_LOGI(TAG_GATT_A, "ESP_GATTC_CFG_MTU_EVT, Status %d, MTU %d, conn_id %d", param->cfg_mtu.status, param->cfg_mtu.mtu, param->cfg_mtu.conn_id);
        break;

    case ESP_GATTC_SEARCH_RES_EVT: {
        ESP_LOGI(TAG_GATT_A, "SEARCH RES: conn_id = %x is primary service %d", p_data->search_res.conn_id, p_data->search_res.is_primary);
        ESP_LOGI(TAG_GATT_A, "start handle %d end handle %d current handle value %d", p_data->search_res.start_handle, p_data->search_res.end_handle, p_data->search_res.srvc_id.inst_id);
        if (p_data->search_res.srvc_id.uuid.len == ESP_UUID_LEN_16 && p_data->search_res.srvc_id.uuid.uuid.uuid16 == REMOTE_SERVICE_UUID_01) {
            ESP_LOGI(TAG_GATT_A, "service found");
            get_service_a = true;
            gl_profile_tab[PROFILE_A_APP_ID].service_start_handle = p_data->search_res.start_handle;
            gl_profile_tab[PROFILE_A_APP_ID].service_end_handle = p_data->search_res.end_handle;
            ESP_LOGI(TAG_GATT_A, "UUID16: %X", p_data->search_res.srvc_id.uuid.uuid.uuid16);
        }
        
        break;
    }
    case ESP_GATTC_SEARCH_CMPL_EVT:
        if (p_data->search_cmpl.status != ESP_GATT_OK){
            ESP_LOGE(TAG_GATT_A, "search service failed, error status = %x", p_data->search_cmpl.status);
            break;
        }
        if (get_service_a){
            uint16_t count = 0;
            esp_gatt_status_t status = esp_ble_gattc_get_attr_count( gattc_if,
                                                                     p_data->search_cmpl.conn_id,
                                                                     ESP_GATT_DB_CHARACTERISTIC,
                                                                     gl_profile_tab[PROFILE_A_APP_ID].service_start_handle,
                                                                     gl_profile_tab[PROFILE_A_APP_ID].service_end_handle,
                                                                     INVALID_HANDLE,
                                                                     &count);
            if (status != ESP_GATT_OK){
                ESP_LOGE(TAG_GATT_A, "esp_ble_gattc_get_attr_count error");
                break;
            }
            if (count > 0) {
                char_elem_result_a = (esp_gattc_char_elem_t *)malloc(sizeof(esp_gattc_char_elem_t) * count);
                if (!char_elem_result_a){
                    ESP_LOGE(TAG_GATT_A, "gattc no mem");
                    break;
                }else {
                    status = esp_ble_gattc_get_all_char( gattc_if,
                                                            p_data->search_cmpl.conn_id,
                                                            gl_profile_tab[PROFILE_A_APP_ID].service_start_handle,
                                                            gl_profile_tab[PROFILE_A_APP_ID].service_end_handle,
                                                            char_elem_result_a,
                                                            &count,
                                                            0);

                    if (status != ESP_GATT_OK){
                        ESP_LOGE(TAG_GATT_A, "esp_ble_gattc_get_char_by_uuid error");
                        free(char_elem_result_a);
                        char_elem_result_a = NULL;
                        break;
                    }

                    
                    for (int i = 0; i < count; i++) {
                        ESP_LOGW(TAG_GATT_A, "Characteristic found: handle = %d, properties = %d, uuid = %s",
                                            char_elem_result_a[i].char_handle,
                                            char_elem_result_a[i].properties,
                                            esp_log_uuid(&char_elem_result_a[i].uuid));
                        
                        // check len uuid len is 16
                        if((char_elem_result_a[i].uuid.len == ESP_UUID_LEN_16) && 
                            (char_elem_result_a[i].properties & ESP_GATT_CHAR_PROP_BIT_NOTIFY)){
                        
                                esp_err_t rg_status = esp_ble_gattc_register_for_notify(gattc_if, 
                                                                                        gl_profile_tab[PROFILE_A_APP_ID].remote_bda,
                                                                                        char_elem_result_a[i].char_handle);
                                
                                // REGISTER UUID TO CHARS
                                gl_chars_info_A[i].conn_id = p_data->search_cmpl.conn_id;
                                gl_chars_info_A[i].char_handle = char_elem_result_a[i].char_handle;
                                gl_chars_info_A[i].char_uuid_16 = char_elem_result_a[i].uuid.uuid.uuid16;
                                gl_chars_info_A[i].gattc_if = gattc_if;
                                gl_chars_info_A[i].status = 0X00;// OK
                                if (rg_status !=ESP_OK){
                                    gl_chars_info_A[i].status = 0XFF;// FAIL
                                }
                            }
                        }
                    char_count_A = count;
                }
                /* free char_elem_result */
                free(char_elem_result_a);
                char_elem_result_a = NULL;
            }else {
                ESP_LOGE(TAG_GATT_A, "no char found or too many chars");
            }
        }
        break;
    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
        if (p_data->reg_for_notify.status != ESP_GATT_OK){
            ESP_LOGE(TAG_GATT_A, "reg notify failed, error status =%x", p_data->reg_for_notify.status);
            break;
        }
        
        uint16_t notify_en = 1;
        for (int i = 0; i < char_count_A; i++) {
            uint16_t count = 0;
            esp_gatt_status_t ret_status = esp_ble_gattc_get_attr_count( gattc_if,
                                                                     gl_profile_tab[PROFILE_A_APP_ID].conn_id,
                                                                     ESP_GATT_DB_DESCRIPTOR,
                                                                     gl_profile_tab[PROFILE_A_APP_ID].service_start_handle,
                                                                     gl_profile_tab[PROFILE_A_APP_ID].service_end_handle,
                                                                     gl_chars_info_A[i].char_handle,
                                                                     &count);
            if (ret_status != ESP_GATT_OK){
                ESP_LOGE(TAG_GATT_A, "esp_ble_gattc_get_attr_count error");
            }
            if (count > 0){
                descr_elem_result_a = (esp_gattc_descr_elem_t *)malloc(sizeof(esp_gattc_descr_elem_t) * count);
                if (!descr_elem_result_a){
                    ESP_LOGE(TAG_GATT_A, "malloc error, gattc no mem");
                    continue;
                }else{
                    ret_status = esp_ble_gattc_get_descr_by_char_handle( gattc_if,
                                                                        gl_profile_tab[PROFILE_A_APP_ID].conn_id,
                                                                        p_data->reg_for_notify.handle,
                                                                        notify_descr_uuid,
                                                                        descr_elem_result_a,
                                                                        &count);
                    if (ret_status != ESP_GATT_OK){
                        ESP_LOGE(TAG_GATT_A, "esp_ble_gattc_get_descr_by_char_handle error");
                        free(descr_elem_result_a);
                        descr_elem_result_a = NULL;
                        continue;
                    }

                    /* Every char has only one descriptor in our 'ESP_GATTS_DEMO' demo, so we used first 'descr_elem_result' */
                    if (count > 0 && descr_elem_result_a[0].uuid.len == ESP_UUID_LEN_16 && descr_elem_result_a[0].uuid.uuid.uuid16 == ESP_GATT_UUID_CHAR_CLIENT_CONFIG){
                        ret_status = esp_ble_gattc_write_char_descr( gattc_if,
                                                                    gl_profile_tab[PROFILE_A_APP_ID].conn_id,
                                                                    descr_elem_result_a[0].handle,
                                                                    sizeof(notify_en),
                                                                    (uint8_t *)&notify_en,
                                                                    ESP_GATT_WRITE_TYPE_RSP,
                                                                    ESP_GATT_AUTH_REQ_NONE);
                    

                    if (ret_status != ESP_GATT_OK){
                        ESP_LOGE(TAG_GATT_A, "esp_ble_gattc_write_char_descr error for char handle %d", gl_chars_info_A[i].char_handle);
                    }

                }
                /* free descr_elem_result */
                free(descr_elem_result_a);
                }
            }
            else{
                ESP_LOGE(TAG_GATT_A, "decsr not found");
            }
        }
        break;
    }
    case ESP_GATTC_NOTIFY_EVT:
        ESP_LOGI(TAG_GATT_A, "ESP_GATTC_NOTIFY_EVT, Receive notify value:");
        ESP_LOGW(TAG_GATT_A,"NOTITY HANDLE: %u, CONN ID: %u, LEN: %d",p_data->notify.handle, p_data->notify.conn_id,p_data->notify.value_len);
        // ESP_LOG_BUFFER_HEX_LEVEL(TAG_GATT_A, p_data->notify.value, p_data->notify.value_len,ESP_LOG_WARN);
        ESP_LOG_BUFFER_HEX_LEVEL(TAG_GATT_A, p_data->notify.value, p_data->notify.value_len, ESP_LOG_WARN);
        break;
    case ESP_GATTC_WRITE_DESCR_EVT:
        if (p_data->write.status != ESP_GATT_OK){
            ESP_LOGE(TAG_GATT_A, "write descr failed, error status = %x", p_data->write.status);
            break;
        }
        ESP_LOGI(TAG_GATT_A, "write descr success, con_id:%d ",p_data->write.conn_id);
        start_scan();

        break;
    case ESP_GATTC_WRITE_CHAR_EVT:
        if (p_data->write.status != ESP_GATT_OK){
            ESP_LOGE(TAG_GATT_A, "write char failed, error status = %x", p_data->write.status);
        }else{
            ESP_LOGI(TAG_GATT_A, "write char success");
        }
        // start_scan();
        break;
    case ESP_GATTC_SRVC_CHG_EVT: {
        esp_bd_addr_t bda;
        memcpy(bda, p_data->srvc_chg.remote_bda, sizeof(esp_bd_addr_t));
        ESP_LOGI(TAG_GATT_A, "ESP_GATTC_SRVC_CHG_EVT, bd_addr:%08x%04x",(bda[0] << 24) + (bda[1] << 16) + (bda[2] << 8) + bda[3],
                 (bda[4] << 8) + bda[5]);
        break;
    }
    case ESP_GATTC_DISCONNECT_EVT:
        //Start scanning again
        start_scan();
        if (memcmp(p_data->disconnect.remote_bda, gl_profile_tab[PROFILE_A_APP_ID].remote_bda, 6) == 0){
            ESP_LOGI(TAG_GATT_A, "device a disconnect");
            conn_device_a = false;
            get_service_a = false;
        }
        break;
    default:
        break;
    }
}



static void gattc_profile_b_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param)
{
    esp_ble_gattc_cb_param_t *p_data = (esp_ble_gattc_cb_param_t *)param;
    ESP_LOGE(TAG_GATT_B, "EVENT : %u", event);

    switch (event) {
    case ESP_GATTC_REG_EVT:
        ESP_LOGI(TAG_GATT_B, "REG_EVT");
        break;
    case ESP_GATTC_CONNECT_EVT:
        ESP_LOGI(TAG_GATT_B, "ESP_GATTC_CONNECT_EVT conn_id %d, if %d", p_data->connect.conn_id, gattc_if);
        gl_profile_tab[PROFILE_B_APP_ID].conn_id = p_data->connect.conn_id;
        memcpy(gl_profile_tab[PROFILE_B_APP_ID].remote_bda, p_data->connect.remote_bda, sizeof(esp_bd_addr_t));
        ESP_LOGI(TAG_GATT_B, "REMOTE BDA:");
        esp_log_buffer_hex(TAG_GATT_B, gl_profile_tab[PROFILE_B_APP_ID].remote_bda, sizeof(esp_bd_addr_t));
        esp_err_t mtu_ret = esp_ble_gattc_send_mtu_req (gattc_if, p_data->connect.conn_id);
        if (mtu_ret){
            ESP_LOGE(TAG_GATT_B, "config MTU error, error code = %x", mtu_ret);
        }
        break;
    case ESP_GATTC_OPEN_EVT:
        if (p_data->open.status != ESP_GATT_OK){
            //open failed, ignore the second device, connect the third device
            ESP_LOGE(TAG_GATT_B, "connect device failed, status %d", p_data->open.status);
            conn_device_b = false;
            start_scan();
            break;
        }
        break;
    case ESP_GATTC_DIS_SRVC_CMPL_EVT:{
        if (param->dis_srvc_cmpl.status != ESP_GATT_OK){
            ESP_LOGE(TAG_GATT_B, "discover service failed, status %d", param->dis_srvc_cmpl.status);
            break;
        }
        ESP_LOGI(TAG_GATT_B, "discover service complete conn_id %d", param->dis_srvc_cmpl.conn_id);
        esp_ble_gattc_search_service(gattc_if, param->dis_srvc_cmpl.conn_id, &remote_filter_service_uuid_02);
        break;}

    case ESP_GATTC_CFG_MTU_EVT:
        if (param->cfg_mtu.status != ESP_GATT_OK){
            ESP_LOGE(TAG_GATT_B,"Config mtu failed");
        }
        break;
    case ESP_GATTC_SEARCH_RES_EVT: {
        ESP_LOGI(TAG_GATT_B, "SEARCH RES: conn_id = %x is primary service %d", p_data->search_res.conn_id, p_data->search_res.is_primary);
        ESP_LOGI(TAG_GATT_B, "start handle %d end handle %d current handle value %d", p_data->search_res.start_handle, p_data->search_res.end_handle, p_data->search_res.srvc_id.inst_id);
        if (p_data->search_res.srvc_id.uuid.len == ESP_UUID_LEN_16 && p_data->search_res.srvc_id.uuid.uuid.uuid16 == REMOTE_SERVICE_UUID_02) {
            ESP_LOGI(TAG_GATT_B, "UUID16: %x", p_data->search_res.srvc_id.uuid.uuid.uuid16);
            get_service_b = true;
            gl_profile_tab[PROFILE_B_APP_ID].service_start_handle = p_data->search_res.start_handle;
            gl_profile_tab[PROFILE_B_APP_ID].service_end_handle = p_data->search_res.end_handle;
        }
        break;
    }
    case ESP_GATTC_SEARCH_CMPL_EVT:
        if (p_data->search_cmpl.status != ESP_GATT_OK){
            ESP_LOGE(TAG_GATT_B, "search service failed, error status = %x", p_data->search_cmpl.status);
            break;
        }
        if (get_service_b){
            uint16_t count = 0;
            esp_gatt_status_t status = esp_ble_gattc_get_attr_count( gattc_if,
                                                                     p_data->search_cmpl.conn_id,
                                                                     ESP_GATT_DB_CHARACTERISTIC,
                                                                     gl_profile_tab[PROFILE_B_APP_ID].service_start_handle,
                                                                     gl_profile_tab[PROFILE_B_APP_ID].service_end_handle,
                                                                     INVALID_HANDLE,
                                                                     &count);
            if (status != ESP_GATT_OK){
                ESP_LOGE(TAG_GATT_B, "esp_ble_gattc_get_attr_count error");
            }

            if (count > 0){
                char_elem_result_b = (esp_gattc_char_elem_t *)malloc(sizeof(esp_gattc_char_elem_t) * count);
                if (!char_elem_result_b){
                    ESP_LOGE(TAG_GATT_B, "gattc no mem");
                    break;
                }else{
                    status = esp_ble_gattc_get_all_char( gattc_if,
                                                            p_data->search_cmpl.conn_id,
                                                            gl_profile_tab[PROFILE_B_APP_ID].service_start_handle,
                                                            gl_profile_tab[PROFILE_B_APP_ID].service_end_handle,
                                                            char_elem_result_b,
                                                            &count,
                                                            0);

                    if (status != ESP_GATT_OK){
                        ESP_LOGE(TAG_GATT_B, "esp_ble_gattc_get_char_by_uuid error");
                        free(char_elem_result_b);
                        char_elem_result_b = NULL;
                        break;
                    }
                    
                    for (int i = 0; i < count; i++) {
                        ESP_LOGW(TAG_GATT_B, "Characteristic found: handle = %d, properties = %d, uuid = %s",
                                            char_elem_result_b[i].char_handle,
                                            char_elem_result_b[i].properties,
                                            esp_log_uuid(&char_elem_result_b[i].uuid));
                        
                        // check len uuid len is 16
                        if((char_elem_result_b[i].uuid.len == ESP_UUID_LEN_16) && 
                            (char_elem_result_b[i].properties & ESP_GATT_CHAR_PROP_BIT_NOTIFY)){
                        
                                esp_err_t rg_status = esp_ble_gattc_register_for_notify(gattc_if, gl_profile_tab[PROFILE_B_APP_ID].remote_bda, char_elem_result_b[i].char_handle);
                                
                                // REGISTER UUID TO CHARS
                                gl_chars_info_B[i].conn_id = p_data->search_cmpl.conn_id;
                                gl_chars_info_B[i].char_handle = char_elem_result_b[i].char_handle;
                                gl_chars_info_B[i].char_uuid_16 = char_elem_result_b[i].uuid.uuid.uuid16;
                                gl_chars_info_B[i].gattc_if = gattc_if;
                                gl_chars_info_B[i].status = 0X00;// OK
                                if (rg_status !=ESP_OK){
                                    gl_chars_info_B[i].status = 0XFF;// FAIL
                                }
                            }
                        }
                    char_count_B = count;
                }
                /* free char_elem_result */
                free(char_elem_result_b);
                char_elem_result_b = NULL;
            }else{
                ESP_LOGE(TAG_GATT_B, "no char found");
            }
        }
        break;
    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {

        if (p_data->reg_for_notify.status != ESP_GATT_OK){
            ESP_LOGE(TAG_GATT_B, "reg notify failed, error status =%x", p_data->reg_for_notify.status);
            break;
        }
        uint16_t notify_en = 1;
        for (int i = 0; i < char_count_B; i++) {
            uint16_t count = 0;
            esp_gatt_status_t ret_status = esp_ble_gattc_get_attr_count( gattc_if,
                                                                     gl_profile_tab[PROFILE_B_APP_ID].conn_id,
                                                                     ESP_GATT_DB_DESCRIPTOR,
                                                                     gl_profile_tab[PROFILE_B_APP_ID].service_start_handle,
                                                                     gl_profile_tab[PROFILE_B_APP_ID].service_end_handle,
                                                                     gl_chars_info_B[i].char_handle,
                                                                     &count);
            if (ret_status != ESP_GATT_OK){
                ESP_LOGE(TAG_GATT_B, "esp_ble_gattc_get_attr_count error");
                continue;
            }

            if (count > 0){
                descr_elem_result_b = (esp_gattc_descr_elem_t *)malloc(sizeof(esp_gattc_descr_elem_t) * count);
                if (!descr_elem_result_b){
                    ESP_LOGE(TAG_GATT_B, "malloc error, gattc no mem");
                    continue;
                }else{
                    ret_status = esp_ble_gattc_get_descr_by_char_handle(gattc_if,
                                                                            gl_profile_tab[PROFILE_B_APP_ID].conn_id,
                                                                            gl_chars_info_B[i].char_handle,
                                                                            notify_descr_uuid,
                                                                            descr_elem_result_b,
                                                                            &count);
                    if (ret_status != ESP_GATT_OK){
                        ESP_LOGE(TAG_GATT_B, "esp_ble_gattc_get_descr_by_char_handle error");
                        free(descr_elem_result_b);
                        descr_elem_result_b = NULL;
                        continue;
                    }

                    /* Every char has only one descriptor in our 'ESP_GATTS_DEMO' demo, so we used first 'descr_elem_result' */
                    if (count > 0 && descr_elem_result_b[0].uuid.len == ESP_UUID_LEN_16 && descr_elem_result_b[0].uuid.uuid.uuid16 == ESP_GATT_UUID_CHAR_CLIENT_CONFIG){
                        ret_status = esp_ble_gattc_write_char_descr( gattc_if,
                                                                    gl_profile_tab[PROFILE_B_APP_ID].conn_id,
                                                                    descr_elem_result_b[0].handle,
                                                                    sizeof(notify_en),
                                                                    (uint8_t *)&notify_en,
                                                                    ESP_GATT_WRITE_TYPE_RSP,
                                                                    ESP_GATT_AUTH_REQ_NONE);
                    

                        if (ret_status != ESP_GATT_OK){
                            ESP_LOGE(TAG_GATT_B, "esp_ble_gattc_write_char_descr error for char handle %d", gl_chars_info_B[i].char_handle);
                        }
                    }
                    /* free descr_elem_result */
                    free(descr_elem_result_b);
                    descr_elem_result_b = NULL;
                }
            }
            else{
                ESP_LOGE(TAG_GATT_B, "decsr not found");
            }
        }
        break;
    }
    case ESP_GATTC_NOTIFY_EVT:
        ESP_LOGI(TAG_GATT_B, "ESP_GATTC_NOTIFY_EVT, Receive notify value:");
        ESP_LOG_BUFFER_HEX_LEVEL("NOT PORFILE B", p_data->notify.value, p_data->notify.value_len,ESP_LOG_WARN);
        break;
    case ESP_GATTC_WRITE_DESCR_EVT:
        if (p_data->write.status != ESP_GATT_OK){
            ESP_LOGE(TAG_GATT_B, "write descr failed, error status = %x", p_data->write.status);
            break;
        }
        ESP_LOGI(TAG_GATT_B, "write descr success");
        start_scan();
        break;
    case ESP_GATTC_WRITE_CHAR_EVT:
        if (p_data->write.status != ESP_GATT_OK){
            ESP_LOGE(TAG_GATT_B, "Write char failed, error status = %x", p_data->write.status);
        }else{
            ESP_LOGI(TAG_GATT_B, "Write char success");
        }
        start_scan();
        break;
    case ESP_GATTC_SRVC_CHG_EVT: {
        esp_bd_addr_t bda;
        memcpy(bda, p_data->srvc_chg.remote_bda, sizeof(esp_bd_addr_t));
        ESP_LOGI(TAG_GATT_B, "ESP_GATTC_SRVC_CHG_EVT, bd_addr:%08x%04x",(bda[0] << 24) + (bda[1] << 16) + (bda[2] << 8) + bda[3],
                 (bda[4] << 8) + bda[5]);
        break;
    }
    case ESP_GATTC_DISCONNECT_EVT:
        if (memcmp(p_data->disconnect.remote_bda, gl_profile_tab[PROFILE_B_APP_ID].remote_bda, 6) == 0){
            ESP_LOGI(TAG_GATT_B, "device b disconnect");
            conn_device_b = false;
            get_service_b = false;
        }
        break;
    default:
        break;
    }
}



static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{

    switch (event) {
    case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
         ESP_LOGI(GATTC_TAG, "update connection params status = %d, min_int = %d, max_int = %d,conn_int = %d,latency = %d, timeout = %d",
                  param->update_conn_params.status,
                  param->update_conn_params.min_int,
                  param->update_conn_params.max_int,
                  param->update_conn_params.conn_int,
                  param->update_conn_params.latency,
                  param->update_conn_params.timeout);
        break;
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT: {
        //the unit of the duration is second
        uint32_t duration = 30;
        esp_ble_gap_start_scanning(duration);
        break;
    }
    case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
        //scan start complete event to indicate scan start successfully or failed
        if (param->scan_start_cmpl.status == ESP_BT_STATUS_SUCCESS) {
            ESP_LOGI(GATTC_TAG, "Scan start success");
        }else{
            ESP_LOGE(GATTC_TAG, "Scan start failed");
        }
        break;
    case ESP_GAP_BLE_SCAN_RESULT_EVT: {
        esp_ble_gap_cb_param_t *scan_result = (esp_ble_gap_cb_param_t *)param;
        switch (scan_result->scan_rst.search_evt) {
        case ESP_GAP_SEARCH_INQ_RES_EVT:

            ESP_LOG_BUFFER_HEX(GATTC_TAG, scan_result->scan_rst.bda, 6);
            ESP_LOGI(GATTC_TAG, "searched Adv Data Len %d, Scan Response Len %d", scan_result->scan_rst.adv_data_len, scan_result->scan_rst.scan_rsp_len);
            
            ESP_LOGI(GATTC_TAG, " ");
            uint8_t mac_dev_found[6] = {0};
            memcpy(mac_dev_found, scan_result->scan_rst.bda, sizeof(mac_dev_found));


            if (Isconnecting){
                break;
            }

            if (conn_device_a && conn_device_b && !stop_scan_done){
                stop_scan_done = true;
                esp_ble_gap_stop_scanning();
                ESP_LOGI(GATTC_TAG, "all devices are connected");
                break;
            }
                
            // memcmp(remote_addr_dev, mac_dev_found, sizeof(remote_addr_dev)) == 0)
            if (memcmp(remote_device_addr[0], mac_dev_found, 6) == 0) {
                if (conn_device_a == false) {
                    conn_device_a = true;
                    ESP_LOGW(GATTC_TAG, "Searched device A");
                    esp_ble_gap_stop_scanning();
                    esp_ble_gattc_open(gl_profile_tab[PROFILE_A_APP_ID].gattc_if, scan_result->scan_rst.bda, scan_result->scan_rst.ble_addr_type, true);
                    Isconnecting = true;
                }
                break;
            }
            else if (memcmp(remote_device_addr[1], mac_dev_found, 6) == 0) {
                if (conn_device_b == false) {
                    conn_device_b = true;
                    ESP_LOGW(GATTC_TAG, "Searched device B");
                    esp_ble_gap_stop_scanning();
                    esp_ble_gattc_open(gl_profile_tab[PROFILE_B_APP_ID].gattc_if, scan_result->scan_rst.bda, scan_result->scan_rst.ble_addr_type, true);
                    Isconnecting = true;
                }
            }
            break;
        case ESP_GAP_SEARCH_INQ_CMPL_EVT:
            break;
        default:
            break;
        }
        break;
    }

    case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
        if (param->scan_stop_cmpl.status != ESP_BT_STATUS_SUCCESS){
            ESP_LOGE(GATTC_TAG, "Scan stop failed");
            break;
        }
        ESP_LOGI(GATTC_TAG, "Stop scan successfully");

        break;

    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS){
            ESP_LOGE(GATTC_TAG, "Adv stop failed");
            break;
        }
        ESP_LOGI(GATTC_TAG, "Stop adv successfully");
        break;

    default:
        break;
    }
}

static void esp_gattc_cb(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param)
{
    //ESP_LOGI(GATTC_TAG, "EVT %d, gattc if %d, app_id %d", event, gattc_if, param->reg.app_id);

    /* If event is register event, store the gattc_if for each profile */
    if (event == ESP_GATTC_REG_EVT) {
        if (param->reg.status == ESP_GATT_OK) {
            gl_profile_tab[param->reg.app_id].gattc_if = gattc_if;
        } else {
            ESP_LOGI(GATTC_TAG, "Reg app failed, app_id %04x, status %d",
                    param->reg.app_id,
                    param->reg.status);
            return;
        }
    }

    /* If the gattc_if equal to profile A, call profile A cb handler,
     * so here call each profile's callback */
    do {
        int idx;
        for (idx = 0; idx < PROFILE_NUM; idx++) {
            if (gattc_if == ESP_GATT_IF_NONE || /* ESP_GATT_IF_NONE, not specify a certain gatt_if, need to call every profile cb function */
                    gattc_if == gl_profile_tab[idx].gattc_if) {
                if (gl_profile_tab[idx].gattc_cb) {
                    gl_profile_tab[idx].gattc_cb(event, gattc_if, param);
                }
            }
        }
    } while (0);
}

void app_main(void)
{
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
        ESP_LOGE(GATTC_TAG, "%s initialize controller failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) {
        ESP_LOGE(GATTC_TAG, "%s enable controller failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_bluedroid_init();
    if (ret) {
        ESP_LOGE(GATTC_TAG, "%s init bluetooth failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(GATTC_TAG, "%s enable bluetooth failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    //register the  callback function to the gap module
    ret = esp_ble_gap_register_callback(esp_gap_cb);
    if (ret){
        ESP_LOGE(GATTC_TAG, "gap register error, error code = %x", ret);
        return;
    }

    //register the callback function to the gattc module
    ret = esp_ble_gattc_register_callback(esp_gattc_cb);
    if(ret){
        ESP_LOGE(GATTC_TAG, "gattc register error, error code = %x", ret);
        return;
    }

    ret = esp_ble_gattc_app_register(PROFILE_A_APP_ID);
    if (ret){
        ESP_LOGE(GATTC_TAG, "gattc app register error, error code = %x", ret);
        return;
    }

    ret = esp_ble_gattc_app_register(PROFILE_B_APP_ID);
    if (ret){
        ESP_LOGE(GATTC_TAG, "gattc app register error, error code = %x", ret);
        return;
    }

    ret = esp_ble_gatt_set_local_mtu(512);
    if (ret){
        ESP_LOGE(GATTC_TAG, "set local  MTU failed, error code = %x", ret);
    }

    while (1)
    {
       

        printf("\n");
        ESP_LOGI("MAIN", "STATUS, CONN A: %d, CONN B: %d", conn_device_a,conn_device_b);
        ESP_LOGI("MAIN", "STATUS, SERV A: %d, SERV B: %d", get_service_a,get_service_b);

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
    

}



/* *************************************************
 * ************************************************* */
char* esp_log_uuid(esp_bt_uuid_t *uuid) {
    static char uuid_str[37]; // UUIDs are 36 characters plus null terminator
    if (uuid->len == ESP_UUID_LEN_16) {
        snprintf(uuid_str, sizeof(uuid_str), "%04x", uuid->uuid.uuid16);
    } else if (uuid->len == ESP_UUID_LEN_32) {
        snprintf(uuid_str, sizeof(uuid_str), "%08lx", uuid->uuid.uuid32);
    } else if (uuid->len == ESP_UUID_LEN_128) {
        snprintf(uuid_str, sizeof(uuid_str), "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                 uuid->uuid.uuid128[0], uuid->uuid.uuid128[1], uuid->uuid.uuid128[2], uuid->uuid.uuid128[3],
                 uuid->uuid.uuid128[4], uuid->uuid.uuid128[5], uuid->uuid.uuid128[6], uuid->uuid.uuid128[7],
                 uuid->uuid.uuid128[8], uuid->uuid.uuid128[9], uuid->uuid.uuid128[10], uuid->uuid.uuid128[11],
                 uuid->uuid.uuid128[12], uuid->uuid.uuid128[13], uuid->uuid.uuid128[14], uuid->uuid.uuid128[15]);
    }
    return uuid_str;
}