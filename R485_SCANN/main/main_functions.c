#include "main.h"

#include <stdio.h>
#include "stdlib.h"
#include <string.h>
#include "time.h"
#include "esp_timer.h"

#include "esp_log.h"
#include <ctype.h>

void m_replace_char(char *cadena, char buscar, char reemplazar) {
    for (int i = 0; cadena[i] != '\0'; ++i) {
        if (cadena[i] == buscar) {
            cadena[i] = reemplazar;
        }
    }
}


/*=======================================================================*/
void m_convert_time_to_str( time_t now, char *buffer, size_t buffer_size) {

    struct tm timeinfo;

    // Convertir a estructura de tiempo local
    // time_t tlocal = now-5*60*60; // restar 5 horas
    gmtime_r(&now, &timeinfo);
    
    // Formatear el tiempo en la cadena de caracteres
    strftime(buffer, buffer_size, "%Y-%m-%d %H:%M:%S", &timeinfo);
    return;
}


char * buffer_to_hex_string(const uint8_t *buffer, size_t size) {
    static char hex_string[MAX_HEX_STRING_LEN];
    char *hex_ptr = hex_string;  // Puntero para rastrear la posición actual
    int i;

    // Asegúrate de que el tamaño del buffer no exceda el tamaño de la cadena de salida
    if (size * 3 >= MAX_HEX_STRING_LEN) {
        printf("size is too long");
        return NULL;
    }

    // Convierte los bytes hexadecimales a una cadena de texto
    for (i = 0; i < size; i++) {
        hex_ptr += snprintf(hex_ptr, 4, "%02X ", buffer[i]);
    }
    *hex_ptr = '\n';  // Agrega el salto de línea al final
    return hex_string;
}


/**************************************** */
