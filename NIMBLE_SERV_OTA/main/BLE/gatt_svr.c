#include "gatt_svr.h"
#include "gap.h"

#define LEN_BUFF_BLE    512

uint8_t gatt_svr_chr_ota_control_val;
uint8_t gatt_svr_chr_ota_data_val[512];
uint8_t gatt_svr_chr_nec_data_val[LEN_BUFF_BLE]; // data to nec control data

uint16_t ota_control_val_handle;
uint16_t ota_data_val_handle;
uint16_t nec_ctrl_val_handle; // add nec handle


const esp_partition_t *update_partition;
esp_ota_handle_t update_handle;
uint16_t num_pkgs_received = 0;
uint16_t packet_size = 0;

/*---> EXTERNAL VARIBLE <--*/
bool ota_updating = false;


static const char *manuf_name = "LABOTEC";
static const char *model_num = "ESP32-V01";

static int gatt_svr_chr_write(struct os_mbuf *om, uint16_t min_len,
                              uint16_t max_len, void *dst, uint16_t *len);

static int gatt_svr_chr_ota_control_cb(uint16_t conn_handle,
                                       uint16_t attr_handle,
                                       struct ble_gatt_access_ctxt *ctxt,
                                       void *arg);

static int gatt_svr_chr_ota_data_cb(uint16_t conn_handle, uint16_t attr_handle,
                                    struct ble_gatt_access_ctxt *ctxt,
                                    void *arg);

static int gatt_svr_chr_nec_data_cb(uint16_t conn_handle, uint16_t attr_handle,
                                    struct ble_gatt_access_ctxt *ctxt,
                                    void *arg);


static int gatt_svr_chr_access_device_info(uint16_t conn_handle,
                                           uint16_t attr_handle,
                                           struct ble_gatt_access_ctxt *ctxt,
                                           void *arg);

static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    {// Service: Device Information
     .type = BLE_GATT_SVC_TYPE_PRIMARY,
     .uuid = BLE_UUID16_DECLARE(GATT_DEVICE_INFO_UUID),
     .characteristics =
         (struct ble_gatt_chr_def[]){
             {
                 // Characteristic: Manufacturer Name
                 .uuid = BLE_UUID16_DECLARE(GATT_MANUFACTURER_NAME_UUID),
                 .access_cb = gatt_svr_chr_access_device_info,
                 .flags = BLE_GATT_CHR_F_READ,
             },
             {
                 // Characteristic: Model Number
                 .uuid = BLE_UUID16_DECLARE(GATT_MODEL_NUMBER_UUID),
                 .access_cb = gatt_svr_chr_access_device_info,
                 .flags = BLE_GATT_CHR_F_READ,
             },
             {
                 0,
             },
         }},

    {
        // service: OTA Service
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &gatt_svr_svc_ota_uuid.u,
        .characteristics =
            (struct ble_gatt_chr_def[]){
                {
                    // characteristic: OTA control
                    .uuid = &gatt_svr_chr_ota_control_uuid.u,
                    .access_cb = gatt_svr_chr_ota_control_cb,
                    .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_NOTIFY,
                    .val_handle = &ota_control_val_handle,
                },
                {
                    // characteristic: OTA data
                    .uuid = &gatt_svr_chr_ota_data_uuid.u,
                    .access_cb = gatt_svr_chr_ota_data_cb,
                    .flags = BLE_GATT_CHR_F_WRITE,
                    .val_handle = &ota_data_val_handle,
                },
                {
                    // characteristic: CTRL NEC data
                    .uuid = &gatt_svr_chr_ctrl_nec_data_uuid.u,
                    .access_cb = gatt_svr_chr_nec_data_cb,
                    .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_NOTIFY,
                    .val_handle = &nec_ctrl_val_handle,
                },
                {
                    0,
                }},
    },

    {
        0,
    },
};

static int gatt_svr_chr_access_device_info(uint16_t conn_handle,
                                           uint16_t attr_handle,
                                           struct ble_gatt_access_ctxt *ctxt,
                                           void *arg) {
  uint16_t uuid;
  int rc;

  uuid = ble_uuid_u16(ctxt->chr->uuid);

  if (uuid == GATT_MODEL_NUMBER_UUID) {
    rc = os_mbuf_append(ctxt->om, model_num, strlen(model_num));
    return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
  }

  if (uuid == GATT_MANUFACTURER_NAME_UUID) {
    rc = os_mbuf_append(ctxt->om, manuf_name, strlen(manuf_name));
    return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
  }

  assert(0);
  return BLE_ATT_ERR_UNLIKELY;
}

