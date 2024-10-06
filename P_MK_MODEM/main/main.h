
#ifndef _MAIN_H_
#define _MAIN_H_

#include <stdio.h>
#include <string.h>

#include "EG915/EG915_modem.h"

#include "ble_MK115B.h"


/************************************************
 * DEFINES
*************************************************/
#define TAB_PORT_MAX    30

/************************************************
 * STRUCTURES
*************************************************/


typedef struct modem_gsm{
    EG915_info_t info;  
    char         code[10];
	int          signal;
	time_t       time;
}modem_gsm_t;


typedef struct{
    uint8_t con;    // 1:Con, 0:Disc
    uint8_t on_of;  // 1:ON,  0:OFF
    time_t  date;   // time to ON/OFF
}dev_state_t;

typedef struct 
{
   dev_state_t  tv;
   dev_state_t  air;
   dev_state_t  project;
}ctrl_devices_t;



/*************************************************
 * 
 **************************************************/

typedef struct {
    char ip[20];
    uint16_t port;
}web_dir_t;

/************************************************
 * FUNCTIONS
*************************************************/

void m_replace_char(char *cadena, char buscar, char reemplazar);
void m_reverse_string(char *str);
void m_convert_time_to_str( time_t now, char *buffer, size_t buffer_size);


int validarIP(const char* ip);
char* m_get_esp_rest_reason();

void split_sentence(const char *sentence, const char *word, char *result);
int split_and_check_IP(char* cadena, char* ip);
int split_and_check_web(const char *message, web_dir_t *aux_web);

/************************************************
 * JSON PARSER
*************************************************/

int js_modem_to_str(const modem_gsm_t modem, char* buffer,size_t buffer_size);
int js_mk115_to_str(MK115_bc_data_t data, char* buffer,size_t buffer_size);
int js_ctrl_dev_to_str(const ctrl_devices_t data, char* buffer,size_t buffer_size);

#endif /*_MAIN_H_*/