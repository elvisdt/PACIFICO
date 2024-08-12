#include "EG915_modem.h"
#include "credentials.h"
#include "modem_aux.h"
#include "errors_modem.h"

#include <time.h>
#include <sys/time.h>
#include <esp_task_wdt.h>
#include <esp_ota_ops.h>
#include "esp_log.h"

/**********************************************
 * DEFINES
 ***************************************************/

#define WAIT_MS(x) vTaskDelay(pdMS_TO_TICKS(x))
#define WAIT_S(x) vTaskDelay(pdMS_TO_TICKS(x * 1e3))

#define DEBUG_MODEM 0 // 0 -> NO DEBUG, 1-> DEBUG

// Definimos las funciones en la libreria
const char *TAG = "EG915";

/*****************************************************
 * VARIABLES
 ******************************************************/

uint8_t Rx_data[BUF_SIZE_MD]; //--
char *buff_reciv = (char *)&Rx_data[0];
char buff_send[BUF_SIZE_MD];

/*--> variables to control send and read AT <--*/
uint64_t actual_time_M95;
uint64_t idle_time_m95;

/*****************************************************
 * FUNCTIONS
 ******************************************************/

void Modem_config_gpio()
{
	ESP_LOGI(TAG, "--> GPIO CONFIG <--");

	ESP_ERROR_CHECK(gpio_reset_pin(modem_gpio.gpio_pwrkey));
	ESP_ERROR_CHECK(gpio_reset_pin(modem_gpio.gpio_status));

	gpio_reset_pin(modem_gpio.gpio_reset);
	gpio_set_direction(modem_gpio.gpio_reset, GPIO_MODE_OUTPUT);
	gpio_set_level(modem_gpio.gpio_reset, 0);

	ESP_ERROR_CHECK(gpio_set_direction(modem_gpio.gpio_pwrkey, GPIO_MODE_OUTPUT));
	ESP_ERROR_CHECK(gpio_set_direction(modem_gpio.gpio_status, GPIO_MODE_INPUT));
	//ESP_ERROR_CHECK(gpio_pulldown_en(modem_gpio.gpio_status));

	WAIT_MS(10);
}

void Modem_config_uart()
{
	ESP_LOGI(TAG, "--> UART CONFIG <--");
	uart_config_t uart_config_modem = {
		.baud_rate = modem_uart.baud_rate,
		.data_bits = UART_DATA_8_BITS,
		.parity = UART_PARITY_DISABLE,
		.stop_bits = UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
		.source_clk = UART_SCLK_DEFAULT,
	};

	// instalar el controlador UART
	esp_err_t ret = uart_driver_install(modem_uart.uart_num,
										BUF_SIZE_MD * 2,
										BUF_SIZE_MD * 2,
										configMAX_PRIORITIES - 1,
										&uart_md_queue, 0);

	if (ret == ESP_FAIL)
	{
		// Si la instalación falla, se desinstala y vuelve a instalar
		ESP_ERROR_CHECK(uart_driver_delete(modem_uart.uart_num));
		ESP_ERROR_CHECK(uart_driver_install(modem_uart.uart_num,
											BUF_SIZE_MD * 2,
											BUF_SIZE_MD * 2,
											configMAX_PRIORITIES - 1,
											&uart_md_queue, 1));
	}

	// Configure UART parameters
	ESP_ERROR_CHECK(uart_param_config(modem_uart.uart_num, &uart_config_modem));

	// Set UART pins as per KConfig settings
	ESP_ERROR_CHECK(uart_set_pin(modem_uart.uart_num,
								 modem_uart.gpio_uart_tx,
								 modem_uart.gpio_uart_rx,
								 UART_PIN_NO_CHANGE,
								 UART_PIN_NO_CHANGE));
	// Set uart pattern detect function.
	// uart_enable_pattern_det_baud_intr(modem_uart.uart_num, '+', PATTERN_CHR_NUM, 9, 0, 0);
	// Reset the pattern queue length to record at most 20 pattern positions.
	// uart_pattern_queue_reset(modem_uart.uart_num, 20);

	// Modem start: OFF
	uart_flush(modem_uart.uart_num);
	return;
}

void Modem_config()
{
	// Llama a las funciones de configuración
	ESP_LOGI(TAG, "--> CONFIG MODEM <--");
	Modem_config_gpio();
	Modem_config_uart();

	// Modem start: OFF
	gpio_set_level(modem_gpio.gpio_pwrkey, 1);
	WAIT_MS(50);
	return;
}

int sendAT(char *command, char *ok, char *error, uint32_t timeout, char *response)
{
	memset(response, '\0', strlen(response));

	int send_resultado = MD_AT_TIMEOUT;

	actual_time_M95 = esp_timer_get_time();
	idle_time_m95 = (uint64_t)(timeout * 1000);

#if DEBUG_MODEM
	ESP_LOGI("SEND_AT", "%s", command);
#endif

	rx_modem_ready = 0;
	uart_write_bytes(modem_uart.uart_num, (uint8_t *)command, strlen(command));

	while ((esp_timer_get_time() - actual_time_M95) < idle_time_m95)
	{
		if (rx_modem_ready == 0)
		{
			vTaskDelay(10 / portTICK_PERIOD_MS);
			continue;
		}
		if (strstr((char *)p_RxModem, ok) != NULL)
		{
			send_resultado = MD_AT_OK;
			break;
		}
		else if (strstr((char *)p_RxModem, error) != NULL)
		{
			send_resultado = MD_AT_ERROR;
			break;
		}
		rx_modem_ready = 0;
	}

	if (send_resultado == MD_AT_TIMEOUT)
	{
		ESP_LOGE("RECIB_AT", "TIMEOUT");
		return send_resultado;
	}

	strcpy(response, (char *)p_RxModem);
#if DEBUG_MODEM
	if (send_resultado == MD_AT_OK)
	{
		ESP_LOGW("RECIB_AT", "%s", response);
	}
	else
	{
		ESP_LOGE("RECIB_AT", "%s", response);
	}
#endif

	return send_resultado;
}

int readAT(char *ok, char *error, uint32_t timeout, char *response)
{

	memset(response, '\0', strlen(response));
	int correcto = MD_AT_TIMEOUT;

	uint64_t n = esp_timer_get_time();
	rx_modem_ready = 0;

	while (esp_timer_get_time() < (n + timeout * 1000))
	{
		vTaskDelay(1);
		if (rx_modem_ready == 1)
		{
			break;
		}
	}
	if (rxBytesModem > 0)
	{
		memcpy(response, p_RxModem, rxBytesModem);
		// check if the response is correct
		if (strstr(response, ok) != NULL)
		{
			correcto = MD_AT_OK;
		}
		// check if there is an error
		else if (strstr(response, error) != NULL)
		{
			correcto = MD_AT_ERROR;
		}
	}

#if DEBUG_MODEM
	if (correcto != MD_AT_TIMEOUT)
	{
		printf(response);
		printf("\r\n");
	}
#endif
	return correcto;
}

