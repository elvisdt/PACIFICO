#include "main.h"

#include <stdio.h>
#include "stdlib.h"
#include <string.h>
#include "time.h"
#include "esp_timer.h"

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




/*=======================================================================*/