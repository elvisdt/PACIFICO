

#include "ble_MK115B.h"

#include "esp_log.h"
#define TAG "MK115B"


static MK115_bc_data_t data_mk115 = {0};


int mk115_get_copy_data(MK115_bc_data_t *data){
    memcpy(data, &data_mk115, sizeof(MK115_bc_data_t));
    return 0;
}


int mk115_parse_head_B0(uint8_t *raw_data, size_t len_data){
        if (raw_data ==NULL || len_data<3){
        return -1;
    }
    
    uint8_t head = raw_data[0];
    if (head != 0xB0){
        return -2;
    }
    
    uint8_t function = raw_data[1];

    uint8_t length = raw_data[2];
    const uint8_t *data = &raw_data[3];
    ESP_LOGI(TAG,"FUNCTION: %02X", function);
    ESP_LOG_BUFFER_HEX(TAG, data, length);
    return 0;
}

int mk115_parse_head_B1(uint8_t *raw_data, size_t len_data){
    if (raw_data ==NULL || len_data<3){
        return -1;
    }
    
    uint8_t head = raw_data[0];
    if (head != 0xB1){
        return -2;
    }
    
    uint8_t function = raw_data[1];

    uint8_t length = raw_data[2];
    const uint8_t *data = &raw_data[3];
    ESP_LOGI(TAG,"FUNCTION: %02X", function);
    ESP_LOG_BUFFER_HEX(TAG, data, length);

    int ret = RET_MK_FAIL;
    switch (function)
    {
        // ON/OFF Status
        case 0x03:{
            if(length==0x01){
                data_mk115.sw_state = data[0];
                ESP_LOGW(TAG, "OFF/ON Status: %X", data[0]);
                ret = RET_MK_OK;
            }
            break;}
        // Device Power on Status
        case 0x04:{
            if(length==0x01){
                ESP_LOGW(TAG, "Dev Power st: %X", data[0]);
                ret = RET_MK_OK;
            }
            break;}

        // load status
        case 0x05:{
            if(length==0x01){
                data_mk115.load_state = data[0];
                ESP_LOGW(TAG, "load status: %X", data[0]);
                ret = RET_MK_OK;
            }
            break;}
        // Total Energy Consumpt
        case 0x08:{
            if(length==0x04){
                uint32_t e_consum_t = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
                data_mk115.t_energy_c = e_consum_t;
                ESP_LOGW(TAG, "Total Energy Consumpt: %lu kwh.EC, %f kwh", e_consum_t, (double)e_consum_t/EC_MK);
                ret = RET_MK_OK;
            }
            break;}

        default:
            break;
        }
    return ret;
}



/*************************************************
 * PARSE HEAD B2
 *************************************************/
int mk115_parse_head_B2(uint8_t *raw_data, size_t len_data){
    if (raw_data ==NULL || len_data<3){
        return -1;
    }
    
    uint8_t head = raw_data[0];
    if (head != 0xB2){
        return -2;
    }
    
    uint8_t function = raw_data[1];

    uint8_t length = raw_data[2];
    const uint8_t *data = &raw_data[3];
    ESP_LOGI(TAG,"FUNCTION: %02X", function);
    ESP_LOG_BUFFER_HEX(TAG, data, length);

    return 0;
}

int mk115_parse_head_B3(uint8_t *raw_data, size_t len_data){
        if (raw_data ==NULL || len_data<3){
        return -1;
    }
    
    uint8_t head = raw_data[0];
    if (head != 0xB3){
        return -2;
    }
    
    uint8_t function = raw_data[1];

    uint8_t length = raw_data[2];
    const uint8_t *data = &raw_data[3];
    ESP_LOGI(TAG,"FUNCTION: %02X", function);
    ESP_LOG_BUFFER_HEX(TAG, data, length);
    return 0;
}


