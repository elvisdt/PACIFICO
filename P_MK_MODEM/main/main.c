#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <esp_event.h>

#include <esp_system.h>
#include <sdkconfig.h>
#include <sys/time.h>
#include <time.h>
#include <esp_timer.h>

#include <esp_log.h>
#include "esp_ota_ops.h"
#include <nvs.h>
#include <nvs_flash.h>


#include "esp_log.h"


#include <cJSON.h>

/* main libraries */
#include "main.h"
#include "credentials.h"
#include "EG915_modem.h"
#include "ota_modem.h"

#include "gap.h"
#include "gatt_cli.h"
#include "ble_MK115B.h"

/* ota modem librarias */
#include "crc.h"
#include "ota_control.h"
#include "ota_esp32.h"
#include "ota_headers.h"


/***********************************************
 * DEFINES
 ************************************************/
#define DBG_MAIN_TASK 0
#define DBG_I2C_TASK 1
#define DBG_RS485_TASK 1
#define DBG_SD_CARD_TASK 0

#define TAG "MAIN"
#define MAX_ATTEMPS 3

#define WAIT_MS(x) vTaskDelay(pdMS_TO_TICKS(x))
#define WAIT_S(x) vTaskDelay(pdMS_TO_TICKS(x * 1e3))

#define MASTER_TOPIC_MQTT "PACIFICO"

/***********************************************
 * STRUCTURES
 ************************************************/



/***********************************************
 * VARIABLES
 ************************************************/


/*---> External variables <--*/
QueueHandle_t uart_md_queue;
uint8_t rx_modem_ready;
int rxBytesModem;
uint8_t *p_RxModem;

/*---> Task Handle <---*/
TaskHandle_t UART_task_handle = NULL;
TaskHandle_t MAIN_task_handle = NULL;
TaskHandle_t BLE_Task_handle = NULL;
TaskHandle_t RS485_Task_handle = NULL;

/*---> Data OTA loggin_ota <---*/
char *loggin_ota;

/*---> Aux Mememory <---*/
uint8_t aux_buff_mem[BUF_SIZE_MD];
char *buff_aux_main = (char *)aux_buff_mem;

/*---> data structure for modem <---*/
static modem_gsm_t gb_data_md = {0};


static int ret_update_time = 0;

/*---> gpio and uart config <---*/
EG915_gpio_t modem_gpio;
EG915_uart_t modem_uart;

/*---> OTA <--*/
uint8_t watchdog_en = 1;
uint32_t current_time = 0;

/*--> MQTT CHARS <---*/
int mqtt_idx = 0; // 0->5

/*--> global main variables <--*/
uint8_t delay_info = 5; /* Configurar de acuerdo al cliente*/
uint32_t Info_time = 0;
uint32_t MQTT_read_time = 0;
uint32_t OTA_md_time = 1;

/***********************************************
 * VARIABLES
 ************************************************/

void main_cfg_parms()
{
    // ---> placa negra GPS <--- //
    modem_gpio.gpio_reset = GPIO_NUM_46;
    modem_gpio.gpio_pwrkey = GPIO_NUM_9;
    modem_gpio.gpio_status = GPIO_NUM_8;

    modem_uart.uart_num = UART_NUM_2;
    modem_uart.baud_rate = 115200;
    modem_uart.gpio_uart_rx = GPIO_NUM_17;
    modem_uart.gpio_uart_tx = GPIO_NUM_18;
};

/**
 * Inicializa la configuración del módem, encendiéndolo y ejecutando los comandos de inicio.
 *
 * @return MD_CFG_SUCCESS si la configuración se inicializa correctamente, MD_CFG_FAIL en caso contrario.
 */
int Init_config_modem()
{
    ESP_LOGW(TAG, "--> INIT CONFIG MODEM <--");
    int state = 0;
    Modem_reset();
    WAIT_S(10);
    for (size_t i = 0; i < MAX_ATTEMPS; i++)
    {
        WAIT_S(2);
        state = Modem_begin_commands();
        if (state == MD_AT_TIMEOUT)
        {
            Modem_reset();
        }
        else if (state == MD_AT_OK)
        {
            return MD_CFG_SUCCESS;
        }
    }
    return MD_CFG_FAIL;
}