int Modem_turn_ON()
{
	ESP_LOGI(TAG, "=> TURN ON MODULE");
	int ret = 0;
	WAIT_S(2);

	// --- COMPROBAR UART ----- //
	ret = Modem_check_AT();
	if (ret == MD_AT_OK)
		return MD_CFG_SUCCESS;

	gpio_set_level(modem_gpio.gpio_pwrkey, 0);
	WAIT_MS(500);
	gpio_set_level(modem_gpio.gpio_pwrkey, 1);
	WAIT_MS(2000); // >= 2s
	gpio_set_level(modem_gpio.gpio_pwrkey, 0);
	WAIT_S(10);

	// --- COMPROBAR UART ----- //
	if (gpio_get_level(modem_gpio.gpio_status))
	{							   //
		return Modem_check_uart(); // MD_CFG_SUCCESS, MD_CFG_FAIL
	}
	WAIT_S(1);
	return MD_CFG_FAIL;
}

int Modem_check_AT()
{
	int ret = 0;
	// ESP_LOGI(TAG, "CHECK COMMAND AT");
	uart_flush(modem_uart.uart_num);

	WAIT_MS(500);
	ret = sendAT("AT\r\n", "OK\r\n", "ERROR\r\n", 2000, buff_reciv);
	WAIT_MS(100);
	return ret; // MD_AT_OK, MD_AT_ERROR MD_AT_TIMEOUT
}

int Modem_turn_OFF()
{
	ESP_LOGI(TAG, "=> Turn off Module by PWRKEY");
	int status = gpio_get_level(modem_gpio.gpio_status);
	if (status != 0)
	{
		gpio_set_level(modem_gpio.gpio_pwrkey, 0);
		WAIT_MS(100);
		gpio_set_level(modem_gpio.gpio_pwrkey, 1);
		WAIT_MS(3100); // >3s //
		gpio_set_level(modem_gpio.gpio_pwrkey, 0);
		WAIT_MS(1000);
	}
	status = gpio_get_level(modem_gpio.gpio_status);
	if (status == 0)
	{
		return MD_CFG_SUCCESS;
	}

	return MD_CFG_FAIL;
}

int Modem_turn_OFF_command()
{
	int ret = sendAT("AT+QPOWD=1\r\n", "OK\r\n", "FAILED\r\n", 1000, buff_reciv);
	if (ret != MD_AT_OK)
	{
		return sendAT("AT+QPOWD=0\r\n", "OK\r\n", "FAILED\r\n", 1000, buff_reciv);
	}
	return ret; // MD_AT_OK, MD_AT_ERROR MD_AT_TIMEOUT
}

void Modem_reset()
{
	ESP_LOGI(TAG, "=> RESET MODEM");
	gpio_set_level(modem_gpio.gpio_reset, 0);
	WAIT_MS(100);
	gpio_set_level(modem_gpio.gpio_reset, 1);
	WAIT_MS(110);
	gpio_set_level(modem_gpio.gpio_reset, 0);
	WAIT_MS(2000);
}

int Modem_check_uart()
{
	// Enviamos el commando AT
	char *com = "AT\r\n";

	// Comando para iniciar comunicacion
	rx_modem_ready = 0;
	uart_write_bytes(modem_uart.uart_num, (uint8_t *)com, strlen(com));

	// Intentos
	int a = 2;
	ESP_LOGI("MODEM", "Esperando respuesta ...");

	// rx_modem_ready = 0;
	while (a > 0)
	{
		if ((readAT("OK", "ERROR", 1500, buff_reciv) == MD_AT_OK))
		{
			printf("  Modem is active\n");
			return MD_CFG_SUCCESS; // OK
		}
		WAIT_MS(300);
		a--;
	}
	return MD_CFG_FAIL; // fail
}

int Modem_begin_commands()
{
	WAIT_MS(10);
	ESP_LOGI(TAG, "=> CONFIG FIRST AT COMMANDS");
	uart_flush(modem_uart.uart_num);

	int ret = sendAT("AT\r\n", "OK\r\n", "ERROR\r\n", 5000, buff_reciv);
	WAIT_MS(200);
	if (ret != MD_AT_OK)
		return ret;

	// restablecer parametros de fabrica
	sendAT("AT&F\r\n", "OK\r\n", "ERROR\r\n", 1000, buff_reciv);
	WAIT_MS(300);

	// Disable local echo mode
	sendAT("ATE0\r\n", "OK\r\n", "ERROR\r\n", 5000, buff_reciv);
	WAIT_MS(500);

	// Disable "result code" prefix
	sendAT("AT+CMEE=0\r\n", "OK\r\n", "ERROR\r\n", 5000, buff_reciv);
	WAIT_S(1);

	// Indicate if  chip is OKEY
	ret = sendAT("AT+CPIN?\r\n", "READY", "ERROR\r\n", 5000, buff_reciv);
	if (ret!= MD_AT_OK){
		return ret;
	}
	ESP_LOGI(TAG,"MODEM CPIN: READY");
	WAIT_S(5);

	//--- Operator Selection ---//
	ret = sendAT("AT+COPS=0\r\n", "OK", "ERROR\r\n", 180000, buff_reciv);
	WAIT_MS(200);
	if (ret!= MD_AT_OK){
		return ret;
	}
	// Signal Quality - .+CSQ: 6,0  muy bajo
	sendAT("AT+CSQ\r\n", "+CSQ:", "ERROR\r\n", 400, buff_reciv);
	WAIT_MS(200);

	// if (bandera !=1) return bandera;

	sendAT("AT+COPS?\r\n", "OK\r\n", "ERROR\r\n", 50000, buff_reciv);
	WAIT_MS(200);

	sendAT("AT+QICSGP=1\r\n", "OK\r\n", "ERROR", 100000, buff_reciv);
	WAIT_MS(500);

	// APN CONFIG
	sprintf(buff_send, "AT+QICSGP=1,1,\"%s,\"\",\"\",1\r\n", APN);
	sendAT(buff_send, "OK\r\n", "ERROR\r\n", 500, buff_reciv);
	WAIT_MS(200);

	// Activate a PDP Context
	sendAT("AT+QIACT=1\r\n", "OK", "ERROR", 100000, buff_reciv);
	WAIT_MS(100);

	sendAT("AT+QIACT?\r\n", "OK", "ERROR", 10000, buff_reciv);
	WAIT_MS(100);

	// activate PDP
	sendAT("AT+CGACT?\r\n", "OK", "ERROR", 100000, buff_reciv);
	WAIT_MS(100);

	sendAT("AT+CGACT=1,1\r\n", "OK", "ERROR", 100000, buff_reciv);
	WAIT_MS(100);

	sendAT("AT+CGPADDR=1\r\n", "OK", "ERROR", 500, buff_reciv);
	WAIT_MS(100);

	// Enable Network register
	sendAT("AT+CGREG=1\r\n", "OK\r\n", "ERROR\r\n", 400, buff_reciv);
	WAIT_MS(200);

	// chek Network Registration Status
	ret = sendAT("AT+CGREG?\r\n", "1,", "ERROR\r\n", 400, buff_reciv);
	WAIT_MS(200);

	sendAT("AT+CSMS?\r\n", "OK\r\n", "ERROR\r\n", 400, buff_reciv);
	WAIT_MS(200);

	// config sms
	sendAT("AT+CSMS=1\r\n", "OK\r\n", "ERROR\r\n", 400, buff_reciv);
	WAIT_MS(200);

	return ret; // MD_AT_OK, MD_AT_ERROR, MD_AT_TIMEOUT
}

