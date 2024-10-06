/* Declare static functions */

#include "gap.h"
#include "gatt_cli.h"


#define GAPC_TAG "GAPC"
static bool connect    = false;

static bool scanning        = false;

// ble addr to remote device init 
static uint8_t remote_addr_dev[] = {0xF0, 0XA3, 0xED, 0x3A, 0xE9, 0xFE};


//-------------------------------------------------//
static esp_ble_scan_params_t ble_scan_params = {
    .scan_type              = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type          = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy     = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval          = BT_SCAN_INTERVAL,
    .scan_window            = BT_SCAN_DURATION, //0x30,
    .scan_duplicate         = BLE_SCAN_DUPLICATE_DISABLE
};


static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    uint8_t *adv_name = NULL;
    uint8_t adv_name_len = 0;
    switch (event) {
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT: {
        //the unit of the duration is second
        /*
        uint32_t duration = 30;
        esp_ble_gap_start_scanning(duration);
        */
        break;
    }
    case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
        scanning = true;
        //scan start complete event to indicate scan start successfully or failed
        if (param->scan_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(GAPC_TAG, "scan start failed, error status = %x", param->scan_start_cmpl.status);
            break;
        }
        ESP_LOGI(GAPC_TAG, "scan start success");

        break;
    case ESP_GAP_BLE_SCAN_RESULT_EVT: {
        esp_ble_gap_cb_param_t *scan_result = (esp_ble_gap_cb_param_t *)param;
        switch (scan_result->scan_rst.search_evt) {
        case ESP_GAP_SEARCH_INQ_RES_EVT:
            uint8_t mac_dev_found[6] = {0};
            memcpy(mac_dev_found, scan_result->scan_rst.bda, sizeof(mac_dev_found));

            ESP_LOG_BUFFER_HEX(GAPC_TAG, scan_result->scan_rst.bda, 6);
            ESP_LOGI(GAPC_TAG, "searched Adv Data Len %d, Scan Response Len %d", scan_result->scan_rst.adv_data_len, scan_result->scan_rst.scan_rsp_len);
            
            ESP_LOGI(GAPC_TAG, " ");
            if (memcmp(remote_addr_dev, mac_dev_found, sizeof(remote_addr_dev)) == 0)
            {
                ESP_LOGW(GAPC_TAG, "FOUND ADDR OF DEVICE");
                
                adv_name = esp_ble_resolve_adv_data(scan_result->scan_rst.ble_adv, ESP_BLE_AD_TYPE_NAME_CMPL, &adv_name_len);
                ESP_LOGI(GAPC_TAG, "searched Device Name Len %d", adv_name_len);
                ESP_LOG_BUFFER_CHAR(GAPC_TAG, adv_name, adv_name_len);

                if (scan_result->scan_rst.adv_data_len > 0) {
                    ESP_LOGI(GAPC_TAG, "adv data:");
                    ESP_LOG_BUFFER_HEX(GAPC_TAG, &scan_result->scan_rst.ble_adv[0], scan_result->scan_rst.adv_data_len);
                }
                if (scan_result->scan_rst.scan_rsp_len > 0) {
                    ESP_LOGI(GAPC_TAG, "scan resp:");
                    ESP_LOG_BUFFER_HEX(GAPC_TAG, &scan_result->scan_rst.ble_adv[scan_result->scan_rst.adv_data_len], scan_result->scan_rst.scan_rsp_len);
                }

                
                if (connect == false) {
                    connect = true;
                    ESP_LOGI(GAPC_TAG, "connect to the remote device.");
                    esp_ble_gap_stop_scanning();
                    scanning = false;
                    esp_ble_gattc_open(gl_profile_tab[PROFILE_A_APP_ID].gattc_if, scan_result->scan_rst.bda, scan_result->scan_rst.ble_addr_type, true);
                }
            }
            
            break;
        case ESP_GAP_SEARCH_INQ_CMPL_EVT:
            scanning = false;
            break;
        default:
            break;
        }
        break;
    }

    case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
        scanning  = false;
        if (param->scan_stop_cmpl.status != ESP_BT_STATUS_SUCCESS){
            ESP_LOGE(GAPC_TAG, "scan stop failed, error status = %x", param->scan_stop_cmpl.status);
            break;
        }

        ESP_LOGI(GAPC_TAG, "stop scan successfully");
        break;

    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS){
            ESP_LOGE(GAPC_TAG, "adv stop failed, error status = %x", param->adv_stop_cmpl.status);
            break;
        }
        ESP_LOGI(GAPC_TAG, "stop adv successfully");
        break;
    case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
         ESP_LOGI(GAPC_TAG, "update connection params status = %d, min_int = %d, max_int = %d,conn_int = %d,latency = %d, timeout = %d",
                  param->update_conn_params.status,
                  param->update_conn_params.min_int,
                  param->update_conn_params.max_int,
                  param->update_conn_params.conn_int,
                  param->update_conn_params.latency,
                  param->update_conn_params.timeout);
        break;
    default:
        break;
    }
}


int bt_set_addr_dev(uint8_t new_addr[ESP_BD_ADDR_LEN]){
    
    if (new_addr == NULL) {
        return -1; // Dirección nueva es NULL
    }

    // ble addr to remote device init 
    if(memcpy(remote_addr_dev, new_addr,ESP_BD_ADDR_LEN)!=NULL){
        return 0; // update ADDR OK
    };
    
    return -2; // Fail set addr
}

int bt_get_addr_dev(uint8_t addr_buffer[ESP_BD_ADDR_LEN]){
    if (addr_buffer == NULL) {
        return -1; // Buffer de destino es NULL
    }

    if (memcpy(addr_buffer, remote_addr_dev, ESP_BD_ADDR_LEN) != NULL) {
        return 0; // Copy ADDR OK
    }

    return -2; // Fallo al obtener dirección
}




bool bt_gap_get_scann_state(){
    return scanning;
}

bool bt_gap_get_conn_state(){
    return connect;
}
int bt_gap_set_conn_state(bool state){
    connect = state;
    return 0;
}


/************************************************
 * 
 ***********************************************/
void ble_scanner_start(void) {
    if (!scanning) {
        esp_ble_gap_set_scan_params(&ble_scan_params); 
        esp_ble_gap_start_scanning(BT_SCAN_DURATION);
        scanning = true;
        ESP_LOGE(GAPC_TAG, "Started BLE scanning...\n");
    }
}

void ble_scanner_stop(void) {
    if (scanning) {
        esp_ble_gap_stop_scanning();
        scanning = false;
        ESP_LOGE(GAPC_TAG, "Stopped BLE scanning \n");
    }
}


/*=========================================================*/

esp_err_t init_gap_client(){
    esp_err_t ret = 0;
    //register the  callback function to the gap module
    ret = esp_ble_gap_register_callback(esp_gap_cb);
    return ret;
}