/**
 * Cnifiguraraion el modem y obtencion de
 * @return MD_CFG_SUCCESS si el módem se activa correctamente, MD_CFG_FAIL en caso contrario.
 */
int Active_modem()
{
    int status = Init_config_modem();
    if (status == MD_CFG_SUCCESS)
    {
        int status = Modem_get_dev_info(&gb_data_md.info);
        if (status == MD_CFG_SUCCESS)
        {
            return MD_CFG_SUCCESS; // FAIL
        }
    }
    return MD_CFG_FAIL;
}

void Init_NVS_Keys()
{
    /*------------------------------------------------*/
    ESP_LOGI(TAG, "Init NVS keys of data");

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // nvs_open(NAMESPACE_NVS,NVS_READWRITE,&storage_nvs_handle);

    ESP_LOGI(TAG, "NVS keys recovered succsesfull\r\n");
    return;
}


int Init_BLE_params()
{
    /*------------------------------------------------*/
    
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_err_t ret = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        ESP_LOGE(TAG, "%s initialize controller failed: %s", __func__, esp_err_to_name(ret));
        return ret;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) {
        ESP_LOGE(TAG, "%s enable controller failed: %s", __func__, esp_err_to_name(ret));
        return ret;
    }

    ret = esp_bluedroid_init();
    if (ret) {
        ESP_LOGE(TAG, "%s init bluetooth failed: %s", __func__, esp_err_to_name(ret));
        return ret;
    }

    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(TAG, "%s enable bluetooth failed: %s", __func__, esp_err_to_name(ret));
        return ret;
    }

    ret = init_gap_client();
    if (ret){
        ESP_LOGE(TAG, "%s gap register failed, error code = %x", __func__, ret);
        return ret;
    }

    ret =  init_gatt_client();
    if(ret){
        ESP_LOGE(TAG, "%s gattc register failed, error code = %x", __func__, ret);
        return ret;
    }

    ret = esp_ble_gattc_app_register(PROFILE_A_APP_ID);
    if (ret){
        ESP_LOGE(TAG, "%s gattc app register failed, error code = %x", __func__, ret);
    }

    ret = esp_ble_gatt_set_local_mtu(500);
    if (ret){
        ESP_LOGE(TAG, "set local  MTU failed, error code = %x", ret);
    }
    return ret;
}
/**************************************************************
 *
 **************************************************************/
