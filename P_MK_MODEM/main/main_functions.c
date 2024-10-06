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

int validarIP(const char* ip) {
    char ip_copy[20]; 
    strcpy(ip_copy, ip); // Copia la dirección IP 

    int numTokens = 0;
    char* token = strtok(ip_copy, ".");

    while (token != NULL) {
        // Elimina espacios vacíos y caracteres no numéricos
        char cleaned_token[4];
        int i = 0;
        for (int j = 0; token[j] != '\0'; j++) {
            if (token[j] >= '0' && token[j] <= '9') {
                cleaned_token[i++] = token[j];
            }else{
                return -2; // CARACTERES INVALIDOS
            }
        }
        cleaned_token[i] = '\0';

        // Verifica si el token es un número válido
        char* endptr;
        int num = strtol(cleaned_token, &endptr, 10);
        if (*endptr != '\0' || num < 0 || num > 255) {
            return -1;
        }
        numTokens++;
        token = strtok(NULL, ".");
    }
    if (numTokens == 4){
        return 1;   // validate succesfull
    }
    return 0;   // fail num tokens
}


int split_and_check_IP(char* cadena, char* ip) {
    int ret = 0;
    if (sscanf(cadena, "MQTT,%[^,]", ip) == 1) {
        printf("Dirección MAC: %s\n", ip);
        ret =validarIP(ip);
        if (ret==1){
            return 1;// validate succesfull
        }else{
            return -1; // fail validate
        }
    } 
    return 0; // FAIL
}


void split_sentence(const char *sentence, const char *word, char *result) {
    // Encontrar la palabra en la frase
    char *pos = strstr(sentence, word);
    if (pos != NULL) {
        // Mover el puntero al final de la palabra encontrada
        pos += strlen(word);
        // Saltar cualquier espacio en blanco después de la palabra
        while (*pos == ' ') {
            pos++;
        }
        // Copiar la parte de la frase desde la palabra hasta el final
        strcpy(result, pos);
    } else {
        // Si la palabra no se encuentra, dejar el resultado vacío
        result[0] = '\0';
    }
}

int split_and_check_web(const char *message, web_dir_t *aux_web) {
    if (message == NULL) {
        return -1;
    }
    char ip[20] = {0};  // Asegúrate de que el tamaño sea suficiente para la IP
    uint16_t port;

    // Usar sscanf para separar la cadena en IP y puerto
    if (sscanf(message, "%19[^:]:%hu", ip, &port) == 2) {
        // update daux ip and port
        strcpy(aux_web->ip, ip);
        aux_web->port = port;
        return 1; // Separación exitosa
    }

    return 0; // Separación fallida
}