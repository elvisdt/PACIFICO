#ifndef SD_CARD_CONTROL_H
#define SD_CARD_CONTROL_H

#include <stdio.h>
#include <stdint.h>
#include "esp_err.h"
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"


#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************
 * DEFINITIONS
 *************************************************************************/
#define SD_PIN_MISO GPIO_NUM_6
#define SD_PIN_MOSI GPIO_NUM_15
#define SD_PIN_CLK GPIO_NUM_7
#define SD_PIN_CS GPIO_NUM_16

#define SD_PIN_PWR GPIO_NUM_4
#define SD_PIN_DETEC GPIO_NUM_5


#define FILE_NAME_TABLERO   "tablero.csv"
#define FILE_NAME_ICM01     "ICM01.csv"
#define FILE_NAME_ICM02     "ICM02.csv"

#define MAX_CHAR_SIZE_SD    512
/**************************************************************************
 * STRUCTURES
 *************************************************************************/
// event to write SD 

typedef enum{
    SD_EVENT_TABLERO,
    SD_EVENT_ICM_01,
    SD_EVENT_ICM_02
}event_sd_t;

// type DATA 

typedef struct {
    event_sd_t event;
    char data[MAX_CHAR_SIZE_SD];
}sd_data_struct;

/**************************************************************************
 * FUNCTIONS
 *************************************************************************/
void sd_init_gpios();

// 0 -> OK    1 -> FAIL 
int sd_get_status_conn();

// 0 -> OFF    1 -> ON
void sd_set_power(uint32_t level);





// inicializar la SD
esp_err_t sd_card_init(void);

// desinicializar la SD
esp_err_t sd_card_deinit(void);

// Obtener espacio libre en la tarjeta SD
esp_err_t sd_card_get_free_space(size_t *free_bytes);

// Escribir la cabecera en un archivo CSV, si el archivo no existe lo crea
esp_err_t sd_card_csv_write_header(const char *filename, const char *header);

// Añadir datos adicionales a un archivo CSV existente
esp_err_t sd_card_csv_append_data(const char *filename, const char *data);

// Leer y procesar datos desde un archivo CSV
esp_err_t sd_card_csv_read_and_process(const char *filename);

esp_err_t sd_card_csv_write(const char *filename, const char *header, const char *data);

esp_err_t sd_card_csv_clear_file(const char *filename);

esp_err_t sd_card_csv_delete_line(const char *filename, int line_to_delete);

esp_err_t sd_card_csv_read_specific_line(const char *filename, int line_number, char *buffer, size_t buffer_size);

/*----------------------------------------------------------------------*/

void sd_generate_str_head(event_sd_t event,char *buffer, size_t buffer_size);
#ifdef __cplusplus
}
#endif

#endif /* SD_CARD_CONTROL_H */