int CheckRecMqtt(void)
{
    ESP_LOGI("MAIN-MQTT", "--> Revisando conexion <--");
    static int num_max_check = 0;
    static int ret_conn = 0;
    static int ret_open = 0;

    if (num_max_check >= (MAX_ATTEMPS + 2))
    {
        ESP_LOGE("MAIN-MQTT", "RESTART ESP32");
        esp_restart();
    }
    printf("num_max: %d\r\n", num_max_check);

    ret_conn = Modem_Mqtt_CheckConn(mqtt_idx);
    printf("ret_conn: 0x%X\r\n", ret_conn);
    if (ret_conn == MD_MQTT_CONN_ERROR)
    {
        ret_open = Modem_Mqtt_CheckOpen(mqtt_idx, IP_MQTT, PORT_MQTT);
        printf("ret_open: 0x%X\r\n", ret_open);
        if (ret_open == MD_CFG_FAIL)
        {
            num_max_check++;
        }
        else if (ret_open == MD_MQTT_IS_OPEN)
        {
            // Conectamos son nuesto indice e imei
            Modem_Mqtt_Conn(mqtt_idx, gb_data_md.info.imei);
        }
        else if (ret_open == MD_MQTT_NOT_OPEN)
        {
            num_max_check++;
            // Configurar y Abrir  MQTT
            ret_open = Modem_Mqtt_Open(mqtt_idx, IP_MQTT, PORT_MQTT);
            if (ret_open == MD_MQTT_OPEN_OK)
            {
                // Abrir comunicacion  y suscripcion
                Modem_Mqtt_Conn(mqtt_idx, gb_data_md.info.imei);
            }
            else
            {
                WAIT_S(2);
                // desconectar y cerrar en caso exista alguna comunicacion
                Modem_Mqtt_Disconnect(mqtt_idx);
                Modem_Mqtt_Close(mqtt_idx);
            }
        }
        CheckRecMqtt(); // Volvemos a verificar la conexion
    }
    else if (ret_conn == MD_MQTT_CONN_INIT || ret_conn == MD_MQTT_CONN_DISCONNECT)
    {
        WAIT_S(1);
        Modem_Mqtt_Conn(mqtt_idx, gb_data_md.info.imei);
        CheckRecMqtt(); // CHECK AGAIN
    }
    else if (ret_conn == MD_MQTT_CONN_CONNECT)
    {
        WAIT_S(1);
        CheckRecMqtt(); // CHECK AGAIN
    }
    else if (ret_conn == MD_MQTT_CONN_OK)
    {
        num_max_check = 0;
        ESP_LOGI("MAIN-MQTT", "CONNECT SUCCESFULL");
        // printf("TOPIC SUB: \'%s\'\r\n",topic_sub);
        // printf("ret_sub: 0x%X\r\n",state_mqtt_sub);
    }
    return ret_conn;
}

/** */
int Send_info_mqtt_md(void)
{
    ESP_LOGI("MQTT-INFO", "<-- Send device info -->");

    char topic[60] = {0};
    sprintf(topic, "%s/%s/INFO", MASTER_TOPIC_MQTT, gb_data_md.info.imei);

    time(&gb_data_md.time);
    gb_data_md.signal = Modem_get_signal();

    int ret_check = 0;

    ret_check = CheckRecMqtt();
    ESP_LOGI("MQTT-INFO", "ret-conn: 0x%X", ret_check);
    if (ret_check == MD_MQTT_CONN_OK)
    {
        js_modem_to_str(gb_data_md, buff_aux_main, BUF_SIZE_MD);
        ret_check = Modem_Mqtt_Pub(buff_aux_main, topic, strlen(buff_aux_main), mqtt_idx, 0);
        ESP_LOGI("MQTT-INFO", "ret-pubb: 0x%X", ret_check);
        WAIT_S(1);
    }
    // Modem_Mqtt_Disconnect(mqtt_idx);
    return ret_check;
}

/** */

int Send_all_data_mqtt_md()
{
    ESP_LOGI("MQTT-ALL", "<-- Send data rep-->");

    char topic[100] = {0};
    //--- set topic to info modem--//

    time(&gb_data_md.time);
    gb_data_md.signal = Modem_get_signal();
    int ret_check = 0;


    ret_check = CheckRecMqtt();
    ESP_LOGI("MQTT-ALL", "ret-conn: 0x%X", ret_check);
    if (ret_check == MD_MQTT_CONN_OK)
    {
        // --SEND INFO DATA---
        sprintf(topic, "%s/%s/INFO", MASTER_TOPIC_MQTT, gb_data_md.info.imei);
        js_modem_to_str(gb_data_md, buff_aux_main, BUF_SIZE_MD);
        ret_check = Modem_Mqtt_Pub(buff_aux_main, topic, strlen(buff_aux_main), mqtt_idx, 0);
        ESP_LOGI("MQTT-INFO", "ret-pubb: 0x%X", ret_check);
        WAIT_S(1);

        //-- MK115--
        sprintf(topic, "%s/%s/MK115-0", MASTER_TOPIC_MQTT, gb_data_md.info.imei);
        MK115_bc_data_t data_mk = {0};
        mk115_get_copy_data(&data_mk);
        js_mk115_to_str(data_mk, buff_aux_main,BUF_SIZE_MD);
        ret_check = Modem_Mqtt_Pub(buff_aux_main, topic, strlen(buff_aux_main), mqtt_idx, 0);
        ESP_LOGI("MQTT-MK115", "ret-pubb: 0x%X", ret_check);
        WAIT_S(1);

    }
    // Modem_Mqtt_Disconnect(mqtt_idx);
    return ret_check;
}



