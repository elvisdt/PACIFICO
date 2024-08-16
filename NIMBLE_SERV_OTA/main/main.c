#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <esp_event.h>

#include <esp_log.h>
#include "esp_ota_ops.h"
#include <nvs.h>
#include <nvs_flash.h>

/* ota ble librarias */
#include "gap.h"
#include "gatt_svr.h"

#include "ir_nec_encoder.h"
#include "ir_nec_main.h"

#include "main.h"
/* =====================================================================
 * DEFINITIONS
 * =====================================================================*/


#define TAG "MAIN"

/*----- define nvs-params--------*/
#define NAMESPACE_NVS	"storage"
#define KEY_NVS_IR_CTRL		"CTRL_LIST"


nvs_handle_t storage_nvs_handle;

/*--> external handle <--*/
QueueHandle_t ir_nec_quemain = NULL;
QueueHandle_t bt_wr_quemain  = NULL;


// global variable ctrl
static ir_nec_scan_t new_scan_ir ={0};
static ir_nec_list_t list_btn={0};
size_t size_l_ir = sizeof(ir_nec_list_t);

static bool active_scan_ntfy = false;


void Init_NVS_Keys(){
	/*------------------------------------------------*/
	ESP_LOGI(TAG,"Init NVS keys of data");

	esp_err_t err = nvs_flash_init();
	if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		err = nvs_flash_init();
	}
    ESP_ERROR_CHECK(err);
	
	ESP_LOGI(TAG,"NVS get WMBUS keys");
	nvs_open(NAMESPACE_NVS,NVS_READWRITE,&storage_nvs_handle);

	// Recuperar estructura ink_list_ble_addr_t
	err = nvs_get_blob(storage_nvs_handle, KEY_NVS_IR_CTRL, &list_btn, &size_l_ir);
	if(err == ESP_ERR_NVS_NOT_FOUND){
		memset(&list_btn,0,sizeof(list_btn));
		nvs_set_blob(storage_nvs_handle, KEY_NVS_IR_CTRL, &list_btn, sizeof(ir_nec_list_t));     
	}

    // Revisar el numero de botnes registrados
    if (list_btn.num_ctrl>=MAX_IR_CTRL_NUM)
    {
        list_btn.num_ctrl = MAX_IR_CTRL_NUM;
    }
    

    for (size_t i = 0; i < list_btn.num_ctrl ; i++)
    {
        ESP_LOGI("NVS","[%d] addr: %04X, cmd: %04X",
                i,
                list_btn.list_ir[i].nec.address,
                list_btn.list_ir[i].nec.command
                );
    }
    

	printf("------------------------\r\n");
	//-------------------------------------------------//
	ESP_LOGI(TAG,"NVS keys recovered succsesfull\r\n");
	return;
}


void array_ir_nec_scan(const ir_nec_scan_t *scan, uint8_t *buffer) {
    buffer[0] = (scan->nec.address >> 8) & 0xFF;
    buffer[1] = scan->nec.address & 0xFF;
    buffer[2] = (scan->nec.command >> 8) & 0xFF;
    buffer[3] = scan->nec.command & 0xFF;
    buffer[4] = (scan->time >> 24) & 0xFF;
    buffer[5] = (scan->time >> 16) & 0xFF;
    buffer[6] = (scan->time >> 8) & 0xFF;
    buffer[7] = scan->time & 0xFF;
}

void array_ir_nec_code(const ir_nec_scan_code_t *code, uint8_t *buffer) {
    buffer[0] = (code->address >> 8) & 0xFF;
    buffer[1] = code->address & 0xFF;
    buffer[2] = (code->command >> 8) & 0xFF;
    buffer[3] = code->command & 0xFF;
}


/**************************************************************
 * 
 **************************************************************/

