/*
 * esp32_can_idf.cpp - ESP32-S3 TWAI (built-in CAN) driver
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
 *  ESP32CAN class — TWAI implementation
 * ═══════════════════════════════════════════════════════════════════════════ */

ESP32CAN::ESP32CAN() {
    _rxPin     = TWAI_RX_PIN;
    _txPin     = TWAI_TX_PIN;
    _enabled   = false;
    _listenOnly = false;
    _baud      = 500000;
    _started   = false;
}

void ESP32CAN::setCANPins(int rxPin, int txPin) {
    _rxPin = rxPin;
    _txPin = txPin;
}

void ESP32CAN::enable()  { _enabled = true; }

void ESP32CAN::disable() {
    if (_started) {
        twai_stop();
        twai_driver_uninstall();
        _started = false;
    }
    _enabled = false;
}

void ESP32CAN::setListenOnlyMode(bool mode) { _listenOnly = mode; }
void ESP32CAN::watchFor() { /* accept-all filter is set in begin() */ }

bool ESP32CAN::begin(uint32_t baud, int rxPin, int txPin) {
    /* If already running, stop first (for reconfiguration via GVRET) */
    if (_started) {
        twai_stop();
        twai_driver_uninstall();
        _started = false;
    }

    if (rxPin != 255) _rxPin = rxPin;
    if (txPin != 255) _txPin = txPin;
    _baud = baud;

    /* General configuration */
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(
        (gpio_num_t)_txPin,
        (gpio_num_t)_rxPin,
        _listenOnly ? TWAI_MODE_LISTEN_ONLY : TWAI_MODE_NORMAL
    );
    g_config.rx_queue_len = 32;   /* 32 frames RX buffer (vs MCP2515's 2!) */
    g_config.tx_queue_len = 5;

    /* Timing configuration (from APB 80 MHz, no external crystal needed) */
    twai_timing_config_t t_config = get_timing_config(_baud);

    /* Accept ALL frames — no hardware filtering */
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    /* Install TWAI driver */
    esp_err_t r = twai_driver_install(&g_config, &t_config, &f_config);
    if (r != ESP_OK) {
        ESP_LOGE(TAG, "twai_driver_install: %s", esp_err_to_name(r));
        return false;
    }

    /* Start TWAI */
    r = twai_start();
    if (r != ESP_OK) {
        ESP_LOGE(TAG, "twai_start: %s", esp_err_to_name(r));
        twai_driver_uninstall();
        return false;
    }

    _started = true;
    ESP_LOGI(TAG, "TWAI OK: %lu bps, mode=%s, TX=GPIO%d, RX=GPIO%d, RX queue=32",
             (unsigned long)_baud,
             _listenOnly ? "listen-only" : "normal",
             _txPin, _rxPin);
    return true;
}

int ESP32CAN::available() {
    if (!_started || !_enabled) return 0;

    twai_status_info_t status;
    if (twai_get_status_info(&status) != ESP_OK) return 0;
    return (int)status.msgs_to_rx;
}

void ESP32CAN::read(CAN_FRAME &frame) {
    if (!_started || !_enabled) return;

    twai_message_t msg;
    if (twai_receive(&msg, 0) != ESP_OK) return;   /* non-blocking (0 ticks) */

    /* Convert twai_message_t -> CAN_FRAME */
    frame.id        = msg.identifier;
    frame.extended  = msg.extd ? true : false;
    frame.rtr       = msg.rtr ? 1 : 0;
    frame.length    = msg.data_length_code;
    if (frame.length > 8) frame.length = 8;
    memcpy(frame.data.bytes, msg.data, frame.length);
    frame.timestamp = (uint32_t)(esp_timer_get_time() / 1000);
}

void ESP32CAN::sendFrame(CAN_FRAME &frame) {
    if (!_started || _listenOnly) return;

    twai_message_t msg = {};
    msg.identifier       = frame.id;
    msg.extd             = frame.extended ? 1 : 0;
    msg.rtr              = frame.rtr ? 1 : 0;
    msg.data_length_code = frame.length;
    if (msg.data_length_code > 8) msg.data_length_code = 8;
    memcpy(msg.data, frame.data.bytes, msg.data_length_code);

    esp_err_t r = twai_transmit(&msg, pdMS_TO_TICKS(10));
    if (r != ESP_OK) {
        ESP_LOGW(TAG, "twai_transmit: %s", esp_err_to_name(r));
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Global instances
 * ═══════════════════════════════════════════════════════════════════════════ */

ESP32CAN CAN0;
ESP32CAN_Stub CAN1;
