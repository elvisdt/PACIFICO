

set(IR_nec_srcs  
        "ir_nec/ir_nec_main.c"
        "ir_nec/ir_nec_encoder.c")

set(ble_serv_srcs  
        "ble_serv/ble_main.c"
        "ble_serv/gatt_svr.c")


idf_component_register(
        SRCS "main.c" ${IR_nec_srcs}  ${ble_serv_srcs} 
        INCLUDE_DIRS "." "ir_nec" "ble_serv"
        )