int Modem_sync_time(char *response)
{
	char *ptr = strstr(response, "+QNTP: 0");
	if (ptr)
	{
		char *start = strchr(ptr, '\"');
		if (start)
		{
			start++;
			char *end = strchr(start, '\"');
			if (end)
			{
				*end = '\0'; // Temporalmente se interrumpe la cadena para analizarla
				// Se parsea la cadena de tiempo en una estructura de tiempo
				struct tm tm;
				struct timeval stv;
				memset(&tm, 0, sizeof(struct tm));
				strptime(start, "%Y/%m/%d,%H:%M:%S", &tm);

				// Se convierte la estructura de tiempo en un tiempo de epoch
				time_t epoch_time = mktime(&tm);
				// Se establece el nuevo tiempo del sistema
				stv.tv_sec = epoch_time;
				stv.tv_usec = 0;
				settimeofday(&stv, NULL);

				*end = '\"';		   // Se restaura la cadena original
				return MD_CFG_SUCCESS; // UPDATE OK
			}
		}
	}
	return MD_CFG_FAIL; // FAIL UPDATE
}

// Función principal para actualizar la hora
int Modem_update_time(uint8_t max_int)
{
	ESP_LOGI(TAG, "=> UPDATE TIME ");
	int ret = 0;
	char *servidor = "time.google.com"; // Servidor NTP inicial
	for (size_t i = 0; i < max_int; i++)
	{
		sprintf(buff_send, "AT+QNTP=1,\"%s\",123,1\r\n", servidor);
		WAIT_MS(100);
		if (sendAT(buff_send, "+QNTP:", "ERROR", 70000, buff_reciv) == MD_AT_OK)
		{
			ret = Modem_sync_time(buff_reciv);
			if (ret == MD_CFG_SUCCESS)
				return ret; // OK UPDATE

			if (strcmp(servidor, "time.google.com") == 0)
			{
				servidor = "pool.ntp.org";
			}
			else
			{
				servidor = "time.google.com";
			}
		}
	}
	return MD_CFG_FAIL; // FAIL UPDATE
}

char *Modem_get_date()
{
	int a = sendAT("AT+CCLK?\r\n", "+CCLK: ", "ERROR", 1000, buff_reciv);
	if (a != MD_AT_OK)
	{
		printf("  hora no conseguida\n\r");
		return NULL;
	}
	char lim[3] = "\"";
	char *p_clock = strtok(buff_reciv, lim);
	while (p_clock != NULL)
	{
		if (strstr(p_clock, "/") != NULL)
		{
			break;
		}
		p_clock = strtok(NULL, lim);
		// printf("%s\n",p_clock);
	}
	return (char *)p_clock;
}

time_t Modem_get_date_epoch()
{

	char *date_str = Modem_get_date();
	if (date_str == NULL)
	{
		return -1;
	}

	struct tm timeinfo = {0};
	int year, month, day, hour, minute, second;

	// Parsea la fecha y hora de la cadena de texto
	sscanf(date_str, "%2d/%2d/%2d,%2d:%2d:%2d", &year, &month, &day, &hour, &minute, &second);

	timeinfo.tm_year = year + 100; // Los años se cuentan desde 1900
	timeinfo.tm_mon = month - 1;   // Los meses se cuentan desde 0
	timeinfo.tm_mday = day;
	timeinfo.tm_hour = hour; // La diferencia de Horas de formato UTC global
	timeinfo.tm_min = minute;
	timeinfo.tm_sec = second;

	// Convierte struct tm a tiempo epoch
	time_t epoch_time = mktime(&timeinfo);

	return epoch_time;
}

int Modem_get_IMEI(char *imei)
{

	ESP_LOGI(TAG, "=> READ IMEI");
	int state_imei = sendAT("AT+QGSN\r\n", "+QGSN", "ERROR", 2000, buff_reciv);
	WAIT_MS(200);

	if (state_imei == MD_AT_OK)
	{
		char *inicio = strstr(buff_reciv, "+QGSN: ");
		if (inicio != NULL)
		{
			sscanf(inicio, "+QGSN: %15s", imei);
			return MD_CFG_SUCCESS; // IMEI encontrado
		}
	}
	return MD_CFG_FAIL; // IMEI no encontrado
}

int Modem_get_ICCID(char *iccid)
{

	ESP_LOGI(TAG, "=> READ ICCID");

	memset(iccid, '\0', strlen(iccid));
	int state = sendAT("AT+QCCID\r\n", "OK\r\n", "ERROR\r\n", 500, buff_reciv);
	WAIT_MS(200);

	if (state == MD_AT_OK)
	{
		char *inicio = strstr(buff_reciv, "+QCCID: ");
		if (inicio != NULL)
		{
			int ret = sscanf(inicio, "+QCCID: %20s", iccid);
			if (ret == 1)
			{
				return MD_CFG_SUCCESS; // ICCID encontrado
			}
		}
	}
	return MD_CFG_FAIL; // ICCD no encontrado
}

int Modem_get_firmware(char *firmare)
{
	ESP_LOGI(TAG, "=> READ FIRMWARE");
	int ret = sendAT("AT+GMR=?\r\n", "OK", "ERROR", 500, buff_reciv);
	if (ret != MD_AT_OK)
		return MD_CFG_FAIL;
	WAIT_MS(200);

	ret = sendAT("AT+GMR\r\n", "OK\r\n", "ERROR", 500, buff_reciv);
	WAIT_MS(50);

	if (ret == MD_AT_OK)
	{
		char *inicio = strstr(buff_reciv, "EG915");
		if (inicio != NULL)
		{
			int ret = sscanf(inicio, "%20s", firmare);
			if (ret == 1)
			{
				return MD_CFG_SUCCESS; // firmware encontrado
			}
		}
	}
	return MD_CFG_FAIL;
}

int Modem_get_dev_info(EG915_info_t *dev_modem)
{
	int ret = 0;

	ret = Modem_get_ICCID(dev_modem->iccid);
	if (ret != MD_CFG_SUCCESS)
		return MD_CFG_FAIL;

	ret = Modem_get_IMEI(dev_modem->imei);
	if (ret != MD_CFG_SUCCESS)
		return MD_CFG_FAIL;

	ret = Modem_get_firmware(dev_modem->firmware);
	if (ret != MD_CFG_SUCCESS)
		return MD_CFG_FAIL;

	return MD_CFG_SUCCESS;
}

