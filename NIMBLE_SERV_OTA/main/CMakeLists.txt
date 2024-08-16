set(srcs 
    "main.c")

set(ble_srcs  
    "BLE/gap.c"
    "BLE/gatt_svr.c")

set(IR_NEC_srcs  
    "IR_NEC/ir_nec_main.c"
    "IR_NEC/ir_nec_encoder.c")


idf_component_register(
        SRCS ${srcs}  ${ble_srcs} ${IR_NEC_srcs}
        INCLUDE_DIRS "." "BLE" "IR_NEC"
    )