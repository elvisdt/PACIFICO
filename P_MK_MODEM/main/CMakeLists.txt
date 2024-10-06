

set(srcs "main.c"
         "json_parser.c"
         "main_functions.c"
        )


set(LIBS_srcs  
        "LIBS/nca9539.c"
        "LIBS/SD_card_control.c"
        "LIBS/uart_rs485.c"
        )

set(BLE_srcs  
        "BLE/ble_MK115B.c"
        "BLE/gatt_multi_con.c")


set(WIFI_srcs  
        "WIFI/wifi_mqtt_tcp.c"
        "WIFI/wifi_ota_tcp.c"
        "WIFI/wifi_STA.c")
    
    

set(OTA_srcs  
    "OTA/crc.c"
    "OTA/ota_control.c"
    "OTA/ota_esp32.c"
    "OTA/ota_global.c"
    "OTA/ota_modem.c")


set(EG915_srcs  
    "EG915/EG915_modem.c"
    "EG915/modem_aux.c")


idf_component_register(
    SRCS ${srcs} ${OTA_srcs} ${EG915_srcs} ${LIBS_srcs} ${BLE_srcs} 
    INCLUDE_DIRS "." "OTA" "EG915" "LIBS" "BLE"
)