int Modem_get_signal()
{
	ESP_LOGI(TAG, "=> MODEM GET SIGNAL");
	char *linea;
	int valor1, valor2;
	int a = sendAT("AT+CSQ\r\n", "+CSQ", "ERROR", 500, buff_reciv);
	if (a == MD_AT_OK)
	{
		linea = strtok(buff_reciv, "\n");
		while (linea != NULL)
		{
			// Intenta extraer los valores de la línea
			if (sscanf(linea, "+CSQ: %d,%d", &valor1, &valor2) == 2)
			{
				return valor1;
			}
			// Obtiene la siguiente línea
			linea = strtok(NULL, "\n");
		}
	}
	return 99;
}

/*---------------MQTT---------------*/

int Modem_Mqtt_Open(int idx, const char *MQTT_IP, uint16_t MQTT_PORT)
{
	int ret = 0;
	char respuesta_esperada[20] = {0};

	// config Params to MQTT
	sprintf(buff_send, "AT+QMTCFG=\"pdpcid\",%d\r\n", idx);
	sendAT(buff_send, "OK\r\n", "ERROR\r\n", 500, buff_reciv);
	WAIT_MS(50);

	sprintf(buff_send, "AT+QMTCFG=\"version\",%d,3\r\n", idx);
	sendAT(buff_send, "OK\r\n", "ERROR\r\n", 500, buff_reciv);
	WAIT_MS(50);

	sprintf(buff_send, "AT+QMTCFG=\"recv/mode\",%d,0,1\r\n", idx);
	sendAT(buff_send, "OK", "ERROR\r\n", 500, buff_reciv);
	WAIT_MS(50);

	sprintf(buff_send, "AT+QMTCFG=\"timeout\",%d,10,3,0\r\n", idx);
	sendAT(buff_send, "OK", "ERROR\r\n", 500, buff_reciv);
	WAIT_MS(50);

	sendAT("AT+QMTOPEN?\r\n", "OK", "ERROR\r\n", 1000, buff_reciv);
	WAIT_MS(500);

	sprintf(buff_send, "AT+QMTOPEN=%d,\"%s\",%u\r\n", idx, MQTT_IP, MQTT_PORT);
	sprintf(respuesta_esperada, "+QMTOPEN: %d,", idx);
	ret = sendAT(buff_send, respuesta_esperada, "ERROR\r\n", 100000, buff_reciv);
	WAIT_MS(200);

	if (ret != MD_AT_OK)
	{
		return MD_CFG_FAIL; // ERROR DE RESPUESTA
	}

	char *p = strchr(buff_reciv, ',') + 1;
	if (p != NULL)
	{
		int val_open = atoi(p);
		if (val_open == -1)
		{
			ESP_LOGW(TAG, "Failed to open network");
			return MD_MQTT_OPEN_FAIL_NET;
		}
		else if (val_open == 0)
		{
			ESP_LOGI(TAG, "Network opened successfully");
			return MD_MQTT_OPEN_OK;
		}
		else if (val_open == 1)
		{
			ESP_LOGW(TAG, "Wrong parameter");
			return MD_MQTT_OPEN_FAIL_PRM;
		}
		else if (val_open == 2)
		{
			ESP_LOGI(TAG, "identifier is occupied");
			return MD_MQTT_OPEN_FAIL_IDX;
		}
		else if (val_open == 3)
		{
			ESP_LOGI(TAG, "Failed to activate PDP");
			return MD_MQTT_OPEN_FAIL_PDP;
		}
		else if (val_open == 4)
		{
			ESP_LOGI(TAG, "Failed to parse domain name");
			return MD_MQTT_OPEN_FAIL_DOM;
		}
		else if (val_open == 5)
		{
			ESP_LOGI(TAG, "Network connection error");
			return MD_MQTT_OPEN_FAIL_CNN;
		}
		else
		{
			return MD_CFG_FAIL;
		}
	}
	return MD_CFG_FAIL; // ERROR DE RESPUESTA ;
}

int Modem_Mqtt_CheckOpen(int idx, char *MQTT_IP, uint16_t MQTT_PORT)
{
	char res_esperada[50] = {0};
	sprintf(res_esperada, "+QMTOPEN: %d,", idx);
	int ret = sendAT("AT+QMTOPEN?\r\n", "OK\r\n", "ERROR\r\n", 10000, buff_reciv);
	WAIT_MS(200);
	if (ret == MD_AT_OK)
	{
		if (strstr(buff_reciv, res_esperada) != NULL)
		{
			return MD_MQTT_IS_OPEN; // Mqtt is Openning;
		}
		else
		{
			return MD_MQTT_NOT_OPEN; // Mqtt is not Openning
		}
	}
	return MD_CFG_FAIL;
}

int Modem_Mqtt_Conn(int idx, const char *clientID)
{
	// ESP_LOGI(TAG,"--> MQTT CONNECTION <--");

	char res_esperada[20] = {0};
	sprintf(res_esperada, "+QMTCONN: %d,", idx);
	sprintf(buff_send, "AT+QMTCONN=%d,\"%s\"\r\n", idx, clientID);
	int ret = sendAT(buff_send, res_esperada, "ERROR\r\n", 10000, buff_reciv);
	WAIT_MS(100);

	if (ret != MD_AT_OK)
	{
		return MD_CFG_FAIL; // ERROR DE RESPUESTA
	}

	char *p = strchr(buff_reciv, ',') + 1;
	if (p != NULL)
	{
		int val_conn = atoi(p);
		if (val_conn == 0)
		{
			ESP_LOGI(TAG, "Connection Accepted");
			return MD_MQTT_CONN_OK;
		}
		else if (val_conn == 1)
		{
			ESP_LOGW(TAG, "Unacceptable Protocol Version");
			return MD_MQTT_CONN_FAIL_VER;
		}
		else if (val_conn == 2)
		{
			ESP_LOGW(TAG, "Server Unavailable");
			return MD_MQTT_CONN_FAIL_SRV;
		}
		else if (val_conn == 3)
		{
			ESP_LOGW(TAG, "Bad Username or Password");
			return MD_MQTT_CONN_FAIL_UPS;
		}
		else if (val_conn == 4)
		{
			ESP_LOGW(TAG, " Not Authorized");
			return MD_MQTT_CONN_FAIL_AUT;
		}
		else
		{
			return MD_CFG_FAIL;
		}
	}
	return MD_CFG_FAIL;
}

int Modem_Mqtt_CheckConn(int idx)
{
	// ESP_LOGI(TAG, "==> MQTT CHECK CONNECTION");
	char res_esperada[30] = {0};
	sprintf(res_esperada, "+QMTCONN: %d,", idx);

	int ret = sendAT("AT+QMTCONN?\r\n", "OK\r\n", "ERROR\r\n", 5000, buff_reciv);
	WAIT_MS(100);

	if (ret != MD_AT_OK)
	{
		return MD_MQTT_CONN_ERROR;
	}

	int result;
	remove_spaces(buff_reciv);
	ret = sscanf(buff_reciv, "+QMTCONN:%*d,%d", &result);
	if (ret == 1)
	{
		if (result == 1)
		{
			ESP_LOGI(TAG, "is initializing");
			return MD_MQTT_CONN_INIT;
		}
		else if (result == 2)
		{
			ESP_LOGI(TAG, "is connecting");
			return MD_MQTT_CONN_CONNECT;
		}
		else if (result == 3)
		{
			ESP_LOGI(TAG, "is connected");
			return MD_MQTT_CONN_OK;
		}
		else if (result == 4)
		{
			ESP_LOGI(TAG, "is disconnecting");
			return MD_MQTT_CONN_DISCONNECT;
		}
		else
		{
			return MD_MQTT_CONN_ERROR;
		}
	}
	return MD_MQTT_CONN_ERROR;
}

