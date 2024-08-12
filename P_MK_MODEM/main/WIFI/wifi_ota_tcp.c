/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include <stdio.h>
#include "sdkconfig.h"
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <errno.h>
#include <netdb.h> // struct addrinfo
#include <arpa/inet.h>
#include "esp_netif.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "main.h"
#include "ota_headers.h"
#include "ota_control.h"
#include "credentials.h"

#include "wifi_confi.h"

/**********************************************************
 * VARIABLES
 *********************************************************/

static const char *TAG = "TCP-WIFI";
static uint8_t rx_buffer[1024] = {0};

/*********************************************************
 * FUNCTIONS
 *********************************************************/
OTA_RESP_ ota_buid_ans(uint8_t ans);

// Función para recibir datos por un búfer
size_t recv_ota_resp(int sock, uint8_t *buffer, size_t bufsize, int timeout)
{
    size_t len = 0;
    memset(buffer, 0, bufsize); // Inicializa el búfer
    uint64_t n = esp_timer_get_time();
    while (esp_timer_get_time() < (n + timeout * 1000))
    {
        ssize_t bytes_received = recv(sock, buffer + len, bufsize - len, 0);
        if (bytes_received <= 0)
        {
            // break; // No se recibieron más datos o se produjo un error
            printf("ret :%d \n", bytes_received);
            bytes_received = 0;
        }
        len += bytes_received;

        // Verifica si se encontró el byte inicial y final
        if (buffer[0] == OTA_SOF && buffer[len - 1] == OTA_EOF)
        {
            break; // Detener la lectura
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    // printf("BUFF: %s\r\n", buffer);
    return len;
}

void check_ota_tcp_wifi(const char *host_ip, uint16_t host_port, const char *msg_loggin)
{

    int addr_family = 0;
    int ip_protocol = 0;

    struct sockaddr_in dest_addr;
    inet_pton(AF_INET, host_ip, &dest_addr.sin_addr);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(host_port);
    addr_family = AF_INET;
    ip_protocol = IPPROTO_IP;

    int sock = socket(addr_family, SOCK_STREAM, ip_protocol);
    if (sock < 0)
    {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        // break;
        goto exit;
    }
    ESP_LOGI(TAG, "Socket created, connecting to %s:%d", host_ip, host_port);
    int err = connect(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err != 0)
    {
        ESP_LOGE(TAG, "Socket unable to connect: errno %d", errno);
        // break;
        goto exit;
    }
    ESP_LOGI(TAG, "Successfully connected");
    vTaskDelay(pdMS_TO_TICKS(1000));

    size_t data_len = 0;
    do
    {
        int err = send(sock, msg_loggin, strlen(msg_loggin), 0);
        if (err < 0)
        {
            ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
            break;
        }
        else
        {
            ESP_LOGI(TAG, "SEND: %s", msg_loggin);
        }

        data_len = recv_ota_resp(sock, rx_buffer, sizeof(rx_buffer) - 1, 5);
        if (data_len <= 0)
        {
            ESP_LOGE(TAG, "Error receiving data");
            break;
        }
        else
        {
            ESP_LOGI("OTA", "Received data: %d", data_len);
            // ESP_LOG_BUFFER_HEXDUMP("OTA", buffer, data_len, ESP_LOG_INFO);
            ESP_LOGI("OTA", "Received data: %.*s", (int)data_len, rx_buffer);

            if (strstr((char *)rx_buffer, "\"ota\": \"true\"") != NULL)
            {
                ESP_LOGI(TAG, "Iniciando OTA");
                printf("Watchdog desactivado\r\n");
                ota_init();
                debug_ota("Init");
                uint16_t ret_ota = OTA_EX_ERR;
                OTA_RESP_ rsp_tcp = {0};
                do
                {
                    data_len = recv_ota_resp(sock, rx_buffer, sizeof(rx_buffer) - 1, 5);
                    if (data_len <= 0)
                    {
                        ret_ota = OTA_EX_ERR;
                    }
                    else
                    {
                        ret_ota = ota_flash(rx_buffer, data_len);
                        //ESP_LOGI(TAG, "Received %d bytes from %s:", data_len, host_ip);
                    }

                    if (ret_ota != OTA_EX_OK)
                    {
                        // debug_ota("ota_m95> ERROR, Sending NACK\r\n");
                        rsp_tcp = ota_buid_ans(OTA_NACK);
                        send(sock, (char *)&rsp_tcp, sizeof(rsp_tcp), 0);
                        break;
                    }
                    else
                    {
                        // debug_ota("ota_m95> OK, Sending ACK\r\n");
                        rsp_tcp = ota_buid_ans(OTA_ACK);
                        send(sock, (char *)&rsp_tcp, sizeof(rsp_tcp), 0);
                    }

                } while (ota_d.ota_state != OTA_STATE_IDLE);

                if (ret_ota == OTA_EX_OK)
                {
                    ESP_LOGW(TAG, "update ESP");
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    esp_restart();
                }

                printf("Watchdog reactivado\r\n");
            }
        }

    } while (false);

exit:
    if (sock != -1)
    {
        ESP_LOGE(TAG, "Shutting down socket and restarting...");
        shutdown(sock, 0);
        close(sock);
    }

    return;
}

/*******************************************************
 * OTA CTRL ANS FUNCTIONS
 *******************************************************/

OTA_RESP_ ota_buid_ans(uint8_t ans)
{
    OTA_RESP_ rsp = {
        .sof = OTA_SOF,
        .packet_type = OTA_PACKET_TYPE_RESPONSE,
        .data_len = 1u,
        .status = ans,
        .crc = 0u, // TODO: Add CRC
        .eof = OTA_EOF};

    return rsp;
}
