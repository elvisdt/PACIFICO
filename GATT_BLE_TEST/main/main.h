#ifndef _MAIN_H_
#define _MAIN_H_

#include "stdio.h"
#include "stdint.h"

#define NUM_MAX_CTRL 10

typedef struct {
    uint16_t addr;
    uint16_t cmd;
}ctrl_btn_t;


typedef struct {
    uint8_t total;
    ctrl_btn_t list[NUM_MAX_CTRL];
}list_ctrl_t;


int m_get_data_ctrl(uint8_t *data, size_t *len);


#endif