static int gatt_svr_chr_write(struct os_mbuf *om, uint16_t min_len,
                              uint16_t max_len, void *dst, uint16_t *len) {
  uint16_t om_len;
  int rc;

  om_len = OS_MBUF_PKTLEN(om);
  if (om_len < min_len || om_len > max_len) {
    return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
  }

  rc = ble_hs_mbuf_to_flat(om, dst, max_len, len);
  if (rc != 0) {
    return BLE_ATT_ERR_UNLIKELY;
  }

  return 0;
}

static void update_ota_control(uint16_t conn_handle) {
  struct os_mbuf *om;
  esp_err_t err;

  // check which value has been received
  switch (gatt_svr_chr_ota_control_val) {
    case SVR_CHR_OTA_CONTROL_REQUEST:
      // OTA request
      ESP_LOGI(LOG_TAG_GATT_SVR, "OTA has been requested via BLE.");
      // get the next free OTA partition
      update_partition = esp_ota_get_next_update_partition(NULL);
      // start the ota update
      err = esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES,
                          &update_handle);
      if (err != ESP_OK) {
        ESP_LOGE(LOG_TAG_GATT_SVR, "esp_ota_begin failed (%s)",
                 esp_err_to_name(err));
        esp_ota_abort(update_handle);
        gatt_svr_chr_ota_control_val = SVR_CHR_OTA_CONTROL_REQUEST_NAK;
      } else {
        gatt_svr_chr_ota_control_val = SVR_CHR_OTA_CONTROL_REQUEST_ACK;
        ota_updating = true;

        // retrieve the packet size from OTA data
        packet_size = (gatt_svr_chr_ota_data_val[1] << 8) + gatt_svr_chr_ota_data_val[0];
        ESP_LOGI(LOG_TAG_GATT_SVR, "Packet size is: %d", packet_size);

        num_pkgs_received = 0;
      }

      // notify the client via BLE that the OTA has been acknowledged (or not)
      om = ble_hs_mbuf_from_flat(&gatt_svr_chr_ota_control_val,
                                 sizeof(gatt_svr_chr_ota_control_val));
      ble_gattc_notify_custom(conn_handle, ota_control_val_handle, om);
      ESP_LOGI(LOG_TAG_GATT_SVR, "OTA request acknowledgement has been sent.");

      break;

    case SVR_CHR_OTA_CONTROL_DONE:

      ota_updating = false;

      // end the OTA and start validation
      err = esp_ota_end(update_handle);
      if (err != ESP_OK) {
        if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
          ESP_LOGE(LOG_TAG_GATT_SVR,
                   "Image validation failed, image is corrupted!");
        } else {
          ESP_LOGE(LOG_TAG_GATT_SVR, "esp_ota_end failed (%s)!",
                   esp_err_to_name(err));
        }
      } else {
        // select the new partition for the next boot
        err = esp_ota_set_boot_partition(update_partition);
        if (err != ESP_OK) {
          ESP_LOGE(LOG_TAG_GATT_SVR, "esp_ota_set_boot_partition failed (%s)!",
                   esp_err_to_name(err));
        }
      }



      // set the control value
      if (err != ESP_OK) {
        gatt_svr_chr_ota_control_val = SVR_CHR_OTA_CONTROL_DONE_NAK;
      } else {
        gatt_svr_chr_ota_control_val = SVR_CHR_OTA_CONTROL_DONE_ACK;
      }

      // notify the client via BLE that DONE has been acknowledged
      om = ble_hs_mbuf_from_flat(&gatt_svr_chr_ota_control_val,
                                 sizeof(gatt_svr_chr_ota_control_val));
      ble_gattc_notify_custom(conn_handle, ota_control_val_handle, om);
      ESP_LOGI(LOG_TAG_GATT_SVR, "OTA DONE acknowledgement has been sent.");

      // restart the ESP to finish the OTA
      if (err == ESP_OK) {
        ESP_LOGI(LOG_TAG_GATT_SVR, "Preparing to restart!");
        vTaskDelay(pdMS_TO_TICKS(REBOOT_DEEP_SLEEP_TIMEOUT));
        esp_restart();
      }

      break;

    default:
      break;
  }
}

