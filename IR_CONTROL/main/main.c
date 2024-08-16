/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include "stdio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

#include "nvs.h"
#include "nvs_flash.h"


#include "time.h"
#include "esp_timer.h"

#include "ir_nec_encoder.h"
#include "ir_nec_main.h"

#include "ble_main.h"

#define TAG "MAIN"


void app_main(void)
{
    ESP_LOGI(TAG, "Init NVS keys of data");
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    init_ir_nec_task();

    init_nimble_serv_app();

}
