
#include "gatt_cli.h"
#include "ble_MK115B.h"


#define GATTC_TAG "GATTC"



static bool get_server      = false;

static esp_gattc_char_elem_t *char_elem_result   = NULL;
static esp_gattc_descr_elem_t *descr_elem_result = NULL;


static uint16_t char_count = 0;
struct gatt_chars_info gl_chars_info[MAX_CHAR_COUNT]={0};





//----------------------------------------------------//
static esp_bt_uuid_t remote_filter_service_uuid = {
    .len = ESP_UUID_LEN_16,
    .uuid = {.uuid16 = BT_SERVICE_UUID,},
};


/*
static esp_bt_uuid_t remote_read_info_char_uuid = {
    .len = ESP_UUID_LEN_16,
    .uuid = {.uuid16 = BT_CHAR_UUID_READ_DEV_INFO,},
};


// falta implementar
static esp_bt_uuid_t remote_set_params_char_uuid = {
    .len = ESP_UUID_LEN_16,
    .uuid = {.uuid16 = BT_CHAR_UUID_SET_DEV_PARAM,},
};

// falta implementar
static esp_bt_uuid_t remote_dev_rep_char_uuid = {
    .len = ESP_UUID_LEN_16,
    .uuid = {.uuid16 = BT_CHAR_UUID_DEV_REP_INFO,},
};

*/

static esp_bt_uuid_t notify_descr_uuid = {
    .len = ESP_UUID_LEN_16,
    .uuid = {.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG,},
};




static void esp_gattc_cb(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);
static void gattc_profile_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);

char* esp_log_uuid(esp_bt_uuid_t *uuid);

/**
 * ========================================================================
*/

/* One gatt-based profile one app_id and one gattc_if, this array will store the gattc_if returned by ESP_GATTS_REG_EVT */
struct gattc_profile_inst gl_profile_tab[PROFILE_NUM] = {
    [PROFILE_A_APP_ID] = {
        .gattc_cb = gattc_profile_event_handler,
        .gattc_if = ESP_GATT_IF_NONE,       /* Not get the gatt_if, so initial is ESP_GATT_IF_NONE */
    },
};