static int gatt_svr_chr_ota_control_cb(uint16_t conn_handle,
                                       uint16_t attr_handle,
                                       struct ble_gatt_access_ctxt *ctxt,
                                       void *arg) {
  int rc;
  uint8_t length = sizeof(gatt_svr_chr_ota_control_val);

  switch (ctxt->op) {
    case BLE_GATT_ACCESS_OP_READ_CHR:
      // a client is reading the current value of ota control
      rc = os_mbuf_append(ctxt->om, &gatt_svr_chr_ota_control_val, length);
      return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
      break;

    case BLE_GATT_ACCESS_OP_WRITE_CHR:
      // a client is writing a value to ota control
      rc = gatt_svr_chr_write(ctxt->om, 1, length,
                              &gatt_svr_chr_ota_control_val, NULL);
      // update the OTA state with the new value
      update_ota_control(conn_handle);
      return rc;
      break;

    default:
      break;
  }

  // this shouldn't happen
  assert(0);
  return BLE_ATT_ERR_UNLIKELY;
}

static int gatt_svr_chr_ota_data_cb(uint16_t conn_handle, uint16_t attr_handle,
                                    struct ble_gatt_access_ctxt *ctxt,
                                    void *arg) {
  int rc;
  esp_err_t err;

  // store the received data into gatt_svr_chr_ota_data_val
  rc = gatt_svr_chr_write(ctxt->om, 1, sizeof(gatt_svr_chr_ota_data_val),
                          gatt_svr_chr_ota_data_val, NULL);

  // write the received packet to the partition
  if (ota_updating) {
    err = esp_ota_write(update_handle, (const void *)gatt_svr_chr_ota_data_val,
                        packet_size);
    if (err != ESP_OK) {
      ESP_LOGE(LOG_TAG_GATT_SVR, "esp_ota_write failed (%s)!",
               esp_err_to_name(err));
    }

    num_pkgs_received++;
    ESP_LOGI(LOG_TAG_GATT_SVR, "Received packet %d", num_pkgs_received);
  }

  return rc;
}

/// @brief callback to NEC CONTROL
/// @param conn_handle 
/// @param attr_handle 
/// @param ctxt 
/// @param arg 
/// @return 
static int gatt_svr_chr_nec_data_cb(uint16_t conn_handle, uint16_t attr_handle,
                                    struct ble_gatt_access_ctxt *ctxt,
                                    void *arg) {
    int rc;
    // uint8_t length = sizeof(gatt_svr_chr_ota_control_val);
    

    ESP_LOGW("NEC", "EVENT %d", ctxt->op);

    switch (ctxt->op) {
    case BLE_GATT_ACCESS_OP_READ_CHR:
        // a client is reading the current value 
        uint8_t data_nec_read[] = {0xAA,0XBB, 0xCC};
        rc = os_mbuf_append(ctxt->om, data_nec_read, sizeof(data_nec_read));
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        break;

    case BLE_GATT_ACCESS_OP_WRITE_CHR:
        // a client is writing a value
        uint16_t len_write=0;
        rc = gatt_svr_chr_write(ctxt->om, 1, LEN_BUFF_BLE, gatt_svr_chr_nec_data_val, &len_write);
        ESP_LOGI("NEC", "Data written: ");
        ESP_LOG_BUFFER_HEXDUMP("NEC", gatt_svr_chr_nec_data_val,len_write, LOG_LEVEL_INFO);
        
        if (rc == 0) {
          
          uint8_t *myArray = (uint8_t *)malloc(len_write * sizeof(uint8_t));
          if (myArray == NULL) {
              // Error al asignar memoria
              break;
          }

          memcpy(myArray,&gatt_svr_chr_nec_data_val,len_write);

          // Crear el mensaje
          bt_msg_t message;
          message.array = myArray;
          message.length = len_write;
          
          if (xQueueSend(bt_wr_quemain, &message, pdMS_TO_TICKS(10)) != pdPASS){
              ESP_LOGE("WR-BT","FAIL SEND");
              // Error al enviar el mensaje
              free(myArray); // Liberar la memoria si no se pudo enviar
          };
          
        }
        return rc;
        break;

    default:
        break;
    }

    // this shouldn't happen
    assert(0);
    return BLE_ATT_ERR_UNLIKELY;
}



void gatt_svr_init() {
    ble_svc_gap_init();
    ble_svc_gatt_init();
    ble_gatts_count_cfg(gatt_svr_svcs);
    ble_gatts_add_svcs(gatt_svr_svcs);
}



/*========================================================*/

int cus_nec_gatt_notify_data(uint8_t *data, size_t len_data){

    // Preparar la notificacion
    struct os_mbuf *om = ble_hs_mbuf_from_flat(data, len_data);

    // Enviar la notificación al cliente
    int rc = ble_gattc_notify_custom(g_conn_handle, nec_ctrl_val_handle, om);

    return rc;
}