static void ble_chek_task(void *pvParameters)
{

    ESP_LOGI("BLE", "------------INIT BLE TASK-----------------");
    WAIT_S(2);
    
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
  
    WAIT_S(3);
    vTaskDelete(NULL);
}