int Modem_Mqtt_Close(int idx)
{
	sprintf(buff_send, "AT+QMTCLOSE=%d\r\n", idx);
	int ret = sendAT(buff_send, "+QMTCLOSE:", "ERROR", 30000, buff_reciv);
	WAIT_MS(100);
	return ret; // MD_AT_OK, MD_AT_ERROR MD_AT_TIMEOUT
}

int Modem_Mqtt_Disconnect(int idx)
{
	int ret;
	sprintf(buff_send, "AT+QMTDISC=%d\r\n", idx);
	ret = sendAT(buff_send, "+QMTDISC:", "ERROR\r\n", 30000, buff_reciv);
	WAIT_MS(100);
	return ret; // MD_AT_OK, MD_AT_ERROR MD_AT_TIMEOUT
}

int Modem_Mqtt_Pub(char *data, char *topic, int data_len, int id, int retain)
{
	int ret = 0;
	sprintf(buff_send, "AT+QMTPUBEX=%d,1,1,%d,\"%s\",%d\r\n", id, retain, topic, data_len);
	ret = sendAT(buff_send, ">", "ERROR", 5000, buff_reciv);
	if (ret != MD_AT_OK)
	{
		return MD_CFG_FAIL;
	}
	uart_flush(modem_uart.uart_num);
	uart_write_bytes(modem_uart.uart_num, data, data_len);
	uart_wait_tx_done(modem_uart.uart_num, 5000);

	ret = readAT("+QMTPUBEX", "ERROR", 15000, buff_reciv);
	WAIT_MS(100);
	if (ret == MD_AT_OK)
		return MD_MQTT_PUB_OK;
	return MD_MQTT_PUB_FAIL;
}

int Modem_Mqtt_Sub(int idx, char *topic_name)
{
	// ESP_LOGI(TAG, "--> MQTT SUBS TOPIC <--");
	int ret = 0;
	char res_esperada[30] = {0};
	sprintf(res_esperada, "+QMTSUB: %d,", idx);
	sprintf(buff_send, "AT+QMTSUB=%d,1,\"%s\",0\r\n", idx, topic_name);
	ret = sendAT(buff_send, res_esperada, "ERROR\r\n", 20000, buff_reciv);
	WAIT_MS(100);
	if (ret != MD_AT_OK)
		return MD_CFG_FAIL;

	int result;
	remove_spaces(buff_reciv);
	printf("SUBS: %s \r\n", buff_reciv);

	ret = sscanf(buff_reciv, "+QMTSUB:%*d,%*d,%d,%*d", &result);
	if (ret == 1)
	{
		if (result == 0)
		{
			return MD_MQTT_SUB_OK;
		}
		else if (result == 0)
		{
			return MD_MQTT_SUB_RETP;
		}
		else if (result == 0)
		{
			return MD_MQTT_SUB_FAIL;
		}
		else
		{
			return MD_MQTT_SUB_UNKNOWN;
		}
	}
	return MD_CFG_FAIL;
}

int Modem_Mqtt_Check_Buff(int idx, uint8_t status_buff[5])
{
	// ESP_LOGI(TAG, "--> MQTT READ SUBS TOPIC FROM BUFFER <--");
	int ret = 0;
	char res_esperada[30] = {0};
	sprintf(res_esperada, "+QMTRECV: %d,", idx);
	ret = sendAT("AT+QMTRECV?\r\n", "OK\r\n", "ERROR\r\n", 20000, buff_reciv);

	WAIT_MS(100);
	if (ret != MD_AT_OK)
	{
		return MD_CFG_FAIL;
	}
	// printf("RESPONSE: %s\r\n",buff_reciv);
	if (strstr(buff_reciv, res_esperada) != NULL)
	{
		int ss0, ss1, ss2, ss3, ss4;
		remove_spaces(buff_reciv);
		// printf("RESPONSE: %s\r\n",buff_reciv);
		ret = sscanf(buff_reciv, "+QMTRECV:%*d,%d,%d,%d,%d,%d", &ss0, &ss1, &ss2, &ss3, &ss4);
		if (ret == 5)
		{
			status_buff[0] = ss0;
			status_buff[1] = ss1;
			status_buff[2] = ss2;
			status_buff[3] = ss3;
			status_buff[4] = ss4;
			if (ss0 == 1 || ss1 == 1 || ss2 == 1 || ss3 == 1 || ss4 == 1)
			{
				return MD_MQTT_RECV_BUFF_DATA;
			}
			else
			{
				return MD_MQTT_RECV_BUFF_CLEAN;
			}
		}
		else
		{
			return MD_MQTT_RECV_UNKOWN;
		}
	}
	return MD_MQTT_RECV_FAIL;
}

int Modem_Mqtt_Read_data(int idx, int num_mem, char *response)
{
	ESP_LOGI(TAG, "--> MQTT READ SUBS TOPIC FROM BUFFER <--");
	int ret = 0;
	memset(response, '\0', strlen(response));
	char res_esperada[] = "+QMTRECV: ";
	sprintf(buff_send, "AT+QMTRECV=%d,%d\r\n", idx, num_mem);
	ret = sendAT(buff_send, "OK\r\n", "ERROR\r\n", 20000, buff_reciv);
	WAIT_MS(200);

	if (ret != MD_AT_OK)
	{
		return MD_MQTT_READ_FAIL;
	}
	if (strstr(buff_reciv, res_esperada) != NULL)
	{
		remove_word_from_string(buff_reciv, res_esperada);
		remove_word_from_string(buff_reciv, "OK\r\n");
		remove_newlines(buff_reciv);
		strcpy(response, buff_reciv);
		return MD_MQTT_READ_OK;
	}
	return MD_MQTT_READ_NO_FOUND;
}

int Modem_Mqtt_Unsub(int idx, char *topic_name)
{
	sprintf(buff_send, "AT+QMTUNS=%d,1,\"%s\"\r\n", idx, topic_name);
	int a = sendAT(buff_send, "+QMTUNS:", "ERROR", 12000, buff_reciv);
	WAIT_MS(100);
	return a; // MD_AT_OK, MD_AT_ERROR MD_AT_TIMEOUT
}