int m_procces_bt_data_01(uint8_t *raw_data, size_t len_data){
    if (raw_data == NULL && len_data<3){
        return -1;
    }
    ESP_LOGI(TAG, "---- PROCCES BT DATA READ ----");
    uint8_t bt_func = raw_data[1];
    uint8_t length  = raw_data[2];

    // fail calculate leng data
    if (length != (len_data-3)){
        return -2;
    }
    
    uint8_t not_data[256];
    size_t  not_len =0;

    not_data[0] = BT_R_HEAD;    // cmd ans
    not_data[1] = bt_func;      // cmd type
    not_data[2] = not_len;      // len data

    int rc=0;   // return to send notify
    ESP_LOGW(TAG,"FUNCTION WRITE %02X", bt_func);
    ESP_LOG_BUFFER_HEX(TAG, raw_data , len_data);
    switch (bt_func){
        //leer todos los botones guardados
        case 0x01:
            not_data[2] = not_len;      // len data
            rc = cus_nec_gatt_notify_data(not_data,not_len+3);
            if (rc != 0) {
                ESP_LOGE("NEC", "Error sending notification; rc=%d", rc);
            }
            break;
        // leer ultimo valor 
        case 0x02:
            // LEER ULTIMO VALOR
            if (list_btn.num_ctrl>0 && list_btn.num_ctrl<=MAX_IR_CTRL_NUM){
                not_data[3] = list_btn.num_ctrl;  // num btn ctrl
                
                uint8_t idx = list_btn.num_ctrl-1;
                uint8_t code[4];
                array_ir_nec_code(&list_btn.list_ir[idx].nec, code);
                for (size_t i = 0; i < 4; i++)
                {
                    not_data[3+i] = code[i];
                }
                not_len = 5;
            }
            not_data[2] = not_len;      // len data
            rc = cus_nec_gatt_notify_data(not_data,not_len+3);
            if (rc != 0) {
                ESP_LOGE("NEC", "Error sending notification; rc=%d", rc);
            }
            break;
        
        // leer ultimo valor scaneado
        case 0x03:
            // LEER ULTIMO VALOR SCANEADO
            if (new_scan_ir.time != 0x00){
                
                uint8_t code_scan[8];
                array_ir_nec_scan(&new_scan_ir, code_scan);
                for (size_t i = 0; i < 8; i++)
                {
                    not_data[3+i] = code_scan[i];
                }
                not_len = 8;
            }
            not_data[2] = not_len;      // len data
            rc = cus_nec_gatt_notify_data(not_data,not_len+3);
            if (rc != 0) {
                ESP_LOGE("NEC", "Error sending notification; rc=%d", rc);
            }
            break;

        default:
            not_data[2] = not_len;      // len data
            rc = cus_nec_gatt_notify_data(raw_data,len_data);
            if (rc != 0) {
                ESP_LOGE("NEC", "Error sending notification; rc=%d", rc);
            }
            break;
    }
    
    return 0;
}



