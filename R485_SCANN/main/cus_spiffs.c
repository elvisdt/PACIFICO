

#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <math.h>
#include <dirent.h>

#include "nvs.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_spiffs.h"

#include "cus_spiffs.h"



#define TAG "NVS"


int cus_nvs_file_exists(const char* path) {
    FILE* file = fopen(path, "r");
    if (file != NULL) {
        fclose(file);
        return 1;
    } else {
        return 0;
    }
}



int cus_nvs_clear_file(const char* path) {
    FILE* file = fopen(path, "w");
    if (file == NULL) {
        ESP_LOGE(TAG,"No se pudo abrir el archivo");
        return 0;
    }
    fclose(file);
	return 1;
}


void cus_nvs_read_data_txt(const char *file_path) {
    FILE *archivo = fopen(file_path, "r");
    if (archivo == NULL) {
        printf("Error al abrir el archivo\n");
        return;
    }

    char linea[300]; // Tamaño del búfer para almacenar cada línea
    int l = 0;
    while (fgets(linea, sizeof(linea), archivo) != NULL) {
        // Procesa la línea aquí (por ejemplo, imprímela)
        l++;
        printf("read l %d : %s", l, linea);
    }
    printf("\n");
    fclose(archivo);
}


int cus_nvs_num_lines(const char *file_path) {
    FILE *archivo = fopen(file_path, "r");
    if (archivo == NULL) {
        ESP_LOGE(TAG,"No se pudo abrir el archivo"); 
        return -1; // Indicador de error
    }

    int contador = 0;
    char linea[300]; // Tamaño del búfer para almacenar cada línea
    while (fgets(linea, sizeof(linea), archivo) != NULL) {
        contador++;
    }
    fclose(archivo);
    return contador;
}



int cus_nvs_read_line(const char *file_path, int indice, char* line_str) {
    FILE *archivo = fopen(file_path, "r");
    if (archivo == NULL) {
        printf("Error al abrir el archivo\n");
        return 0; // Indicador de error
    }

    char linea_aux[300]; // Tamaño del búfer para almacenar cada línea
    int contador = 0;

    while (fgets(linea_aux, sizeof(linea_aux), archivo) != NULL) {
        if (contador == indice) {
            fclose(archivo);
            strcpy(line_str,linea_aux);
            return 1;
        }
        contador++;
    }

    fclose(archivo);
    return 0; // Índice fuera de rango
}

int cus_nvs_write_line(const char *file_path, const char *line) {
    FILE *archivo = fopen(file_path, "w"); // Cambio de "a" a "w"
    if (archivo == NULL) {
        ESP_LOGE(TAG, "No se pudo abrir el archivo");
        return 0; // Indicador de error
    }

    fputs(line, archivo);
    fclose(archivo);
    return 1; // Éxito
}



int cus_nvs_add_line(const char *file_path, const char *new_line) {
    FILE *archivo = fopen(file_path, "a");
    if (archivo == NULL) {
        ESP_LOGE(TAG,"No se pudo abrir el archivo");
        return 0; // Indicador de error
    }

    fputs(new_line, archivo);
    fclose(archivo);
    return 1; // Éxito
}

int cus_nvs_delete_line(const char *file_path, int indice) {
    FILE *archivo = fopen(file_path, "r");
    if (archivo == NULL) {
        printf("No se pudo abrir el archivo\n");
        return 0; // Indicador de error
    }

    FILE *temp = fopen(FILE_PATH_TEMP, "w");
    if (temp == NULL) {
        fclose(archivo);
        printf("Error al crear el archivo temporal\n");
        return 0;
    }

    char linea_aux[300];
    int contador = 0;

    while (fgets(linea_aux, sizeof(linea_aux), archivo) != NULL) {
        if (contador != indice) {
            fputs(linea_aux, temp);
        }
        contador++;
    }

    fclose(archivo);
    fclose(temp);

    // Reemplazar el archivo original con el archivo temporal
    remove(file_path);
    rename(FILE_PATH_TEMP, file_path);

    return 1; // Éxito
}


/*

esp_err_t cus_nvs_delete_line(const char *file_path, int indice) {
    FILE *archivo = fopen(file_path, "r");
    if (archivo == NULL) {
        ESP_LOGE(TAG, "Error al abrir el archivo");
        return ESP_FAIL;
    }

    FILE *temp = fopen("/spiffs/temp.txt", "w");
    if (temp == NULL) {
        fclose(archivo);
        ESP_LOGE(TAG, "Error al crear el archivo temporal");
        return ESP_FAIL;
    }

    char linea_aux[300];
    int contador = 0;

    while (fgets(linea_aux, sizeof(linea_aux), archivo) != NULL) {
        if (contador != indice) {
            fputs(linea_aux, temp);
        }
        contador++;
    }

    fclose(archivo);
    fclose(temp);

    // Reemplazar el archivo original con el archivo temporal
    remove(file_path);
    rename("/spiffs/temp.txt", file_path);

    return ESP_OK;
}

*/