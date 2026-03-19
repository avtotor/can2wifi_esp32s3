/*
 * ESP32S3_CAN2WIFI - ESP-IDF main
 * CAN -> WiFi (GVRET/SavvyCAN). No SD/FLASH logging.
 */
#include "config_idf.h"
#include "esp32_can_idf.h"
#include "uart_serial.h"
#include "logger_idf.h"
#include "sys_io_idf.h"
#include "can_manager.h"
#include "gvret_comm.h"
#include "wifi_manager_idf.h"
#include "serial_console_idf.h"
#include "esp_mac.h"
#include "esp_chip_info.h"
#ifndef CONFIG_IDF_TARGET
#define CONFIG_IDF_TARGET "esp32s3"
#endif
#include "esp_timer.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include <stdio.h>
#include <string.h>

GVRET_Handler_t serialGVRET;
GVRET_Handler_t wifiGVRET;

static void load_settings(void) {
    settings.CAN0Speed          = 500000;
    settings.CAN0_Enabled       = true;
    settings.CAN0ListenOnly     = false;
    settings.useBinarySerialComm = false;
    settings.logLevel           = 1;
    settings.wifiMode           = 2;
    settings.systemType         = 0;
    settings.CAN1Speed          = 500000;
    strncpy(settings.SSID,    SSID_NAME, sizeof(settings.SSID) - 1);
    settings.SSID[sizeof(settings.SSID) - 1] = '\0';
    strncpy(settings.WPA2Key, WPA2KEY,   sizeof(settings.WPA2Key) - 1);
    settings.WPA2Key[sizeof(settings.WPA2Key) - 1] = '\0';

    Logger_setLevel(settings.logLevel);

    SysSettings.LED_CANTX   = 255;
    SysSettings.LED_CANRX   = 255;
    SysSettings.LED_SD      = ESP32_BUILTIN_LED;
    SysSettings.LED_LOGGING = 255;
    SysSettings.logToggle   = false;
    SysSettings.sdToggle    = false;
    SysSettings.txToggle    = false;
    SysSettings.rxToggle    = true;
    SysSettings.numBuses    = 1;
    SysSettings.isWifiActive    = false;
    SysSettings.isWifiConnected = false;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        SysSettings.clientNodes[i].socket_fd = -1;
        SysSettings.clientNodes[i].connected = false;
    }
}

static void app_init_gpio(void) {
    gpio_config_t io = {0};
    io.pin_bit_mask  = (1ULL << ESP32_BUILTIN_LED);
    io.mode          = GPIO_MODE_OUTPUT;
    io.pull_up_en    = GPIO_PULLUP_DISABLE;
    io.pull_down_en  = GPIO_PULLDOWN_DISABLE;
    io.intr_type     = GPIO_INTR_DISABLE;
    gpio_config(&io);
}

void app_main(void) {
    uart_serial_init();
    app_init_gpio();

    esp_chip_info_t info;
    esp_chip_info(&info);
    uart_serial_printf("\n\nESP32 model: %s Rev %d\n", CONFIG_IDF_TARGET, info.revision);
    uart_serial_printf("Cores: %d\n", info.cores);

    uint8_t mac[6];
    if (esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP) == ESP_OK) {
        uart_serial_printf("WiFi Soft-AP MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }

    uart_serial_println("\n---------------------------------------");
    uart_serial_printf("CAN -> WiFi (GVRET/SavvyCAN)\nFirmware: %s\n\n", CFG_VERSION);

    esp_err_t nvs_ret = nvs_flash_init();
    if (nvs_ret == ESP_ERR_NVS_NO_FREE_PAGES || nvs_ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    gvret_init(&serialGVRET);
    gvret_init(&wifiGVRET);

    load_settings();
    can_manager_setup();
    SysSettings.isWifiConnected = false;

    vTaskDelay(pdMS_TO_TICKS(250));
    wifi_manager_setup();

    uint32_t last_flush = (uint32_t)(esp_timer_get_time() / 1000);
    for (;;) {
        can_manager_loop();
        wifi_manager_loop();

        size_t   wifi_len = gvret_num_available_bytes(&wifiGVRET);
        uint32_t now      = (uint32_t)(esp_timer_get_time() / 1000);

        if ((now - last_flush > SER_BUFF_FLUSH_INTERVAL / 1000) || (wifi_len > (WIFI_BUFF_SIZE - 40))) {
            last_flush = now;
            if (wifi_len > 0)
                wifi_manager_send_buffered_data();
        }

        int serial_cnt = 0;
        while (uart_serial_available() > 0 && serial_cnt < 128) {
            serial_cnt++;
            uint8_t b = uart_serial_read();
            gvret_process_byte(&serialGVRET, b);
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