int Modem_Mqtt_Sub_Topic(int idx, char *topic_name, char *response)
{

	memset(response, '\0', strlen(response));

	sprintf(buff_send, "AT+QMTSUB=%d,1,\"%s\",0\r\n", idx, topic_name);
	int success = sendAT(buff_send, "+QMTRECV:", "ERROR\r\n", 00000, buff_reciv);

	// printf("buf data reciv: %s\r\n",buff_reciv);
	if (success != MD_AT_OK)
	{
		ESP_LOGE("MQTT Subs", "Not data: %s\r\n", topic_name);
		return MD_CFG_FAIL;
	}

	// +QMTRECV: <client_idx>,<msgid>,<topic>[,<payload_len>],<payload>
	// "+QMTRECV: 0,0,\"OTA/868695060088992/CONFIG\",17,\"{\"value\":278}\""
	int payload_len;
	printf("data read: %s\n\n", buff_reciv);

	// Utiliza sscanf para extraer el valor de payload_len
	if (sscanf(buff_reciv, "\r\n+QMTRECV: %*d,%*d,%*[^,],%d,", &payload_len) == 1)
	{
		printf("len pylad: %d\r\n", payload_len);
		extraer_ultimos(buff_reciv, (payload_len + 4), response);
		remove_spaces(response);
		// eliminar las comillas del el primero y ultimo valor
		size_t longitud = strlen(response);
		if (longitud > 2)
		{
			strncpy(response, response + 1, longitud - 2);
			response[longitud - 2] = '\0';
		}
		// printf("DATA EXTRACT: %s\n", response);
		return MD_CFG_SUCCESS;
	}
	return MD_CFG_FAIL;
}

/***************************************************************
 * SMS
 **************************************************************/

int Modem_SMS_Read(char *mensaje, char *numero)
{
	int ret = 0;

	sendAT("AT+CPMS=?\r\n", "OK\r\n", "ERROR\r\n", 5000, buff_reciv);
	WAIT_MS(100);

	sendAT("AT+CPMS?\r\n", "OK\r\n", "ERROR\r\n", 5000, buff_reciv);
	WAIT_MS(100);

	ret = sendAT("AT+CMGF=1\r\n", "OK\r\n", "ERROR\r\n", 5000, buff_reciv);
	WAIT_MS(100);

	ret = sendAT("AT+CMGL=?\r\n", "OK", "ERROR\r\n", 1000, buff_reciv);
	WAIT_MS(100);
	if (ret != MD_AT_OK)
		return MD_CFG_FAIL;

	ret = sendAT("AT+CMGL=\"REC UNREAD\"\r\n", "OK\r\n", "ERROR", 1000, buff_reciv);
	WAIT_MS(100);
	if (ret != MD_AT_OK)
		return MD_CFG_FAIL;

	// CMGR

	// verifica la lista de mensajes
	if (strstr(buff_reciv, "+CMGL:") == NULL)
	{
		// en caso no encontrar retorna no encontrado
		return MD_SMS_READ_NO_FOUND;
	}
	// continuar para procesar
	printf("RECIV SMS: %s\n", buff_reciv);
	data_sms_strt_t sms_data;
	ret = str_to_data_sms(buff_reciv, &sms_data);

	// leer lo de la memoria 0,
	sendAT("AT+CMGR=0\r\n", "OK\r\n", "ERROR", 1000, buff_reciv);
	WAIT_MS(100);

	// leer lo de la memoria 1
	sendAT("AT+CMGR=1\r\n", "OK\r\n", "ERROR", 1000, buff_reciv);
	WAIT_MS(100);

	if (sms_data.lines >= 2)
	{
		// line 0
		if (find_phone_and_extract(sms_data.data[0], numero) == 0)
		{
#if DEBUG_MODEM

			ESP_LOGW(TAG, "phone: %s", numero);
#endif
		}
		else
		{
			free_data(&sms_data);
			return MD_SMS_READ_UNKOWN;
		}
		// line 1
		strcpy(mensaje, sms_data.data[1]);
#if DEBUG_MODEM
		ESP_LOGW(TAG, "msg: %s", mensaje);
#endif
		free_data(&sms_data);
		return MD_SMS_READ_FOUND;
	}
	else
	{
		free_data(&sms_data);
		return MD_SMS_READ_UNKOWN;
	}
}

int Modem_SMS_Send(char *mensaje, char *numero)
{
	int ret = 0;
	memset(buff_send, '\0', strlen(buff_send));

	ret = sendAT("AT+CMGF=1\r\n", "OK\r\n", "ERROR\r\n", 5000, buff_reciv);
	WAIT_MS(100);

	sprintf(buff_send, "AT+CMGS=\"%s\"\r\n", numero);
	ret = sendAT(buff_send, ">", "ERROR\r\n", 5000, buff_reciv);
	if (ret != MD_AT_OK)
		return MD_CFG_FAIL;

	memset(buff_send, '\0', strlen(buff_send));
	sprintf(buff_send, "%s%c", mensaje, 26); // sms + 'Ctrl+Z'
	uart_flush(modem_uart.uart_num);
	uart_write_bytes(modem_uart.uart_num, buff_send, strlen(buff_send));
	uart_wait_tx_done(modem_uart.uart_num, pdMS_TO_TICKS(120000));
	ret = readAT("+CMGS", "ERROR", 20000, buff_reciv);
	if (ret == MD_AT_OK)
	{
		return MD_SMS_SEND_OK;
	}
	return MD_SMS_SEND_FAIL;
}

int Modem_SMS_delete()
{
	int ret = 0;
	// ret = sendAT("AT+CMGR=1\r\n","OK\r\n","ERROR",1000,buff_reciv);

	ret = sendAT("AT+CMGD=0,4\r\n", "OK\r\n", "ERROR\r\n", 5000, buff_reciv);
	vTaskDelay(pdMS_TO_TICKS(200));
	ret = sendAT("AT+CMGD=1,4\r\n", "OK\r\n", "ERROR\r\n", 5000, buff_reciv);
	vTaskDelay(pdMS_TO_TICKS(200));

	return ret; // MD_AT_OK, MD_AT_ERROR, MD_AT_TIMEOUT
}

/************************************
 * TCP
 ****************************************/

