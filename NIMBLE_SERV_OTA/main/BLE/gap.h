#pragma once

#include "esp_log.h"
#include "esp_nimble_hci.h"
#include "host/ble_hs.h"
#include "nimble/ble.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"

#define LOG_TAG_GAP "gap"

static const char device_name[] = "esp-ctrl";
extern uint16_t g_conn_handle;

bool get_ble_conn();


void advertise();
void reset_cb(int reason);
void sync_cb(void);
void host_task(void *param);