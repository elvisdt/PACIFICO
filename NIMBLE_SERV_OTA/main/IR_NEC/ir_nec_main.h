

#ifndef _IR_NEC_MAIN_H_
#define _IR_NEC_MAIN_H_



#include <stdint.h>
#include "driver/rmt_encoder.h"

#ifdef __cplusplus
extern "C" {
#endif

/** ===================================================================
 *  DEFINITIONS
 * ====================================================================*/

/**
 * @brief NEC timing spec
 */
#define NEC_LEADING_CODE_DURATION_0  9000
#define NEC_LEADING_CODE_DURATION_1  4500
#define NEC_PAYLOAD_ZERO_DURATION_0  560
#define NEC_PAYLOAD_ZERO_DURATION_1  560
#define NEC_PAYLOAD_ONE_DURATION_0   560
#define NEC_PAYLOAD_ONE_DURATION_1   1690
#define NEC_REPEAT_CODE_DURATION_0   9000
#define NEC_REPEAT_CODE_DURATION_1   2250


#define EXAMPLE_IR_RESOLUTION_HZ     1000000 // 1MHz resolution, 1 tick = 1us
#define EXAMPLE_IR_NEC_DECODE_MARGIN 200     // Tolerance for parsing RMT symbols into bit stream
#define EXAMPLE_IR_TX_GPIO_NUM       21
#define EXAMPLE_IR_RX_GPIO_NUM       18


#define MAX_IR_CTRL_NUM     10
/** ===================================================================
 *  DEFINITIONS
 * ====================================================================*/
typedef struct {
    ir_nec_scan_code_t nec;
    uint32_t time;
}ir_nec_scan_t;

typedef struct {
    uint8_t num_ctrl;
    ir_nec_scan_t list_ir[MAX_IR_CTRL_NUM];
}ir_nec_list_t;


extern QueueHandle_t ir_nec_quemain;

void set_scan_ir_rep(bool mode);

int init_ir_nec_task();
int nec_sed_data_ctrl(ir_nec_scan_code_t nec_code_data);

#ifdef __cplusplus
}
#endif


#endif