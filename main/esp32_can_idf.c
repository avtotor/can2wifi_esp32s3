/*
 * esp32_can_idf.c - ESP32-S3 TWAI (built-in CAN) driver
 * Uses ESP-IDF TWAI peripheral + external TJA1050 transceiver.
 * No MCP2515, no SPI. Only 2 GPIO wires: TX and RX.
 *
 * TJA1050 wiring:
 *   ESP32 GPIO4 (TX) -> TJA1050 TX pin  (direct, 3.3V OK)
 *   TJA1050 RX pin   -> 1k/2k divider -> ESP32 GPIO5 (RX)
 *   TJA1050 VCC = 5V, GND = common ground
 *   TJA1050 CANH/CANL -> car OBD-II pins 6/14
 */
#include "esp32_can_idf.h"
#include "driver/twai.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "twai_can";

static int      s_rxPin     = TWAI_RX_PIN;
static int      s_txPin     = TWAI_TX_PIN;
static bool     s_enabled   = false;
static bool     s_listenOnly = false;
static uint32_t s_baud      = 500000;
static bool     s_started   = false;

/* ═══════════════════════════════════════════════════════════════════════════
 *  TWAI timing configurations (based on APB 80 MHz clock)
 * ═══════════════════════════════════════════════════════════════════════════ */

static twai_timing_config_t get_timing_config(uint32_t baud) {
    switch (baud) {
        case 50000:   { twai_timing_config_t t = TWAI_TIMING_CONFIG_50KBITS();   return t; }
        case 100000:  { twai_timing_config_t t = TWAI_TIMING_CONFIG_100KBITS();  return t; }
        case 125000:  { twai_timing_config_t t = TWAI_TIMING_CONFIG_125KBITS();  return t; }
        case 250000:  { twai_timing_config_t t = TWAI_TIMING_CONFIG_250KBITS();  return t; }
        case 500000:  { twai_timing_config_t t = TWAI_TIMING_CONFIG_500KBITS();  return t; }
        case 1000000: { twai_timing_config_t t = TWAI_TIMING_CONFIG_1MBITS();    return t; }
        default:
            ESP_LOGW(TAG, "unknown baud %lu, defaulting to 500K", (unsigned long)baud);
            { twai_timing_config_t t = TWAI_TIMING_CONFIG_500KBITS(); return t; }
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Public API
 * ═══════════════════════════════════════════════════════════════════════════ */

void can0_set_pins(int rxPin, int txPin) {
    s_rxPin = rxPin;
    s_txPin = txPin;
}

void can0_enable(void)  { s_enabled = true; }

void can0_disable(void) {
    if (s_started) {
        twai_stop();
        twai_driver_uninstall();
        s_started = false;
    }
    s_enabled = false;
}

void can0_set_listen_only(bool mode) { s_listenOnly = mode; }

bool can0_begin(uint32_t baud) {
    if (s_started) {
        twai_stop();
        twai_driver_uninstall();
        s_started = false;
    }

    s_baud = baud;

    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(
        (gpio_num_t)s_txPin,
        (gpio_num_t)s_rxPin,
        s_listenOnly ? TWAI_MODE_LISTEN_ONLY : TWAI_MODE_NORMAL
    );
    g_config.rx_queue_len = 32;
    g_config.tx_queue_len = 5;

    twai_timing_config_t t_config = get_timing_config(s_baud);
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    esp_err_t r = twai_driver_install(&g_config, &t_config, &f_config);
    if (r != ESP_OK) {
        ESP_LOGE(TAG, "twai_driver_install: %s", esp_err_to_name(r));
        return false;
    }

    r = twai_start();
    if (r != ESP_OK) {
        ESP_LOGE(TAG, "twai_start: %s", esp_err_to_name(r));
        twai_driver_uninstall();
        return false;
    }

    s_started = true;
    ESP_LOGI(TAG, "TWAI OK: %lu bps, mode=%s, TX=GPIO%d, RX=GPIO%d, RX queue=32",
             (unsigned long)s_baud,
             s_listenOnly ? "listen-only" : "normal",
             s_txPin, s_rxPin);
    return true;
}

int can0_available(void) {
    if (!s_started || !s_enabled) return 0;
    twai_status_info_t status;
    if (twai_get_status_info(&status) != ESP_OK) return 0;
    return (int)status.msgs_to_rx;
}

void can0_read(CAN_FRAME *frame) {
    if (!s_started || !s_enabled) return;
    twai_message_t msg;
    if (twai_receive(&msg, 0) != ESP_OK) return;

    frame->id        = msg.identifier;
    frame->extended  = msg.extd ? true : false;
    frame->rtr       = msg.rtr ? 1 : 0;
    frame->length    = msg.data_length_code;
    if (frame->length > 8) frame->length = 8;
    memcpy(frame->data.bytes, msg.data, frame->length);
    frame->timestamp = (uint32_t)(esp_timer_get_time() / 1000);
}

void can0_send_frame(CAN_FRAME *frame) {
    if (!s_started || s_listenOnly) return;

    twai_message_t msg = {0};
    msg.identifier       = frame->id;
    msg.extd             = frame->extended ? 1 : 0;
    msg.rtr              = frame->rtr ? 1 : 0;
    msg.data_length_code = frame->length;
    if (msg.data_length_code > 8) msg.data_length_code = 8;
    memcpy(msg.data, frame->data.bytes, msg.data_length_code);

    esp_err_t r = twai_transmit(&msg, pdMS_TO_TICKS(10));
    if (r != ESP_OK) {
        ESP_LOGW(TAG, "twai_transmit: %s", esp_err_to_name(r));
    }
}
