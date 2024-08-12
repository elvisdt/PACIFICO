#include "SD_card_control.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

#include "esp_vfs_fat.h"
#include "diskio.h"

#include "main.h"
#define MOUNT_POINT "/sdcard"


static const char *TAG = "SD_CARD";

void sd_init_gpios()
{
    /*---init gpio config---*/
    gpio_reset_pin(SD_PIN_DETEC);
    gpio_reset_pin(SD_PIN_PWR);
    gpio_set_direction(SD_PIN_DETEC, GPIO_MODE_INPUT);
    gpio_set_direction(SD_PIN_PWR, GPIO_MODE_OUTPUT);
    gpio_set_level(SD_PIN_PWR, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    return;
}

// 0 -> OK    1 -> FAIL
int sd_get_status_conn()
{
    vTaskDelay(pdMS_TO_TICKS(10));
    return gpio_get_level(SD_PIN_DETEC);
}

// 0 -> OFF    1 -> ON
void sd_set_power(uint32_t level)
{
    gpio_set_level(SD_PIN_PWR, level);
    vTaskDelay(pdMS_TO_TICKS(500));
    return;
}

esp_err_t sd_card_init(void)
{
    esp_err_t ret;
    // Initialize SD card and mount FAT filesystem
    // Mount configuration
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = true,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024};
    ESP_LOGI(TAG, "Initializing SD card");
    ESP_LOGI(TAG, "Using SPI peripheral");

    sdmmc_card_t *card;
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = SD_PIN_MOSI,
        .miso_io_num = SD_PIN_MISO,
        .sclk_io_num = SD_PIN_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };

    ret = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize SPI bus %d", ret);
        return ret;
    }

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = SD_PIN_CS;
    slot_config.host_id = host.slot;

    ret = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &host, &slot_config, &mount_config, &card);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to mount filesystem");
        return ret;
    }

    // Card has been initialized, print its properties
    sdmmc_card_print_info(stdout, card);

    return ESP_OK;
}

esp_err_t sd_card_deinit(void)
{
    /*
    esp_err_t ret;

    // Desmonta el sistema de archivos
    ret = esp_vfs_fat_sdspi_mount();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error al desmontar el sistema de archivos: %d", ret);
        return ret;
    }

    // Libera el bus SPI
    ret = spi_bus_free(SPI3_HOST);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error al liberar el bus SPI: %d", ret);
        return ret;
    }

    */
    return ESP_OK;
}


esp_err_t sd_card_csv_write_header(const char *filename, const char *header)
{
    char filepath[64];
    snprintf(filepath, sizeof(filepath), "%s/%s", MOUNT_POINT, filename);

    FILE *f = fopen(filepath, "r+");
    if (f == NULL)
    {
        // The file doesn't exist, create a new one
        f = fopen(filepath, "w");
        if (f == NULL)
        {
            ESP_LOGE(TAG, "Failed to create the file");
            return ESP_FAIL;
        }
        fprintf(f, "%s\n", header); // Write the header
    }
    else
    {
        // The file already exists, check if the header matches
        char current_header[MAX_CHAR_SIZE_SD];
        if (fgets(current_header, sizeof(current_header), f))
        {
            if (strcmp(current_header, header) != 0)
            {
                ESP_LOGW(TAG, "Header mismatch, updating header...");
                fseek(f, 0, SEEK_SET);
                fprintf(f, "%s\n", header);
            }
        }
    }

    fflush(f); // Ensure changes are written to disk
    fclose(f);
    ESP_LOGI(TAG, "Header written to %s", filepath);
    return ESP_OK;
}



esp_err_t sd_card_csv_append_data(const char *filename, const char *data)
{
    char filepath[64];
    snprintf(filepath, sizeof(filepath), "%s/%s", MOUNT_POINT, filename);

    FILE *f = fopen(filepath, "a+");
    if (f == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file for appending data");
        return ESP_FAIL;
    }

    // Escribir los datos al final del archivo
    fprintf(f, "%s\n", data);
    fclose(f);

    ESP_LOGI(TAG, "Data appended to %s", filepath);
    return ESP_OK;
}

