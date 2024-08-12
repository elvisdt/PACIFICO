#ifndef _BLE_MK115B_H_
#define _BLE_MK115B_H_

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>


/* ============================================================
 * DEFINITIOS
 * =========================================================== */

// Erros 
#define RET_MK_OK     0
#define RET_MK_FAIL   -1


// Aux def
#define EC_MK   3200 //0X0C80 (manual protocol -> cap 2.17 )

/* ============================================================
 * DEFINITIOS
 * =========================================================== */
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

}MK115_bc_data_t;


int mk115_get_copy_data(MK115_bc_data_t *data);
void print_MK115_bc_data(const MK115_bc_data_t *data);


int mk115_parse_data(uint8_t *raw_data, size_t len_data);

#endif