/**
 * @brief Checks for OTA (Over-The-Air) updates.
 *
 * This function establishes a connection to an OTA server, sends device information,
 * and waits for a response. If an OTA update is pending (indicated by the response),
 * it initiates the OTA process.
 *
 * @note The specific details such as 'IP_OTA_01', 'PORT_OTA_01', and custom functions are
 * application-specific and should be defined elsewhere in your code.
 */
void OTA_Modem_Check(const char *tcp_ip, uint16_t tcp_port)
{

    static const char *TAG_OTA = "OTA_MD";
    esp_log_level_set(TAG_OTA, ESP_LOG_INFO);
    int conn_idTCP = 0;
    ESP_LOGI(TAG_OTA, "==MODEM OTA CHECK ==");
    char buffer[500] = "";
    do
    {
        if (TCP_open(tcp_ip, tcp_port, conn_idTCP) != MD_TCP_OPEN_OK)
        {
            ESP_LOGW(TAG_OTA, "Not connect to the Server");
            TCP_close(conn_idTCP);
            break;
        }
        ESP_LOGI(TAG_OTA, "Requesting update...");
        if (TCP_send(loggin_ota, strlen(loggin_ota)) == MD_TCP_SEND_OK)
        { // 1. Se envia la info del dispositivo
            ESP_LOGI(TAG_OTA, "Waiting for response...");
            readAT("}\r\n", "-8/()/(\r\n", 10000, buffer); // 2. Se recibe la 1ra respuesta con ota True si tiene un ota pendiente... (el servidor lo envia justo despues de recibir la info)(}\r\n para saber cuando llego la respuesta)
            debug_ota("main> repta %s\r\n", buffer);
            if (strstr(buffer, "\"ota\": \"true\"") != 0x00)
            {
                ESP_LOGI(TAG_OTA, "Start OTA download");
                ESP_LOGW(TAG_OTA, "WDT deactivate");
                watchdog_en = 0;
                if (Ota_UartControl_Modem() == OTA_EX_OK)
                {
                    ESP_LOGI(TAG_OTA, "OTA UPDATE SUCCESFULL, RESTART");
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    esp_restart();
                }
                else
                {
                    ESP_LOGW(TAG_OTA, "FAIL OTA UPDATE");
                }
                current_time = pdTICKS_TO_MS(xTaskGetTickCount()) / 1000;
                watchdog_en = 1;
                ESP_LOGW(TAG_OTA, "WDT reactivate");
            }
            int ret_tcp = TCP_close(conn_idTCP); // close tcp
            printf("ret : 0x%x\r\n", ret_tcp);
        }
    } while (false);
    return;
}

/***********************************************
 * MODEM UART TASK
 ************************************************/

