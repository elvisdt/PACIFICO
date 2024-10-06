#pragma once

#include "esp_ota_ops.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"


/****************************************************
 * DEFINES
*****************************************************/
#define LOG_TAG_GATT_SVR "gatt_svr"
#define REBOOT_DEEP_SLEEP_TIMEOUT   500
#define GATT_DEVICE_INFO_UUID       0x180A
#define GATT_MANUFACTURER_NAME_UUID 0x2A29
#define GATT_MODEL_NUMBER_UUID      0x2A24


// OTA CONTROL AND NEC
#define BT_CUSTOM_SERVICE_UUID     0xAA00

#define BT_CHAR_UUID_OTA_CTRL      0xAA01 //0xAA01 
#define BT_CHAR_UUID_OTA_DATA      0xAA02 //0xAA02 
#define BT_CHAR_UUID_NEC_INFO      0xAA03 //0xAA03


/*--> EXTERNAL VARIALBLE <--*/
extern  bool ota_updating;
extern QueueHandle_t bt_wr_quemain;


/****************************************************
 * ESTRUCUTURES
*****************************************************/

typedef struct {
    uint8_t *array;
    size_t length;
} bt_msg_t;



typedef enum {
  SVR_CHR_OTA_CONTROL_NOP,
  SVR_CHR_OTA_CONTROL_REQUEST,
  SVR_CHR_OTA_CONTROL_REQUEST_ACK,
  SVR_CHR_OTA_CONTROL_REQUEST_NAK,
  SVR_CHR_OTA_CONTROL_DONE,
  SVR_CHR_OTA_CONTROL_DONE_ACK,
  SVR_CHR_OTA_CONTROL_DONE_NAK,
} svr_chr_ota_control_val_t;

// service: OTA Service
// f505f04b-2066-5069-8775-830fcfc57339
static const ble_uuid16_t gatt_svr_svc_ota_uuid = BLE_UUID16_INIT(BT_CUSTOM_SERVICE_UUID);
 

// characteristic: OTA Control
static const ble_uuid16_t gatt_svr_chr_ota_control_uuid = BLE_UUID16_INIT(BT_CHAR_UUID_OTA_CTRL);

// characteristic: OTA Data
static const ble_uuid16_t gatt_svr_chr_ota_data_uuid = BLE_UUID16_INIT(BT_CHAR_UUID_OTA_DATA);


static const ble_uuid16_t gatt_svr_chr_ctrl_nec_data_uuid = BLE_UUID16_INIT(BT_CHAR_UUID_NEC_INFO);

void gatt_svr_init();


int cus_nec_gatt_notify_data(uint8_t *data, size_t len_data);