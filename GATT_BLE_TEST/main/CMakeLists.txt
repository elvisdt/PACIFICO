set(BLE_SERV_srcs  
        "BLE_SERV/ble_main.c"
        "BLE_SERV/gatt_serv.c"
        "BLE_SERV/gap.c")


idf_component_register(
        SRCS "main.c"  ${BLE_SERV_srcs} 
        INCLUDE_DIRS "."  "BLE_SERV"
        )
