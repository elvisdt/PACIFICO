
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#include "cJSON.h"
#include "main.h"
#include "uart_rs485.h"
//-------------------------------------//


int js_modem_to_str(const modem_gsm_t modem, char* buffer, size_t buffer_size) {
    if (buffer == NULL || buffer_size == 0) {
        //printf("Error: Búfer no válido\n");
        return -1;
    }

    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        //printf("Error al crear el objeto JSON\n");
        return -1;
    }
    cJSON_AddStringToObject(root, "iccid", modem.info.iccid);
    cJSON_AddStringToObject(root, "code", modem.code);
    cJSON_AddNumberToObject(root, "signal", modem.signal);
    cJSON_AddNumberToObject(root, "time", modem.time);

    char *json = cJSON_PrintUnformatted(root);
    if (json == NULL) {
        printf("Error al imprimir JSON\n");
        cJSON_Delete(root);
        return -1;
    }

    // Copia el JSON al búfer (con límite de tamaño)
    snprintf(buffer, buffer_size, "%s", json);
    cJSON_Delete(root);
    free(json);
    json = NULL;

    return 0;
}




  

  typedef struct {
    /* data */
    uint16_t voltage;       /*Volt, unit: 0.1*/
    
    uint32_t current;       /*Current, unit mA, with sign*/
    uint32_t power;         /*Power, uint 0.1W, whit sign*/
    uint32_t energy;        /*Power, uint 0.1W, whit sign*/
    
    uint8_t  load_state;        /*1: YES, 0:NO*/
    uint8_t  overload_state;    /*1: YES, 0:NO*/
    uint8_t  sw_state;          /*1: ON, 0:OFF*/

    uint32_t t_energy_c;        /*Total energy Kwh.EC*/

}asd;


  int js_mk115_to_str(MK115_bc_data_t data, char* buffer,size_t buffer_size){

    if (buffer == NULL || buffer_size == 0) {
        //printf("Error: Búfer no válido\n");
        return -1;
    }

    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        //printf("Error al crear el objeto JSON\n");
        return -1;
    }
    cJSON_AddNumberToObject(root, "Volt", (double)(data.voltage/10));
    cJSON_AddNumberToObject(root, "Amp", data.current);
    cJSON_AddNumberToObject(root, "Pwr", data.power);
    cJSON_AddNumberToObject(root, "Kwh", round((double)data.t_energy_c*1E3/EC_MK)/1E3);
    cJSON_AddNumberToObject(root, "Ss", data.sw_state);
    cJSON_AddNumberToObject(root, "Ls", data.load_state);
    cJSON_AddNumberToObject(root, "Os", data.overload_state);


    char *json = cJSON_PrintUnformatted(root);
    if (json == NULL) {
        printf("Error al imprimir JSON\n");
        cJSON_Delete(root);
        return -1;
    }

    // Copia el JSON al búfer (con límite de tamaño)
    snprintf(buffer, buffer_size, "%s", json);
    cJSON_Delete(root);
    free(json);
    json = NULL;

    return 0;

  }