static void gattc_profile_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param)
{
    esp_ble_gattc_cb_param_t *p_data = (esp_ble_gattc_cb_param_t *)param;

    switch (event) {
    case ESP_GATTC_REG_EVT:{
        ESP_LOGI(GATTC_TAG, "REG_EVT");
        /*
        esp_err_t scan_ret = esp_ble_gap_set_scan_params(&ble_scan_params);
        if (scan_ret){
            ESP_LOGE(GATTC_TAG, "set scan params error, error code = %x", scan_ret);
        }
        */
        break;
        }
    case ESP_GATTC_CONNECT_EVT:{
        ESP_LOGI(GATTC_TAG, "ESP_GATTC_CONNECT_EVT conn_id %d, if %d", p_data->connect.conn_id, gattc_if);
        gl_profile_tab[PROFILE_A_APP_ID].conn_id = p_data->connect.conn_id;
        memcpy(gl_profile_tab[PROFILE_A_APP_ID].remote_bda, p_data->connect.remote_bda, sizeof(esp_bd_addr_t));
        ESP_LOGI(GATTC_TAG, "REMOTE BDA:");
        esp_log_buffer_hex(GATTC_TAG, gl_profile_tab[PROFILE_A_APP_ID].remote_bda, sizeof(esp_bd_addr_t));
        esp_err_t mtu_ret = esp_ble_gattc_send_mtu_req (gattc_if, p_data->connect.conn_id);
        if (mtu_ret){
            ESP_LOGE(GATTC_TAG, "config MTU error, error code = %x", mtu_ret);
        }
        break;
        }
    case ESP_GATTC_OPEN_EVT:{
        if (param->open.status != ESP_GATT_OK){
            ESP_LOGE(GATTC_TAG, "open failed, status %d", p_data->open.status);
            break;
        }
        ESP_LOGI(GATTC_TAG, "open success");
        break;}
    case ESP_GATTC_DIS_SRVC_CMPL_EVT:{
        if (param->dis_srvc_cmpl.status != ESP_GATT_OK){
            ESP_LOGE(GATTC_TAG, "discover service failed, status %d", param->dis_srvc_cmpl.status);
            break;
        }
        ESP_LOGI(GATTC_TAG, "discover service complete conn_id %d", param->dis_srvc_cmpl.conn_id);
        esp_ble_gattc_search_service(gattc_if, param->dis_srvc_cmpl.conn_id, &remote_filter_service_uuid);
        break;}
    case ESP_GATTC_CFG_MTU_EVT:{
        if (param->cfg_mtu.status != ESP_GATT_OK){
            ESP_LOGE(GATTC_TAG,"config mtu failed, error status = %x", param->cfg_mtu.status);
        }
        ESP_LOGI(GATTC_TAG, "ESP_GATTC_CFG_MTU_EVT, Status %d, MTU %d, conn_id %d", param->cfg_mtu.status, param->cfg_mtu.mtu, param->cfg_mtu.conn_id);
        break;
        }
    case ESP_GATTC_SEARCH_RES_EVT: {
        ESP_LOGI(GATTC_TAG, "SEARCH RES: conn_id = %x is primary service %d", p_data->search_res.conn_id, p_data->search_res.is_primary);
        ESP_LOGI(GATTC_TAG, "start handle %d end handle %d current handle value %d", p_data->search_res.start_handle, p_data->search_res.end_handle, p_data->search_res.srvc_id.inst_id);
        if (p_data->search_res.srvc_id.uuid.len == ESP_UUID_LEN_16 && p_data->search_res.srvc_id.uuid.uuid.uuid16 == BT_SERVICE_UUID) {
            ESP_LOGI(GATTC_TAG, "service found");
            get_server = true;
            gl_profile_tab[PROFILE_A_APP_ID].service_start_handle = p_data->search_res.start_handle;
            gl_profile_tab[PROFILE_A_APP_ID].service_end_handle = p_data->search_res.end_handle;
            ESP_LOGI(GATTC_TAG, "UUID16: %X", p_data->search_res.srvc_id.uuid.uuid.uuid16);
        }        
        break;
        }
    case ESP_GATTC_SEARCH_CMPL_EVT:{
        if (p_data->search_cmpl.status != ESP_GATT_OK){
            ESP_LOGE(GATTC_TAG, "search service failed, error status = %x", p_data->search_cmpl.status);
            break;
        }
        ESP_LOGI(GATTC_TAG, "ESP_GATTC_SEARCH_CMPL_EVT");
        if (get_server){
            uint16_t count = 0;
            esp_gatt_status_t status = esp_ble_gattc_get_attr_count( gattc_if,
                                                                        p_data->search_cmpl.conn_id,
                                                                        ESP_GATT_DB_CHARACTERISTIC,
                                                                        gl_profile_tab[PROFILE_A_APP_ID].service_start_handle,
                                                                        gl_profile_tab[PROFILE_A_APP_ID].service_end_handle,
                                                                        INVALID_HANDLE,
                                                                        &count);
            if (status != ESP_GATT_OK){
                ESP_LOGE(GATTC_TAG, "esp_ble_gattc_get_attr_count error");
                break;
            }

            if (count > 0 && count <= MAX_CHAR_COUNT){
                char_elem_result = (esp_gattc_char_elem_t *)malloc(sizeof(esp_gattc_char_elem_t) * count);
                if (!char_elem_result){
                    ESP_LOGE(GATTC_TAG, "gattc no mem");
                    break;
                }else{
                    status = esp_ble_gattc_get_all_char( gattc_if,
                                                            p_data->search_cmpl.conn_id,
                                                            gl_profile_tab[PROFILE_A_APP_ID].service_start_handle,
                                                            gl_profile_tab[PROFILE_A_APP_ID].service_end_handle,
                                                            char_elem_result,
                                                            &count,
                                                            0);
                    if (status != ESP_GATT_OK){
                        ESP_LOGE(GATTC_TAG, "esp_ble_gattc_get_all_char error");
                        free(char_elem_result);
                        char_elem_result = NULL;
                        break;
                    }

                    for (int i = 0; i < count; i++) {
                        ESP_LOGW(GATTC_TAG, "Characteristic found: handle = %d, properties = %d, uuid = %s",
                                            char_elem_result[i].char_handle,
                                            char_elem_result[i].properties,
                                            esp_log_uuid(&char_elem_result[i].uuid));
                        
                        // check len uuid len is 16
                        if((char_elem_result[i].uuid.len == ESP_UUID_LEN_16) && 
                            (char_elem_result[i].properties & ESP_GATT_CHAR_PROP_BIT_NOTIFY)){
                        
                                esp_err_t rg_status = esp_ble_gattc_register_for_notify(gattc_if, gl_profile_tab[PROFILE_A_APP_ID].remote_bda, char_elem_result[i].char_handle);
                                
                                // REGISTER UUID TO CHARS
                                gl_chars_info[i].conn_id = p_data->search_cmpl.conn_id;
                                gl_chars_info[i].char_handle = char_elem_result[i].char_handle;
                                gl_chars_info[i].char_uuid_16 = char_elem_result[i].uuid.uuid.uuid16;
                                gl_chars_info[i].gattc_if = gattc_if;
                                gl_chars_info[i].status = 0X00;// OK
                                if (rg_status !=ESP_OK){
                                    gl_chars_info[i].status = 0XFF;// FAIL
                                }
                            }
                        }
                    char_count = count;
                }
                free(char_elem_result);
            }else{
                ESP_LOGE(GATTC_TAG, "no char found or too many chars");
            }
        }
        break;
        }
    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
        ESP_LOGI(GATTC_TAG, "ESP_GATTC_REG_FOR_NOTIFY_EVT");
        if (p_data->reg_for_notify.status != ESP_GATT_OK){
            ESP_LOGE(GATTC_TAG, "REG FOR NOTIFY failed: error status = %d", p_data->reg_for_notify.status);
        } else {
            uint16_t notify_en = 1;
            for (int i = 0; i < char_count; i++) {
                uint16_t count = 0;
                esp_gatt_status_t ret_status = esp_ble_gattc_get_attr_count(gattc_if,
                                                                            gl_profile_tab[PROFILE_A_APP_ID].conn_id,
                                                                            ESP_GATT_DB_DESCRIPTOR,
                                                                            gl_profile_tab[PROFILE_A_APP_ID].service_start_handle,
                                                                            gl_profile_tab[PROFILE_A_APP_ID].service_end_handle,
                                                                            gl_chars_info[i].char_handle,
                                                                            &count);
                if (ret_status != ESP_GATT_OK){
                    ESP_LOGE(GATTC_TAG, "esp_ble_gattc_get_attr_count error for char handle %d", gl_chars_info[i].char_handle);
                    continue;
                }

                if (count > 0){
                    descr_elem_result = malloc(sizeof(esp_gattc_descr_elem_t) * count);
                    if (!descr_elem_result){
                        ESP_LOGE(GATTC_TAG, "malloc error, gattc no mem");
                        continue;
                    } else {
                        ret_status = esp_ble_gattc_get_descr_by_char_handle(gattc_if,
                                                                            gl_profile_tab[PROFILE_A_APP_ID].conn_id,
                                                                            gl_chars_info[i].char_handle,
                                                                            notify_descr_uuid,
                                                                            descr_elem_result,
                                                                            &count);
                        if (ret_status != ESP_GATT_OK){
                            ESP_LOGE(GATTC_TAG, "esp_ble_gattc_get_descr_by_char_handle error for char handle %d", gl_chars_info[i].char_handle);
                            free(descr_elem_result);
                            descr_elem_result = NULL;
                            continue;
                        }

                        if (count > 0 && descr_elem_result[0].uuid.len == ESP_UUID_LEN_16 && descr_elem_result[0].uuid.uuid.uuid16 == ESP_GATT_UUID_CHAR_CLIENT_CONFIG){
                            ret_status = esp_ble_gattc_write_char_descr(gattc_if,
                                                                        gl_profile_tab[PROFILE_A_APP_ID].conn_id,
                                                                        descr_elem_result[0].handle,
                                                                        sizeof(notify_en),
                                                                        (uint8_t *)&notify_en,
                                                                        ESP_GATT_WRITE_TYPE_RSP,
                                                                        ESP_GATT_AUTH_REQ_NONE);
                            if (ret_status != ESP_GATT_OK){
                                ESP_LOGE(GATTC_TAG, "esp_ble_gattc_write_char_descr error for char handle %d", gl_chars_info[i].char_handle);
                            }
                        }

                        free(descr_elem_result);
                    }
                } else {
                    ESP_LOGE(GATTC_TAG, "descriptor not found for char handle %d", gl_chars_info[i].char_handle);
                }
            }
        }
        break;
    }

    case ESP_GATTC_NOTIFY_EVT:
        
        ESP_LOGI(GATTC_TAG, " ");
        if (p_data->notify.is_notify){
            ESP_LOGI(GATTC_TAG, "ESP_GATTC_NOTIFY_EVT, receive notify value:");
        }else{
            ESP_LOGI(GATTC_TAG, "ESP_GATTC_NOTIFY_EVT, receive indicate value:");
        }
        ESP_LOGW(GATTC_TAG,"NOTITY HANDLE: %u, CONN ID: %u, LEN: %d",p_data->notify.handle, p_data->notify.conn_id,p_data->notify.value_len);
        // ESP_LOG_BUFFER_HEX_LEVEL(GATTC_TAG, p_data->notify.value, p_data->notify.value_len,ESP_LOG_WARN);

        // --- code to check notify value --- //
        mk115_parse_data(p_data->notify.value, p_data->notify.value_len);

        break;
    case ESP_GATTC_WRITE_DESCR_EVT:
        if (p_data->write.status != ESP_GATT_OK){
            ESP_LOGE(GATTC_TAG, "write descr failed, error status = %x", p_data->write.status);
            break;
        }

        break;
    case ESP_GATTC_SRVC_CHG_EVT: {
        esp_bd_addr_t bda;
        memcpy(bda, p_data->srvc_chg.remote_bda, sizeof(esp_bd_addr_t));
        ESP_LOGI(GATTC_TAG, "ESP_GATTC_SRVC_CHG_EVT, bd_addr:");
        esp_log_buffer_hex(GATTC_TAG, bda, sizeof(esp_bd_addr_t));
        break;
    }
    case ESP_GATTC_WRITE_CHAR_EVT:
        if (p_data->write.status != ESP_GATT_OK){
            ESP_LOGE(GATTC_TAG, "write char failed, error status = %x", p_data->write.status);
            break;
        }
        ESP_LOGI(GATTC_TAG, "write char success ");
        break;
    case ESP_GATTC_DISCONNECT_EVT:
        // connect = false;
        bt_gap_set_conn_state(false);
        get_server = false;
        ESP_LOGI(GATTC_TAG, "ESP_GATTC_DISCONNECT_EVT, reason = %d", p_data->disconnect.reason);
        break;
    default:
        break;
    }
}