int TCP_open(const char *ip_tcp, uint16_t port_tcp, int connID)
{
	int ret = 0;
	ESP_LOGI(TAG, "==> MODEM OPEN TCP <==");
	ret = sendAT("AT+QICSGP=1\r\n", "OK\r\n", "ERROR", 10000, buff_reciv);
	WAIT_MS(100);
	if (ret == MD_AT_OK)
	{
		// verificamos si el APN se encuantra configurado o no
		if (strstr(buff_reciv, APN) == NULL)
		{
			// APN config
			sprintf(buff_send, "AT+QICSGP=1,1,\"%s,\"\",\"\",1\r\n", APN);
			sendAT(buff_send, "OK\r\n", "ERROR", 10000, buff_reciv);
			WAIT_MS(100);
		}
	}

	// Activate a PDP Context
	sendAT("AT+QIACT=1\r\n", "OK", "ERROR", 100000, buff_reciv);
	WAIT_MS(100);

	sendAT("AT+QIACT?\r\n", "OK", "ERROR", 10000, buff_reciv);
	WAIT_MS(100);

	// AT+QICFG="transpktsize"[,<transpktsize>]
	// AT+QICFG="transwaittm"[,<transwaittm>]
	// AT+QICFG="dataformat"[,<send_data_format>,<recv_data_format>]

	// 1KB pksize
	sendAT("AT+QICFG=\"transpktsize\",1024\r\n", "OK\r\n", "ERROR", 10000, buff_reciv);
	WAIT_MS(100);

	// Range: 0–20. Default value: 2. Unit: ms.
	sendAT("AT+QICFG=\"transwaittm\",10\r\n", "OK\r\n", "ERROR", 10000, buff_reciv);
	WAIT_MS(100);

	// reciv and send 0:text mode, 1:hex mode.
	sendAT("AT+QICFG=\"dataformat\",1,1\r\n", "OK\r\n", "ERROR", 10000, buff_reciv);
	WAIT_MS(100);

	// configuracion previa al modo de conexion trasnparente
	sendAT("AT&D1\r\n", "OK\r\n", "ERROR", 10000, buff_reciv);
	WAIT_MS(100);

	char res_esperada[] = "+QIOPEN:"; // In Buffer Acces Mode 0,0

	// Open en transparent Mode (2)
	sprintf(buff_send, "AT+QIOPEN=1,%d,\"TCP\",\"%s\",%u,0,2\r\n", connID, ip_tcp, port_tcp);
	ret = sendAT(buff_send, res_esperada, "CONNECT", 150000, buff_reciv);
	WAIT_MS(100);

	// verificar si la espuesta contiene conxixon establecida o no
	if (strstr(buff_reciv, "CONNECT") != NULL)
	{
		return MD_TCP_OPEN_OK;
	}

	/*
	if(ret == MD_AT_OK){
		return MD_TCP_OPEN_OK;
	}
	*/

	if (ret == MD_AT_TIMEOUT)
	{
		return MD_TCP_OPEN_FAIL;
	}

	char *result = strstr(buff_reciv, res_esperada);
	if (result != NULL)
	{
		remove_spaces(result);
		int val;
		if (sscanf(result, "+QIOPEN:%*d,%d", &val) == 1)
		{
			if (val == 0)
			{
				return MD_TCP_OPEN_OK;
				// COMBIAMOS A CONEXION DE MODO TRASPARENTE:
				/*
				ret = sendAT("AT+QISWTMD=2\r\n", "CONNECT", "ERROR", 150000, buff_reciv);
				if (ret == MD_AT_OK)
				{
					return MD_TCP_OPEN_OK;
				}*/
			}
			else
			{
				return MD_TCP_OPEN_FAIL;
			}
		}
	}

	return MD_TCP_OPEN_FAIL;
}

int TCP_send(char *msg, uint8_t len)
{
	// sendAT(m95,"ATE1\r\n","OK\r\n","ERROR\r\n",5000,  m95->buff_reciv);//DES Activa modo ECO.
	int ret = 0;
	WAIT_S(1);
	uart_write_bytes(modem_uart.uart_num, (void *)msg, len);
	ret = uart_wait_tx_done(modem_uart.uart_num, pdMS_TO_TICKS(120000));
	if (ret != ESP_OK)
	{
		return MD_TCP_SEND_FAIL;
	}

	/*
	memset(buff_send, '\0', strlen(buff_send));
	// sprintf(buff_send,"AT+QISEND=0\r\n",len);
	ret = sendAT("AT+QISEND=0\r\n", ">", "ERROR\r\n", 25500, buff_reciv);
	if (ret != MD_AT_OK)
	{
		return MD_TCP_SEND_FAIL;
	}

	// uart_write_bytes(modem_uart.uart_num,(void *)msg,len);
	uart_write_bytes(modem_uart.uart_num, (void *)msg, len);
	ret = sendAT("\x1A", "SEND OK\r\n", "ERROR\r\n", 25500, buff_reciv);

	if (ret != MD_AT_OK)
	{
		return MD_TCP_SEND_FAIL;
	}
	ESP_LOGI("TCP", "SEND OK CORRECTO\r\n");
	memset(buff_reciv, '\0', strlen(buff_reciv));
	*/
	return MD_TCP_SEND_OK;
}

int TCP_close(int connID)
{

	// mensaje para salir de modo trasnparente
	char msg[] = "+++";
	WAIT_S(1);
	uart_write_bytes(modem_uart.uart_num, (void *)msg, strlen(msg));
	uart_wait_tx_done(modem_uart.uart_num, pdMS_TO_TICKS(2000));
	WAIT_S(1);

	ESP_LOGI(TAG, "==>> CLOSE TCP <<==");
	sprintf(buff_send, "AT+QICLOSE=%d\r\n", connID);
	int ret = sendAT(buff_send, "OK\r\n", "ERROR\r\n", 20000, buff_reciv);
	WAIT_MS(100);

	if (ret != MD_AT_OK)
	{
		return MD_TCP_CLOSE_FAIL;
	}
	return MD_TCP_CLOSE_OK;
}


/************************************************
 * FTP FILES
 ************************************************/
