

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

/** ===================================================================
 *  DEFINITIONS
 * ====================================================================*/

int init_ir_nec_task();



#ifdef __cplusplus
}
#endif


#endif