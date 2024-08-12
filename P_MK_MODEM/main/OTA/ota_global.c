#include "ota_global.h"

#define OTA_TAG "OTA"

// static
void register_ota_log_level(void)
{
    if (ESP_LOG_BLUE > ESP_LOG_VERBOSE)
    {
        esp_log_level_set("*", ESP_LOG_BLUE);
    }
}

void debug_ota(const char *format, ...)
{

#if DEBUG_OTA_ON
    va_list args;
    va_start(args, format);

    char formatted_message[256];
    vsnprintf(formatted_message, sizeof(formatted_message), format, args);
    esp_log_write(ESP_LOG_BLUE, OTA_TAG, "\033[0;34m* %s: %s\033[0m\n", OTA_TAG, formatted_message);
    va_end(args);
#endif
    return;
}

/*
void my_log_blue(const char* tag, const char* format, ...) {
    va_list args;
    va_start(args, format);

    char formatted_message[256];
    vsnprintf(formatted_message, sizeof(formatted_message), format, args);
    esp_log_write(ESP_LOG_BLUE, tag, "\033[0;34m* %s: %s\033[0m\n",  tag, formatted_message);
    va_end(args);
}
*/