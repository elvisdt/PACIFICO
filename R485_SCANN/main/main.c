#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <ctype.h>


#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <esp_event.h>

#include <esp_system.h>
#include <sdkconfig.h>
#include <sys/time.h>
#include <time.h>
#include <esp_timer.h>

#include "esp_spiffs.h"
#include <dirent.h>

#include <esp_log.h>
#include "esp_ota_ops.h"
#include <nvs.h>
#include <nvs_flash.h>
#include "driver/gpio.h"
#include "driver/uart.h"

#include "main.h"
#include "cus_spiffs.h"
#include "SD_card_control.h"


#define TAG "MAIN"





#define WAIT_MS(x) vTaskDelay(pdMS_TO_TICKS(x))
#define WAIT_S(x) vTaskDelay(pdMS_TO_TICKS(x * 1e3))


#define UART_SCANN  UART_NUM_1
#define UART_BAUD   9600
#define UART_TX     GPIO_NUM_36 // GPIO_NUM_47
#define UART_RX     GPIO_NUM_38 // GPIO_NUM_35
/*----------------------------------------------------------*/



/*---> External variables <--*/
QueueHandle_t uart_md_queue;

TaskHandle_t UART_task_handle = NULL;

int init_params(){
    ESP_LOGI(TAG, "--> UART CONFIG <--");
	uart_config_t uart_config_modem = {
		.baud_rate = UART_BAUD,
		.data_bits = UART_DATA_8_BITS,
		.parity = UART_PARITY_DISABLE,
		.stop_bits = UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
		.source_clk = UART_SCLK_DEFAULT,
	};

	// instalar el controlador UART
	esp_err_t ret = uart_driver_install(UART_SCANN,
										BUF_SIZE_MD * 2,
										BUF_SIZE_MD * 2,
										configMAX_PRIORITIES - 1,
										&uart_md_queue, 0);

	if (ret == ESP_FAIL)
	{
		// Si la instalación falla, se desinstala y vuelve a instalar
		ESP_ERROR_CHECK(uart_driver_delete(UART_SCANN));
		ESP_ERROR_CHECK(uart_driver_install(UART_SCANN,
											BUF_SIZE_MD * 2,
											BUF_SIZE_MD * 2,
											configMAX_PRIORITIES - 1,
											&uart_md_queue, 1));
	}

	// Configure UART parameters
	ESP_ERROR_CHECK(uart_param_config(UART_SCANN, &uart_config_modem));

	// Set UART pins as per KConfig settings
	ESP_ERROR_CHECK(uart_set_pin(UART_SCANN,
								 UART_TX,
								 UART_RX,
								 UART_PIN_NO_CHANGE,
								 UART_PIN_NO_CHANGE));

    return 0;
}

/*-----------------------------------------------------------*/
static void listSPIFFS(char * path) {
	DIR* dir = opendir(path);
	assert(dir != NULL);
	while (true) {
		struct dirent*pe = readdir(dir);
		if (!pe) break;
		ESP_LOGI(__FUNCTION__,"d_name=%s d_ino=%d d_type=%x", pe->d_name,pe->d_ino, pe->d_type);
	}
	closedir(dir);
}

void removeFile(char *path){
    FILE* archivo = fopen(path, "r");
    if (archivo) {
        fclose(archivo);
        if (remove(path) == 0) {
            ESP_LOGI(__FUNCTION__, "Archivo eliminado: %s", path);
        } else {
            ESP_LOGE(__FUNCTION__, "Error al eliminar el archivo: %s", path);
        }

    }else{
         ESP_LOGW(__FUNCTION__, "not found File: %s", path);
    }

    return;
}


esp_err_t mountSPIFFS(char * path, char * label, int max_files) {
	esp_vfs_spiffs_conf_t conf = {
		.base_path = path,
		.partition_label = label,
		.max_files = max_files,
		.format_if_mount_failed =true
	};

	// Use settings defined above toinitialize and mount SPIFFS filesystem.
	// Note: esp_vfs_spiffs_register is anall-in-one convenience function.
	esp_err_t ret = esp_vfs_spiffs_register(&conf);

	if (ret != ESP_OK) {
		if (ret ==ESP_FAIL) {
			ESP_LOGE(TAG, "Failed to mount or format filesystem");
		} else if (ret== ESP_ERR_NOT_FOUND) {
			ESP_LOGE(TAG, "Failed to find SPIFFS partition");
		} else {
			ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)",esp_err_to_name(ret));
		}
		return ret;
	}

	size_t total = 0, used = 0;
	ret = esp_spiffs_info(conf.partition_label, &total, &used);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG,"Failed to get SPIFFS partition information (%s)",esp_err_to_name(ret));
	} else {
		ESP_LOGI(TAG,"Mount %s to %s success", path, label);
		ESP_LOGI(TAG,"Partition size: total: %d, used: %d", total, used);
	}

	return ret;
}