static void Modem_rx_task(void *pvParameters)
{
    uart_event_t event;
    uint8_t *dtmp = (uint8_t *)malloc(RD_BUF_SIZE);
    p_RxModem = dtmp; // Índice para rastrear los bytes almacenados

    for (;;)
    {
        if (xQueueReceive(uart_md_queue, (void *)&event, portMAX_DELAY))
        {
            bzero(dtmp, RD_BUF_SIZE);
            switch (event.type)
            {
            case UART_DATA:
                // ESP_LOGI("UART", "[UART DATA]: %d", event.size);
                rxBytesModem = event.size;
                uart_read_bytes(modem_uart.uart_num, dtmp, event.size, portMAX_DELAY);
                p_RxModem = dtmp;
                rx_modem_ready = 1;
                // ESP_LOG_BUFFER_HEXDUMP("UART", dtmp, event.size, ESP_LOG_INFO);
                //  procesar data
                break;
            case UART_FIFO_OVF:
                ESP_LOGE("UART", "hw fifo overflow");
                uart_flush_input(modem_uart.uart_num);
                xQueueReset(uart_md_queue);
                break;
            case UART_BUFFER_FULL:
                ESP_LOGI(TAG, "ring buffer full");
                // If buffer full happened, you should consider increasing your buffer size
                uart_flush_input(modem_uart.uart_num);
                xQueueReset(uart_md_queue);
                break;

            default:
                break;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    free(dtmp);
    dtmp = NULL;
    vTaskDelete(NULL);
}

/***********************************************
 * Watchdog UART TASK
 ************************************************/
static void GPRS_Wdt_Task(void *pvParameters)
{
    uint32_t watchdog_time;
    for (;;)
    {
        watchdog_time = ((pdTICKS_TO_MS(xTaskGetTickCount()) / 1000) - current_time);
        if (watchdog_time >= 180 && watchdog_en)
        {
            ESP_LOGE("WTD", " moden no respondio por 3 minutos, reiniciando...\r\n");
            vTaskDelete(MAIN_task_handle);
            // M95_reset();
            vTaskDelay(pdMS_TO_TICKS(2000));
            esp_restart();
        }

        if (watchdog_en)
        {
            printf("WTD. time pass: %lu sec\r\n", watchdog_time);
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
    vTaskDelete(NULL);
}

/*********************************************
 * I2C SD CARD TASK
 *********************************************/

/*==============================================================
 * I2C FUNCTIONS AND TASK
 *=============================================================*/


/** ==========================================================
 * RS485 TASK
 * ============================================================*/

static void rs485_read_task(void *pvParameters)
{

    ESP_LOGI("ICM", "------------INIT ICM TASK-----------------");
    WAIT_S(2);
    int ret1 = 0, ret2 = 0;
    
  
#if DBG_RS485_TASK
    ESP_LOGI("ICM", "read uart 01: %d", ret1);
    ESP_LOGI("ICM", "read uart 02: %d", ret2);
#endif
    WAIT_S(5 * 55);
    

    vTaskDelete(NULL);
}


/** ==========================================================
 * RS485 TASK
 * ============================================================*/

static void ble_chek_task(void *pvParameters)
{

    ESP_LOGI("BLE", "------------INIT BLE TASK-----------------");
    WAIT_S(2);
    
    uint8_t last_state=0;
    uint16_t count =0;
    MK115_bc_data_t data_mk115 ={0};

    bool bt_scan_state = bt_gap_get_scann_state();
    bool bt_con_state =  bt_gap_get_conn_state();
    if (!bt_scan_state){
        ble_scanner_start();
        ESP_LOGI("MAIN", "START SCAN");
    }

    vTaskDelay(pdMS_TO_TICKS(5000));
    while (1){
        
        bt_scan_state = bt_gap_get_scann_state();
        bt_con_state =  bt_gap_get_conn_state();
        printf("\n");
        ESP_LOGE("MAIN", "STATUS, SCAN: %d, CONN: %d", bt_scan_state,bt_con_state);

        if (bt_scan_state==false && bt_con_state==false){
            vTaskDelay(pdMS_TO_TICKS(10000));
            ble_scanner_start();
            ESP_LOGI("MAIN", "START SCAN");
        }

        
        
        if (bt_con_state){
            vTaskDelay(pdMS_TO_TICKS(1000));
            last_state = !last_state;

            if(count<3){
                count ++;
                ESP_LOGE("MAIN", "check ON/OFF: ");
                uint8_t s_data[] = {0xB0, 0X03, 0x00};
                gatt_write_bt_data(BT_CHAR_UUID_READ_DEV_INFO,s_data, sizeof(s_data));
                vTaskDelay(pdMS_TO_TICKS(2000));
            }
            // uint8_t send_data[]={0xB2, 0X03, 0x01, last_state};
            // int bt_wr_state =  gatt_write_bt_data(BT_CHAR_UUID_SET_DEV_PARAM,send_data, sizeof(send_data));
            
            uint8_t send_data[] = {0xB0, 0X08, 0x00};
            int bt_wr_state =  gatt_write_bt_data(BT_CHAR_UUID_READ_DEV_INFO,send_data, sizeof(send_data));
            ESP_LOGW(TAG, "RET CHANGUE STATUS : %d\n", bt_wr_state);
            
            // break;
            vTaskDelay(pdMS_TO_TICKS(1000));
            uint8_t send_d1[] = {0xB0, 0X05, 0x00};
            bt_wr_state =  gatt_write_bt_data(BT_CHAR_UUID_READ_DEV_INFO,send_d1, sizeof(send_d1));
            ESP_LOGW(TAG, "RET CHANGUE STATUS : %d\n", bt_wr_state);
            vTaskDelay(pdMS_TO_TICKS(2000));

            mk115_get_copy_data(&data_mk115);
            print_MK115_bc_data(&data_mk115);
        }
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
  
    WAIT_S(3);
    vTaskDelete(NULL);
}

/*==============================================================
 * MAIN TASK
 *=============================================================*/
static void Main_Task(void *pvParameters)
{
    WAIT_S(10);
    /** data variable to data read */

    static uint32_t ctrl_server_ota = 0;

    WAIT_S(1);
    for (;;)
    {
        current_time = pdTICKS_TO_MS(xTaskGetTickCount()) / 1000;
        if (current_time % 30 == 0)
        {
            printf("Tiempo: %lu\r\n", current_time);
        }


        // SEND INFO DATA
        if ((pdTICKS_TO_MS(xTaskGetTickCount()) / 1000) >= Info_time)
        {
            current_time = pdTICKS_TO_MS(xTaskGetTickCount()) / 1000;
            Info_time += (uint32_t)delay_info * 60; // cada 1 min
            if (ret_update_time != MD_CFG_SUCCESS)
            {
                ret_update_time = Modem_update_time(1);
            }
            // -- actualizar estado de repoerte de tablero

            // Send_info_mqtt_md();
            int ret_send = Send_all_data_mqtt_md();
            ESP_LOGE(TAG, "ret update: %04X", ret_send);
            current_time = pdTICKS_TO_MS(xTaskGetTickCount()) / 1000;
            WAIT_S(1);
        }

        // ADD More condicions and functions//

        if ((pdTICKS_TO_MS(xTaskGetTickCount()) / 1000) >= OTA_md_time)
        {
            current_time = pdTICKS_TO_MS(xTaskGetTickCount()) / 1000;

            //------------------------//
            ctrl_server_ota++;

            if (ctrl_server_ota % 2 == 0)
            {
                OTA_Modem_Check(IP_OTA_01, PORT_OTA_01);
            }
            else
            {
                OTA_Modem_Check(IP_OTA_02, PORT_OTA_02);
            }

            printf("OTA CHECK tomo %lu segundos\r\n", (pdTICKS_TO_MS(xTaskGetTickCount()) / 1000 - current_time));
            current_time = pdTICKS_TO_MS(xTaskGetTickCount()) / 1000;
            OTA_md_time = current_time + 90;
            vTaskDelay(100);
        }

        if (Modem_check_AT() != MD_AT_OK)
        {
            WAIT_S(2);
            if (Modem_check_AT() != MD_AT_OK)
            {
                Modem_turn_OFF();
                // check if modem turn ON
                if (Active_modem() != MD_CFG_SUCCESS)
                    esp_restart();
                ret_update_time = Modem_update_time(1);
            }
        }

        WAIT_S(1);
    }
    vTaskDelete(NULL);
}

void app_main(void)
{
    ESP_LOGW(TAG, "--->> INIT PROJECT <<---");
    int ret_main = 0;
    /*--- Initialize NVS ---*/
    Init_NVS_Keys();
    Init_BLE_params();
#if DEBUG_OTA_ON
    register_ota_log_level();
#endif
    // Get configured OTA partition
    const esp_partition_t *configured = esp_ota_get_boot_partition();
    const esp_partition_t *running = esp_ota_get_running_partition();

    if (configured != running)
    {
        ESP_LOGW(TAG, "Configured OTA boot partition at offset 0x%08lx, but running from offset 0x%08lx",
                 configured->address, running->address);
        ESP_LOGW(TAG, "This can happen if the OTA partition table has been changed, or if the bootloader believes the OTA data is corrupted.");
    }
    else
    {
        ESP_LOGI(TAG, "Running from OTA partition at offset 0x%08lx", running->address);
    }

    // Init Modem params
    
    main_cfg_parms();
    Modem_config();

    // INIT OTA BLE
    WAIT_S(2);

    // INIT UART TASK
    xTaskCreate(Modem_rx_task, "M95_rx_task", 1024 * 10, NULL, configMAX_PRIORITIES - 1, &UART_task_handle); // active service

    ret_main = Active_modem();
    if (ret_main != MD_CFG_SUCCESS)
        esp_restart();

    ESP_LOGW(TAG, "-->> END CONFIG <<--\n");
    ret_update_time = Modem_update_time(3);
    ESP_LOGI(TAG, "RET update time: %d", ret_update_time);

    gb_data_md.time = time(NULL);
    gb_data_md.signal = Modem_get_signal();
    strcpy(gb_data_md.code, PROJECT_VER);

    // char* date=epoch_to_string(gb_data_md.time);
    ESP_LOGI(TAG, "IMEI: %s", gb_data_md.info.imei);
    ESP_LOGI(TAG, "ICID: %s", gb_data_md.info.iccid);
    ESP_LOGI(TAG, "FIRMWARE: %s", gb_data_md.info.firmware);
    ESP_LOGI(TAG, "UNIX: %lld", gb_data_md.time);
    ESP_LOGI(TAG, "CODE: %s", gb_data_md.code);
    ESP_LOGI(TAG, "SIGNAL: %d", gb_data_md.signal);

    cJSON *doc = cJSON_CreateObject();
    cJSON_AddItemToObject(doc, "imei", cJSON_CreateString(gb_data_md.info.imei));
    cJSON_AddItemToObject(doc, "project", cJSON_CreateString(PROJECT_NAME));
    cJSON_AddItemToObject(doc, "ota", cJSON_CreateString("true"));
    cJSON_AddItemToObject(doc, "cmd", cJSON_CreateString("false"));
    cJSON_AddItemToObject(doc, "sw", cJSON_CreateString("1.1"));
    cJSON_AddItemToObject(doc, "hw", cJSON_CreateString("1.1"));
    cJSON_AddItemToObject(doc, "otaV", cJSON_CreateString("1.0"));
    loggin_ota = cJSON_PrintUnformatted(doc);
    ESP_LOGI(TAG, "Mensaje OTA: %s", loggin_ota);
    cJSON_Delete(doc);

    OTA_md_time = pdTICKS_TO_MS(xTaskGetTickCount()) / 1000 + 60;
    MQTT_read_time = pdTICKS_TO_MS(xTaskGetTickCount()) / 1000 + 15;
    Info_time = pdTICKS_TO_MS(xTaskGetTickCount()) / 1000+(uint32_t)delay_info * 10;
    current_time = pdTICKS_TO_MS(xTaskGetTickCount()) / 1000;

    xTaskCreate(ble_chek_task,"ble_task",1024*6,NULL,8, &BLE_Task_handle);

    xTaskCreate(Main_Task, "Main_Task", 1024 * 8, NULL, 10, &MAIN_task_handle);
    xTaskCreate(GPRS_Wdt_Task, "Wdt_Task", 1024 * 2, NULL, 11, NULL);
    xTaskCreate(rs485_read_task, "icm_task", 1024 * 10, NULL, 5, &RS485_Task_handle);
    

}
