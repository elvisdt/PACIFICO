#ifndef _CREDENTIALS_H_
#define _CREDENTIALS_H_

#include <stdio.h>
#include <time.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>



// #define TCP_CONNECTION
// #define MQTT_CONNECTION


#define APN     "smglobal.entel.pe" //
//#define APN     "airtelwap.es"//"ciot.vodafone.com"
//#define APN     "movistar.pe"//


// #define APN    "sim2m"

/**
 * OTA: defined OTA parameters
*/
#define PORT_OTA_01    65431
#define IP_OTA_01      "18.229.227.108"    // franco server

/*-----------------------*/
#define IP_OTA_02      "34.176.255.243"//"34.176.88.28"      // elvis server
#define PORT_OTA_02    65431

/**
 * FTP: defined OTA parameters
*/
#define IP_FTP_01      "18.229.227.108"      // FRANCO SERVER
#define PORT_FTP_01    21




/**
 * MQTT: defined MQTT parameters
*/
#define IP_MQTT     "161.97.102.234" //"3.129.163.139" //"34.176.125.182"//
#define PORT_MQTT   1883



#endif /*_CREDENTIALS_H_*/