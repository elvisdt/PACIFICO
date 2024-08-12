/*
 * ota_esp32.c
 *
 * Created on: 3 set. 2022
 * Author: Franco
 */

#include "ota_esp32.h"
#include "esp_task_wdt.h"

static esp_ota_handle_t otaHandlerXXX = 0;

HAL_StatusTypeDef write_data_to_flash_app(uint8_t *data,
                                          uint16_t data_len,
                                          bool is_first_block,
                                          ota_t *ota)
{
    HAL_StatusTypeDef ret = HAL_OK;
    do
    {
        if (is_first_block)
        {
            otaHandlerXXX = 0;
            debug_ota("Begin - OTA");
            const esp_task_wdt_config_t config_wd = {
                .timeout_ms = 20,
                .idle_core_mask = 0,
                .trigger_panic = false,
            };
            esp_task_wdt_init(&config_wd);
            esp_ota_begin(esp_ota_get_next_update_partition(NULL), OTA_SIZE_UNKNOWN, &otaHandlerXXX);
        }

        if (data_len > 0)
        {
            esp_ota_write(otaHandlerXXX, (const void *)&data[4], data_len);
            ota->received_size += data_len;

            if (ota->received_size == ota->total_size)
            {
                esp_ota_end(otaHandlerXXX);
                const esp_task_wdt_config_t config_wd = {
                    .timeout_ms = 15,
                    .idle_core_mask = 0,
                    .trigger_panic = false,
                };
                esp_task_wdt_init(&config_wd);
                debug_ota("End - OTA");
                if (ESP_OK == esp_ota_set_boot_partition(esp_ota_get_next_update_partition(NULL)))
                {
                    ret = HAL_OK;
                }
                else
                {
                    debug_ota("Upload Error");
                    ret = HAL_ERROR;
                }
            }
        }
    } while (false);

    return ret;
}