/*---------------------------------------*/
#define BYTES_PER_LINE 16
void mc_log_data(const char *tag, const void *buffer, uint16_t buff_len)
{

    if (buff_len == 0) {
        return;
    }
    char temp_buffer[BYTES_PER_LINE + 3]; //for not-byte-accessible memory
    const char *ptr_line;
    //format: field[length]
    // ADDR[10]+"   "+DATA_HEX[8*3]+" "+DATA_HEX[8*3]+"  |"+DATA_CHAR[8]+"|"
    char hd_buffer[2 + sizeof(void *) * 2 + 3 + BYTES_PER_LINE * 3 + 1 + 3 + BYTES_PER_LINE + 1 + 1];
    char *ptr_hd;
    int bytes_cur_line;

    do {
        if (buff_len > BYTES_PER_LINE) {
            bytes_cur_line = BYTES_PER_LINE;
        } else {
            bytes_cur_line = buff_len;
        }
        if (true) {
            //use memcpy to get around alignment issue
            memcpy(temp_buffer, buffer, (bytes_cur_line + 3) / 4 * 4);
            ptr_line = temp_buffer;
        } else {
            ptr_line = buffer;
        }
        ptr_hd = hd_buffer;

        ptr_hd += sprintf(ptr_hd, "%p ", buffer);
        for (int i = 0; i < BYTES_PER_LINE; i ++) {
            if ((i & 7) == 0) {
                ptr_hd += sprintf(ptr_hd, " ");
            }
            if (i < bytes_cur_line) {
                ptr_hd += sprintf(ptr_hd, " %02x", (unsigned char) ptr_line[i]);
            } else {
                ptr_hd += sprintf(ptr_hd, "   ");
            }
        }
        ptr_hd += sprintf(ptr_hd, "  |");
        for (int i = 0; i < bytes_cur_line; i ++) {
            if (isprint((int)ptr_line[i])) {
                ptr_hd += sprintf(ptr_hd, "%c", ptr_line[i]);
            } else {
                ptr_hd += sprintf(ptr_hd, ".");
            }
        }
        ptr_hd += sprintf(ptr_hd, "|");

        ESP_LOGI(tag, "%s", hd_buffer);
        sd_card_csv_append_data(FILE_NAME_UP_BOTONERA, hd_buffer);

        buffer += bytes_cur_line;
        buff_len -= bytes_cur_line;
    } while (buff_len);


    return;
}




static void scann_rx_task(void *pvParameters)
{
    uart_event_t event;
    uint8_t *dtmp = (uint8_t *)malloc(RD_BUF_SIZE);
    char data_time_str[100]={0};
    for (;;)
    {
        if (xQueueReceive(uart_md_queue, (void *)&event, portMAX_DELAY))
        {
            bzero(dtmp, RD_BUF_SIZE);
            switch (event.type)
            {
            case UART_DATA:
                // ESP_LOGI("UART", "[UART DATA]: %d", event.size);
                uart_read_bytes(UART_SCANN, dtmp, event.size, portMAX_DELAY);
                // ESP_LOG_BUFFER_HEXDUMP("UART", dtmp, event.size, ESP_LOG_INFO);
                mc_log_data("HEX", dtmp, event.size);
                time_t date = time(NULL);
                sprintf(data_time_str,"Time: %lld-------\n",date);
                ESP_LOGI(TAG,"%s",data_time_str);

                sd_card_csv_append_data(FILE_NAME_UP_BOTONERA, data_time_str);

                
                
                //  procesar data
                break;
            case UART_FIFO_OVF:
                ESP_LOGE("UART", "hw fifo overflow");
                uart_flush_input(UART_SCANN);
                xQueueReset(uart_md_queue);
                break;
            case UART_BUFFER_FULL:
                ESP_LOGI(TAG, "ring buffer full");
                // If buffer full happened, you should consider increasing your buffer size
                uart_flush_input(UART_SCANN);
                xQueueReset(uart_md_queue);
                break;

            default:
                break;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    free(dtmp);
    dtmp = NULL;
    vTaskDelete(NULL);
}

void app_main(void)
{
    ESP_LOGW(TAG, "--->> INIT PROJECT <<---");
    // Initialize NVS
	ESP_LOGI(TAG, "Initialize NVS");
	esp_err_t err = nvs_flash_init();
	if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		err = nvs_flash_init();
	}
	ESP_ERROR_CHECK( err );


    ESP_LOGI(TAG, "Initializing SPIFFS");
    esp_err_t ret;
	
    ret = mountSPIFFS("/"DIR_FILE"", LABEL_SPIFFS, 5);
	if (ret != ESP_OK) return;
	listSPIFFS("/"DIR_FILE"/");
    // cus_nvs_read_data_txt(FILE_PATH_DATA);

    ESP_LOGI(TAG, "INIT SD TASK CONTROL ");
    sd_init_gpios();

    vTaskDelay(pdMS_TO_TICKS(1000));
    int conn_sd = sd_get_status_conn();
    if (conn_sd != 0)
    {
        ESP_LOGE(TAG, "SD CARD NOT FOUND");
        vTaskDelete(NULL);
        return;
    }
    sd_set_power(1); // power ON
    ret = sd_card_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize SD card. Error = %d", ret);
        // sd_card_deinit();
        sd_set_power(0); // // power OFF
        vTaskDelete(NULL);
        return;
    }else
    {
        ESP_LOGI(TAG, "SD CARD INIT OK");
    }
    
    ret = sd_card_csv_write_header(FILE_NAME_UP_BOTONERA,"--------------BOTONERA DATA------------\n");
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed header DATA UP. Error = %d", ret);
    }

    // sd_read_text_file(FILE_NAME_UP_DATA);
    /*--- read and delete --*/
    //sd_card_csv_append_data(FILE_NAME_UP_BOTONERA, "***************CORTINAS(2,1-3,1)***********");
    init_params();
    //xTaskCreate(scann_rx_task, "M95_rx_task", 1024 * 10, NULL, configMAX_PRIORITIES - 1, &UART_task_handle); // active service
    
    

    ESP_LOGW(TAG, "END CONFIG");
}