int m_procces_bt_data_02(uint8_t *raw_data, size_t len_data){
        if (raw_data == NULL && len_data<3){
        return -1;
    }
    ESP_LOGI(TAG, "---- PROCCES BT WRITE READ ----");
    uint8_t bt_func = raw_data[1];
    uint8_t length  = raw_data[2];

    // fail calculate leng data
    if (length != (len_data-3)){
        return -2;
    }
    const uint8_t *data = &raw_data[3];
    
    uint8_t not_data[256];
    size_t  not_len =0;

    not_data[0] = BT_W_HEAD;    // cmd ans
    not_data[1] = bt_func;      // cmd type
    not_data[2] = not_len;      // len data

    int rc=0;   // return to send notify
    uint8_t idx;
    ESP_LOGW(TAG,"FUNCTION READ %02X", bt_func);
    ESP_LOG_BUFFER_HEX(TAG, raw_data , len_data);


    switch (bt_func){
        case 0x01:
            // num report to scann 0 -> FF
            ESP_LOGI(TAG, "SCANN NOT: %02X",data[0]);
            if (data[0]==0x01){
                active_scan_ntfy = true;
                set_scan_ir_rep(true);
            }else if (data[0]==0x00){
                 active_scan_ntfy = false;
                set_scan_ir_rep(false);
            }
            break;

        case 0x02:
            ESP_LOGI(TAG, "CLEAR DATA: ");
            
            // eliminar valores almacennados
            memset(&list_btn, 0, sizeof(ir_nec_list_t));
            
            // actualizar NVS
            nvs_erase_key(storage_nvs_handle,KEY_NVS_IR_CTRL);
		    nvs_set_blob(storage_nvs_handle, KEY_NVS_IR_CTRL, &list_btn, sizeof(ir_nec_list_t));
		    nvs_commit(storage_nvs_handle); // guardar cambios de maenra permanente
            
            // retornar list_de btn
            not_data[3] = list_btn.num_ctrl;
            not_len = 1;

            not_data[2] = not_len;      // len data
            rc = cus_nec_gatt_notify_data(not_data,not_len +3);
            if (rc != 0) {
                ESP_LOGE("NEC", "Error sending notification; rc=%d", rc);
            }
            break;

        // agregar o eliminar el ultimo valor
        case 0x03:
            ESP_LOGI(TAG, "LAST VALUE, DEL OR ADD: %02X",data[0]);
            // eliminar 
            if (data[0]==0 && (list_btn.num_ctrl >0)){
                idx = list_btn.num_ctrl -1; 
                memset(&list_btn.list_ir[idx],0,sizeof(list_btn.list_ir[idx]));
                list_btn.num_ctrl --;
            }// agregar
            else if (data[0]==1 && (list_btn.num_ctrl<MAX_IR_CTRL_NUM)){
                idx = list_btn.num_ctrl;
                list_btn.list_ir[idx] = new_scan_ir; // ADD SACN IR
                list_btn.num_ctrl ++;
            }
            
            
            if (list_btn.num_ctrl>0 && list_btn.num_ctrl<=MAX_IR_CTRL_NUM){
                idx = list_btn.num_ctrl-1;
                not_data[3] = list_btn.num_ctrl;  // num btn ctrl

                uint8_t code[4];
                array_ir_nec_code(&list_btn.list_ir[idx].nec, code);
                for (size_t i = 0; i < 4; i++)
                {
                    not_data[3+i] = code[i];
                }
                not_len = 5;
            }

            // actualizar NVS
            nvs_erase_key(storage_nvs_handle,KEY_NVS_IR_CTRL);
		    nvs_set_blob(storage_nvs_handle, KEY_NVS_IR_CTRL, &list_btn, sizeof(ir_nec_list_t));
		    nvs_commit(storage_nvs_handle); // guardar cambios de maenra permanente
            
            not_data[2] = not_len;  // len data
            rc = cus_nec_gatt_notify_data(not_data,not_len+3);
            if (rc != 0) {
                ESP_LOGE("NEC", "Error sending notification; rc=%d", rc);
            }
            break;

        case 0x04:
            ESP_LOGI(TAG, "SCANN STATUS: %02X",data[0]);
            // reproducir control de la lista
            uint8_t num_btn = data[0];

            if (num_btn<list_btn.num_ctrl && num_btn>0){
                idx = num_btn-1;
                // PUSH BUTTON
                nec_sed_data_ctrl(list_btn.list_ir[num_btn-1].nec);
                
                // ENBLE RAW DATA
                
                not_data[3] = list_btn.num_ctrl;  // num btn ctrl

                uint8_t code[4];
                array_ir_nec_code(&list_btn.list_ir[idx].nec, code);
                for (size_t i = 0; i < 4; i++)
                {
                    not_data[3+i] = code[i];
                }
                not_len = 5;

            }
            not_data[2] = not_len;  // len data
            rc = cus_nec_gatt_notify_data(not_data,not_len+3);
            if (rc != 0) {
                ESP_LOGE("NEC", "Error sending notification; rc=%d", rc);
            }
            break;
        default:
            break;
    }
    
    return 0;
}




