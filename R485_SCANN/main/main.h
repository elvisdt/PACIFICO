
#ifndef _MAIN_H_
#define _MAIN_H_

#include <stdio.h>
#include <string.h>
#include "time.h"


/************************************************
 * DEFINES
*************************************************/
#define TAB_PORT_MAX    30

#define BUF_SIZE_MD             (1024)
#define RD_BUF_SIZE             (BUF_SIZE_MD)
#define MAX_HEX_STRING_LEN      BUF_SIZE_MD

/************************************************
 * STRUCTURES
*************************************************/


/*************************************************
 * 
 **************************************************/


/************************************************
 * FUNCTIONS
*************************************************/

void m_replace_char(char *cadena, char buscar, char reemplazar);
void m_reverse_string(char *str);
char * buffer_to_hex_string(const uint8_t *buffer, size_t size);

void mc_log_data(const char *tag, const void *buffer, uint16_t buff_len);


/************************************************
 * JSON PARSER
*************************************************/

#endif /*_MAIN_H_*/