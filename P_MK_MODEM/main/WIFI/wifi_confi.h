
#ifndef _WIFI_CONF_H_
#define _WIFI_CONF_H_

#include "esp_log.h"
#include "mqtt_client.h"


/*********************************************************
 * DEFINITIONS
 **********************************************************/
#define ESP_WIFI_SSID "M2 ANKATEC 2.4G"
#define ESP_WIFI_PASSWORD "Ankatec666"
#define ESP_WIFI_MAXIMUM_RETRY 5


extern esp_mqtt_client_handle_t client_mqtt;

/*********************************************************
 * FUNCTIONS
 **********************************************************/
bool is_wifi_connected();
void check_ota_tcp_wifi(const char* host_ip, uint16_t host_port, const char* msg_loggin);


void wifi_init_sta(void);
void wifi_reconnect_task(void *pvParameters);

/*----MQTT PARAMS----*/
void mqtt_app_start(void);
bool mqtt_wifi_is_connected();


#endif