// Función para reiniciar el BLE
void init_ota_ble() {
    ESP_LOGW("BLE",">> INIT BLE OTA ..");
    int ret=0;

    ret = nimble_port_init();
    ESP_LOGI("BLE","ret init nimble: 0x%X\n",ret);

    ble_hs_cfg.sync_cb = sync_cb;
    ble_hs_cfg.reset_cb = reset_cb;

    // ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    gatt_svr_init();
    ble_svc_gap_device_name_set(device_name);
    nimble_port_freertos_init(host_task);
}

bool run_diagnostics() {
    // Verificar si se realizó una actualización OTA correctamente
    // Si no se detectó una actualización OTA, simplemente retornar verdadero
    return true;
}

void app_main(void)
{

    ESP_LOGW(TAG, "--->> INIT PROJECT <<---");
	esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);


    const esp_partition_t *partition = esp_ota_get_running_partition();
    switch (partition->address) {
        case 0x00010000:
            ESP_LOGI(TAG, "Running partition: factory");
            break;
        case 0x00110000:
            ESP_LOGI(TAG, "Running partition: ota_0");
            break;
        case 0x00210000:
            ESP_LOGI(TAG, "Running partition: ota_1");
            break;
        default:
            ESP_LOGE(TAG, "Running partition: unknown");
        break;
    }

    // check if an OTA has been done, if so run diagnostics
    esp_ota_img_states_t ota_state;
    if (esp_ota_get_state_partition(partition, &ota_state) == ESP_OK) {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
            ESP_LOGI(TAG, "An OTA update has been detected.");
            if (run_diagnostics()) {
                ESP_LOGI(TAG,"Diagnostics completed successfully! Continuing execution.");
                esp_ota_mark_app_valid_cancel_rollback();
            } else {
                ESP_LOGE(TAG,"Diagnostics failed! Start rollback to the previous version.");
                esp_ota_mark_app_invalid_rollback_and_reboot();
            }
        }
    }

    ir_nec_quemain = xQueueCreate(5, sizeof(ir_nec_scan_t));
    bt_wr_quemain  = xQueueCreate(5, sizeof(bt_msg_t));

    init_ota_ble();
    init_ir_nec_task();

    
    //uint8_t count = 0;
    static ir_nec_scan_t rec_data_nec={0};
    bt_msg_t msg_bt;
    uint8_t dscan[12];


    while (1)
    {
        if (xQueueReceive(ir_nec_quemain, &rec_data_nec, pdMS_TO_TICKS(1000)) == pdPASS){
            ESP_LOGI(TAG, "UPDATE VALUE TO SCANN");
            memcpy(&new_scan_ir, &rec_data_nec,sizeof(ir_nec_scan_t));

            if (get_ble_conn() && active_scan_ntfy==true){
                dscan[0] = BT_N_HEAD;
                dscan[1] = 0x00;
                dscan[2] = 0x00;// data len
                uint8_t ar_scan[8];
                
                array_ir_nec_scan(&new_scan_ir, ar_scan);
                for (int i = 0; i < 8; i++) {
                    dscan[3 + i] = ar_scan[i];
                }
                dscan[2] = 8; // Longitud total del mensaje
                
                ESP_LOG_BUFFER_HEX(TAG, dscan,11);
                int rc = cus_nec_gatt_notify_data(dscan, 11);
                if (rc != 0) {
                    ESP_LOGE("NEC", "Error sending notification; rc=%d", rc);
                }
            }
        }

        if (get_ble_conn()){
            // ESP_LOGI(TAG, "BLE IS CONN");
            if (xQueueReceive(bt_wr_quemain, &msg_bt, pdMS_TO_TICKS(1000)) == pdPASS){
                switch (msg_bt.array[0])
                {
                case 0x01:
                    m_procces_bt_data_01(msg_bt.array, msg_bt.length);
                    break;
                case 0x02:
                    m_procces_bt_data_02(msg_bt.array, msg_bt.length);
                    break;

                default:
                    break;
                } 
            }                      
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    vTaskDelete(NULL);
}