int mk115_parse_head_B4(uint8_t *raw_data, size_t len_data){
    if (raw_data ==NULL || len_data<3){
        return -1;
    }
    
    uint8_t head = raw_data[0];
    if (head != 0xB4){
        return -2;
    }
    
    uint8_t function = raw_data[1];

    uint8_t length = raw_data[2];
    const uint8_t *data = &raw_data[3];
    ESP_LOGI(TAG,"FUNCTION: %02X", function);
    ESP_LOG_BUFFER_HEX(TAG, data, length);

    int ret = RET_MK_FAIL;

    switch (function)
    {
    // switch status
    case 0x01:{
        if(length==0x01){
            ESP_LOGW(TAG, "SW STATE: %X", data[0]);
            ret = RET_MK_OK;
        }
        break;}

    // load detection
    case 0x02:{
        if(length==0x01){
            ESP_LOGW(TAG, "LOAD DETECT: %X", data[0]);
            ret = RET_MK_OK;
        }
        break;}

    // Overload protection
    case 0x03:{
        if(length==0x02){
            uint16_t ov_prot = (data[0]<<8) | data[1];
            ESP_LOGW(TAG, "Overload protec: %u W", ov_prot);
            ret = RET_MK_OK;
        }
        break;}
    // Countdown
    case 0x04:{
        if(length==0x09){
            uint8_t sw_after = data[0];
            uint32_t count_val = (data[1] << 24) | (data[2] << 16) | (data[3] << 8) | data[4];
            uint32_t count_rem = (data[5] << 24) | (data[6] << 16) | (data[7] << 8) | data[8];

            ESP_LOGW(TAG, "Countdown: SW: %u, cv: %lu s, cr: %lu", sw_after, count_val, count_rem);
            ret = RET_MK_OK;
        }
        break;}

    // Active Current Voltage and Power
    case 0x05:{
        if(length==0x0A){
            uint16_t voltage = ((data[0] << 8) | data[1]);
            uint32_t current = (data[2] << 24) | (data[3] << 16) | (data[4] << 8) | data[5];
            uint32_t power   = (data[6] << 24) | (data[7] << 16) | (data[8] << 8) | data[9];
            
            // data update active: current voltage power
            data_mk115.current = current;
            data_mk115.voltage = voltage;
            data_mk115.power = power;

            ESP_LOGI(TAG, "DATA V: %.1f V, C: %ld mA, P: %.1f W",(float)voltage/10, current, (float)power/10);
            ret = RET_MK_OK;
        }
        break;}

    // Current Energy Consump
    case 0x06:{
        if(length==0x11){
            uint16_t d_year = (data[0] << 8) | data[1];
            uint8_t  d_month = data[2];
            uint8_t  d_day = data[3];
            uint8_t  d_hour = data[4];

            uint32_t e_total = (data[5] << 24)  | (data[6] << 16) | (data[7] << 8) | data[8];
            uint16_t e_cc30d = (data[9] << 8)   | data[10];
            uint16_t e_ccd   = (data[11] << 8)  | data[12];
            uint16_t e_cch   = (data[13] << 8)  | data[14];

            ESP_LOGW(TAG, "CEC: date: %u-%u-%u %u, ET: %f, EC30: %f, ECCD: %f, ECCH: %f", 
                        d_year,
                        d_month,
                        d_day,
                        d_hour,
                        (double)e_total/EC_MK,
                        (double)e_cc30d/EC_MK, 
                        (double)e_ccd/EC_MK, 
                        (double)e_cch/EC_MK);
            ret = RET_MK_OK;
        }
        break;}

    default:
        break;
    }

    return ret;
}



int mk115_parse_data(uint8_t *raw_data, size_t len_data){
    if (raw_data ==NULL || len_data<3){
        return -1;
    }
    
    uint8_t head = raw_data[0];
     ESP_LOGI(TAG,"\n");
    ESP_LOGI(TAG,"============= PARSE DATA =============");
    ESP_LOGE(TAG, "HEAD DATA: %02X", head);
    ESP_LOG_BUFFER_HEX_LEVEL(TAG, raw_data, len_data, ESP_LOG_WARN);
    int ret = 0;

    switch (head)
    {
    case 0xB0:
        ret = mk115_parse_head_B0(raw_data, len_data);
        break;

    case 0xB1:
        ret = mk115_parse_head_B1(raw_data, len_data);
        break;

    case 0xB2:
        ret = mk115_parse_head_B2(raw_data, len_data);
        break;

    case 0xB3:
        ret = mk115_parse_head_B3(raw_data, len_data);
        break;
    case 0xB4:
        ret = mk115_parse_head_B4(raw_data, len_data);
        break;
    
    default:
        break;
    }

    return ret;
}


/************************************************************************
 * 
 ************************************************************************/
void print_MK115_bc_data(const MK115_bc_data_t *data) {
    printf("Voltage: %.1f V\n", data->voltage / 10.0);
    printf("Current: %ld mA\n", data->current);
    printf("Power: %.2f W\n", data->power / 10.0);
    printf("Energy: %.2f W\n", data->energy / 10.0);
    printf("Load State: %s\n", data->load_state ? "YES" : "NO");
    printf("Overload State: %s\n", data->overload_state ? "YES" : "NO");
    printf("Switch State: %s\n", data->sw_state ? "ON" : "OFF");
    printf("Total Energy: %ld Kwh.EC, %.3f kwh\n", data->t_energy_c, (double)data->t_energy_c/EC_MK);
}