static void esp_gattc_cb(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param)
{
    /* If event is register event, store the gattc_if for each profile */
    if (event == ESP_GATTC_REG_EVT) {
        if (param->reg.status == ESP_GATT_OK) {
            gl_profile_tab[param->reg.app_id].gattc_if = gattc_if;
        } else {
            ESP_LOGI(GATTC_TAG, "reg app failed, app_id %04x, status %d",
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



int gatt_write_bt_data( uint16_t uuid_char16, uint8_t *data, size_t length) {

    // foun id uuid char is register
    ESP_LOGI(GATTC_TAG, "WRITE DATA -> CHAR UUID16: %02X",uuid_char16);
    ESP_LOG_BUFFER_HEX(GATTC_TAG, data, length);

    for (size_t i = 0; i < char_count; i++)
    {
        // chek the uuid in char count
        if (uuid_char16==gl_chars_info[i].char_uuid_16){
            
            esp_err_t ret = esp_ble_gattc_write_char(gl_chars_info[i].gattc_if,
                                                    gl_chars_info[i].conn_id,
                                                    gl_chars_info[i].char_handle,
                                                    length,
                                                    data,
                                                    ESP_GATT_WRITE_TYPE_RSP,
                                                    ESP_GATT_AUTH_REQ_NONE);
            return ret;
        }
    }
    return -1; // fail to send or not found
}


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


esp_err_t init_gatt_client(){

    esp_err_t ret = 0;
    //register the callback function to the gattc module
    ret = esp_ble_gattc_register_callback(esp_gattc_cb);
    return ret; 
}

