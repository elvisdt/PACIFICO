#ifndef OTA_INC_OTA_GLOBAL_H_
#define OTA_INC_OTA_GLOBAL_H_

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include "esp_log.h"

#ifdef __cplusplus
extern "C" {
#endif


/***************************************
 * DEFINITIONS
 *************************************/
#define DEBUG_OTA_ON    1   // (0:DISABLE, 1:ENABLE)


#define ESP_LOG_BLUE  (ESP_LOG_VERBOSE + 1)
/***************************************
 * FUNTIONS
 *************************************/
void register_ota_log_level(void);
void debug_ota(const char *buffer, ...);


#ifdef __cplusplus
}
#endif




#endif /* OTA_INC_OTA_CONFIG_H_ */