int Modem_FTP_Open(const char *ipServer, uint16_t port)
{
	int ret = 0;
	/**
	 * Step 1: Configure and activate the PDP context.
	 */
	ret = sendAT("AT+QICSGP=1\r\n", "OK\r\n", "ERROR", 10000, buff_reciv);
	WAIT_MS(100);
	if (ret == MD_AT_OK)
	{
		// verificamos si el APN se encuantra configurado o no
		if (strstr(buff_reciv, APN) == NULL)
		{
			// APN config
			sprintf(buff_send, "AT+QICSGP=1,1,\"%s,\"\",\"\",1\r\n", APN);
			sendAT(buff_send, "OK\r\n", "ERROR", 10000, buff_reciv);
			WAIT_MS(100);
		}
	}

	// Activate a PDP Context
	sendAT("AT+QIACT=1\r\n", "OK", "ERROR", 100000, buff_reciv);
	WAIT_MS(100);

	sendAT("AT+QIACT?\r\n", "OK", "ERROR", 10000, buff_reciv);
	WAIT_MS(100);

	// sendAT("AT+QFTPCFG=?\r\n", "OK\r\n", "ERROR", 10000, buff_reciv);
	// WAIT_S(3);

	// AT+QFTPCFG="contextid"[,<contextID>]
	sendAT("AT+QFTPCFG=\"contextid\",1\r\n", "OK\r\n", "ERROR", 10000, buff_reciv);
	WAIT_MS(100);

	/**
	 * Step 2: Configure user account and transfer settings.
	 */
	// AT+QFTPCFG="account"[,<username>,<password>]
	sendAT("AT+QFTPCFG=\"account\",\"ec200\",\"1234\"\r\n", "OK\r\n", "ERROR", 10000, buff_reciv);
	WAIT_MS(100);

	// 1: file as ASCII, 0: file as bin
	sendAT("AT+QFTPCFG=\"filetype\",0\r\n", "OK\r\n", "ERROR", 10000, buff_reciv);
	WAIT_MS(100);

	// AT+QFTPCFG="transmode"[,<transmode>]
	//  1: passive mode, 0: activr mode
	sendAT("AT+QFTPCFG=\"transmode\",1\r\n", "OK\r\n", "ERROR", 10000, buff_reciv);
	WAIT_MS(100);

	// set max time 90 seg
	sendAT("AT+QFTPCFG=\"rsptimeout\",90\r\n", "OK\r\n", "ERROR", 10000, buff_reciv);
	WAIT_MS(100);

	// AT+QFTPCFG="cache_size"[,<cache_size>[,<threshold>]]
	strcpy(buff_send, "AT+QFTPCFG=\"cache_size\",65536,1\r\n");
	sendAT(buff_send, "OK\r\n", "ERROR", 10000, buff_reciv);
	WAIT_MS(100);

	sendAT("AT&D1\r\n", "OK\r\n", "ERROR", 10000, buff_reciv);
	WAIT_MS(100);

	/**
	 * //Step 3: Configure FTPS.
	 */
	/*
	// Set SSL type as 1, the module works as FTPS client
	sendAT("AT+QFTPCFG=\"ssltype\",1\r\n", "OK\r\n", "ERROR", 10000, buff_reciv);
	WAIT_MS(100);

	// Set SSL context as 1.
	sendAT("AT+QFTPCFG=\"sslctxid\",1\r\n", "OK\r\n", "ERROR", 10000, buff_reciv);
	// WAIT_MS(100);
	// AT+QSSLCFG="ciphersuite",1,0xffff
	// AT+QSSLCFG="seclevel",1,0
	// //Set SSL security level as 0, which means the SSL CA certificate is not needed.
	sendAT("AT+QSSLCFG=\"seclevel\",1,0\r\n", "OK\r\n", "ERROR", 10000, buff_reciv);
	WAIT_MS(100);
	*/

	/**
	 * //Step 4: Login to FTPS server
	 */

	// Open en transparent Mode (2)
	sprintf(buff_send, "AT+QFTPOPEN=\"%s\",%u\r\n", ipServer, port);
	ret = sendAT(buff_send, "+QFTPOPEN:", "ERROR", 100000, buff_reciv);
	WAIT_MS(100);

	if (ret == MD_AT_OK)
	{
		remove_spaces(buff_reciv);
		int val1, val2;
		if (sscanf(buff_reciv, "%*[^:]:%d,%d", &val1, &val2) == 2)
		{
			if (val1 == 0 && val2 == 0)
			{
				return MD_FTP_OPEN_OK;
			}
		}
	}

	return MD_FTP_OPEN_FAIL;
}

int Modem_FTP_Close()
{
	int ret = 0;
	char msg[] = "+++";
	WAIT_S(1);
	uart_write_bytes(modem_uart.uart_num, (void *)msg, strlen(msg));
	uart_wait_tx_done(modem_uart.uart_num, pdMS_TO_TICKS(2000));
	WAIT_S(2);

	ret = sendAT("AT+QFTPCLOSE\r\n", "+QFTPCLOSE:", "ERROR", 100000, buff_reciv);
	WAIT_MS(100);
	ret = sendAT("AT+QIDEACT=1\r\n", "OK\r\n", "ERROR", 40000, buff_reciv);
	WAIT_MS(100);
	return ret;
}

int Modem_FTP_CheckFile(const char *dir_file, const char *file_name, size_t *file_size)
{
	int ret = 0;
	// Cambio: Configurar la carpeta de trabajo
	sprintf(buff_send, "AT+QFTPCWD=\"%s\"\r\n", dir_file);
	ret = sendAT(buff_send, "+QFTPCWD:", "ERROR", 100000, buff_reciv);
	WAIT_MS(100);

	if (ret == MD_AT_OK)
	{
		remove_spaces(buff_reciv);
		int val1, val2;

		// Escaneo: Verificar si la carpeta está vacía
		if (sscanf(buff_reciv, "%*[^:]:%d,%d", &val1, &val2) == 2)
		{
			if (val1 == 0 && val2 == 0)
			{
				// Cambio: Obtener el tamaño del archivo
				sprintf(buff_send, "AT+QFTPSIZE=\"%s\"\r\n", file_name);
				ret = sendAT(buff_send, "+QFTPSIZE:", "ERROR", 100000, buff_reciv);
				WAIT_MS(100);

				if (ret == MD_AT_OK)
				{
					remove_spaces(buff_reciv);
					if (sscanf(buff_reciv, "%*[^:]:%d,%d", &val1, &val2) == 2)
					{
						if (val1 == 0)
						{
							*file_size = val2;
							return MD_FTP_OPEN_OK;
						}
					}
				}
			}
		}
	}
	return MD_FTP_OPEN_FAIL;
}

int Modem_FTP_DownloadFile(const char *file_name, size_t file_size)
{

	int ret = 0;
	//size_t len_max = file_size / BUF_SIZE_MD;
	size_t inicio = 0; // Puedes ajustar el inicio según tus necesidades
	size_t fin = inicio + BUF_SIZE_MD;

	// for (size_t i = 0; i < len_max; i++) {
	// fin = inicio + BUF_SIZE_MD;

	sprintf(buff_send, "AT+QFTPGET=\"%s\",\"COM:\",%d,%d\r\n", file_name, inicio, fin);
	ret = sendAT(buff_send, "\r\n+QFTPGET:", "ERROR", 100000, buff_reciv);
	ESP_LOGE(TAG, "data read: %s", buff_reciv);
	WAIT_MS(200);
	if (ret == MD_AT_OK)
	{	
		char *result = strstr(buff_reciv, "QFTPGET:");
		if (result != NULL)
		{
			remove_spaces(result);
			int val1, val2;
			if (sscanf(result, "%*[^:]:%d,%d", &val1, &val2) == 1)
			{
				if (val1 == 0 && val2 == fin)
				{
					return MD_TCP_OPEN_OK;
				}
			}
		}
	}

	// inicio = fin;
	//}

	/*
	if (ret == MD_AT_OK&&  (inicio < file_size)){
		sprintf(buff_send, "AT+QFTPGET=\"%s\",\"COM:\",%d,%d\r\n", file_name, inicio, file_size);
		ret = sendAT(buff_send, "CONNECT", "ERROR", 100000, buff_reciv);
		if (ret == MD_AT_OK){
			WAIT_S(5);
		}
	}
	*/
	WAIT_S(2);
	return MD_TCP_OPEN_FAIL;
}


/*
int ret_ftp = Modem_FTP_Open(IP_OTA_01, 21);
ESP_LOGI(TAG, "FTP ret open: %04X", ret_ftp);
WAIT_S(1);
if (ret_ftp == MD_FTP_OPEN_OK)
{
	size_t file_size = 0;
	ret_ftp = Modem_FTP_CheckFile("EG915", "test01.bin", &file_size);
	ESP_LOGI(TAG, "FTP ret check file: %04X, size: %d ", ret_ftp, file_size);
	if (ret_ftp == MD_FTP_OPEN_OK)
	{
		Modem_FTP_DownloadFile("test01.bin", file_size);
	}
}
ret_ftp = Modem_FTP_Close();
ESP_LOGI(TAG, "FTP ret close: %04X", ret_ftp);
vTaskDelete(NULL);

*/