esp_err_t sd_card_csv_read_and_process(const char *filename)
{
    char filepath[64];
    snprintf(filepath, sizeof(filepath), "%s/%s", MOUNT_POINT, filename);

    FILE *f = fopen(filepath, "r");
    if (f == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file for reading");
        return ESP_FAIL;
    }

    char line[MAX_CHAR_SIZE_SD];
    while (fgets(line, sizeof(line), f))
    {
        // Procesar cada línea (e.g., parsear datos CSV)
        // Ejemplo: Dividir la línea en tokens usando strtok() y procesar los datos
        m_replace_char(line, ',', '|');
        ESP_LOGI("DATA", "%s", line);
    }

    fclose(f);
    return ESP_OK;
}

esp_err_t sd_card_csv_clear_file(const char *filename)
{
    char filepath[64];
    snprintf(filepath, sizeof(filepath), "%s/%s", MOUNT_POINT, filename);

    FILE *f = fopen(filepath, "w");
    if (f == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file for clearing");
        return ESP_FAIL;
    }

    fclose(f);
    ESP_LOGI(TAG, "File %s cleared", filepath);
    return ESP_OK;
}

esp_err_t sd_card_csv_delete_line(const char *filename, int line_to_delete)
{
    char filepath[64];
    snprintf(filepath, sizeof(filepath), "%s/%s", MOUNT_POINT, filename);

    FILE *f_in = fopen(filepath, "r");
    if (f_in == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file for reading");
        return ESP_FAIL;
    }

    char tmp_filepath[64];
    snprintf(tmp_filepath, sizeof(tmp_filepath), "%s/tmp_%s", MOUNT_POINT, filename);
    FILE *f_out = fopen(tmp_filepath, "w");
    if (f_out == NULL)
    {
        fclose(f_in);
        ESP_LOGE(TAG, "Failed to create temporary file for writing");
        return ESP_FAIL;
    }

    char line[MAX_CHAR_SIZE_SD];
    int current_line = 0;
    while (fgets(line, sizeof(line), f_in))
    {
        current_line++;
        if (current_line != line_to_delete)
        {
            fputs(line, f_out);
        }
    }

    fclose(f_in);
    fclose(f_out);

    // Reemplazar el archivo original con el temporal
    if (rename(tmp_filepath, filepath) != 0)
    {
        ESP_LOGE(TAG, "Failed to rename temporary file");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Deleted line %d from file %s", line_to_delete, filepath);
    return ESP_OK;
}

esp_err_t sd_card_csv_read_specific_line(const char *filename, int line_number, char *buffer, size_t buffer_size)
{
    char filepath[64];
    snprintf(filepath, sizeof(filepath), "%s/%s", MOUNT_POINT, filename);

    FILE *f = fopen(filepath, "r");
    if (f == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file for reading");
        return ESP_FAIL;
    }

    int current_line = 0;
    while (fgets(buffer, buffer_size, f))
    {
        current_line++;
        if (current_line == line_number)
        {
            // Eliminar el salto de línea final si existe
            char *pos = strchr(buffer, '\n');
            if (pos)
            {
                *pos = '\0';
            }
            fclose(f);
            return ESP_OK;
        }
    }

    fclose(f);
    ESP_LOGE(TAG, "Line %d not found in file %s", line_number, filepath);
    return ESP_FAIL;
}


/*******************************************************************************************
 * EXTRA FUNCTIONS 
 *******************************************************************************************/


void sd_generate_str_head(event_sd_t event,char *buffer, size_t buffer_size) {
    // Índice para el buffer
    if (buffer == NULL || buffer_size == 0) {
        // Handle invalid input parameters
        return;
    }
    switch (event)
    {
    case SD_EVENT_TABLERO:
        // "DateTime,C01,C02....C30"
        snprintf(buffer, buffer_size, "%s", "DateTime");
        for (int i = 30; i >= 1; --i) {
            snprintf(buffer + strlen(buffer), buffer_size - strlen(buffer), ",C%02d", i);
        }
        break;
    case SD_EVENT_ICM_01:
        //MSN,testN,testR,DateTime,Temperature,RH,Flow,ISO_code,Tcount4,Tcoun6,Tcount14
        snprintf(buffer, buffer_size, "DateTime,MSN,testN,testR,Temperature,RH,Flow,ISO_code,Tcount4,Tcoun6,Tcount14");
        break;
    case SD_EVENT_ICM_02:
        //MSN,testN,testR,DateTime,Temperature,RH,Flow,ISO_code,Tcount4,Tcoun6,Tcount14
        snprintf(buffer, buffer_size, "DateTime,MSN,testN,testR,Temperature,RH,Flow,ISO_code,Tcount4,Tcoun6,Tcount14");
        break;
    default:
        break;
    }
    return;
}
