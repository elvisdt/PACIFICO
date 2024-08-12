

#ifndef _CUSTOM_NVS_H_
#define _CUSTOM_NVS_H_

#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <math.h>
#include <dirent.h>

#include "nvs.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_spiffs.h"
#include <dirent.h>

/*********************************************
 * DEFINE
*********************************************/

//"EXNAND:" for external nand flash


#define LABEL_SPIFFS            "storage"
#define DIR_FILE                "spiffs"
#define FILE_DATA               "data.txt"
#define FILE_B01                "bot01.txt"

#define FILE_TEMP               "temp.txt"

#define FILE_PATH_DATA          "/"DIR_FILE"/"FILE_DATA""
#define FILE_PATH_TEMP          "/"DIR_FILE"/"FILE_TEMP""
#define FILE_PATH_BO1          "/"DIR_FILE"/"FILE_B01""

/*********************************************
 * STRUCTURES
*********************************************/



/*********************************************
 *FUNCTIONS
*********************************************/

int cus_nvs_file_exists(const char* path);

int cus_nvs_clear_file(const char* path);

int cus_nvs_write_line(const char *file_path, const char *line);

void cus_nvs_read_data_txt(const char *file_path);

int cus_nvs_num_lines(const char *file_path);


int cus_nvs_read_line(const char *file_path, int indice, char* line_str);

int cus_nvs_add_line(const char *file_path, const char *new_line);

int cus_nvs_delete_line(const char *file_path, int indice);


#endif /*CUSTOM_NVS_H*/