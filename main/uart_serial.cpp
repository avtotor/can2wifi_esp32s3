#include "uart_serial.h"
#include "driver/uart.h"
#include "esp_log.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#define UART_NUM UART_NUM_0
#define BUF_SIZE 256

void uart_serial_init(void) {
    uart_config_t uart_config = {};
    uart_config.baud_rate = 115200;
    uart_config.data_bits = UART_DATA_8_BITS;
    uart_config.parity = UART_PARITY_DISABLE;
    uart_config.stop_bits = UART_STOP_BITS_1;
    uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uart_config.rx_flow_ctrl_thresh = 0;
    uart_config.source_clk = UART_SCLK_DEFAULT;
    uart_driver_install(UART_NUM, BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(UART_NUM, &uart_config);
    uart_set_pin(UART_NUM, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

int uart_serial_available(void) {
    size_t len = 0;
    uart_get_buffered_data_len(UART_NUM, &len);
    return (int)len;
}

uint8_t uart_serial_read(void) {
    uint8_t b;
    if (uart_read_bytes(UART_NUM, &b, 1, 20 / portTICK_PERIOD_MS) > 0)
        return b;
    return 0;
}

size_t uart_serial_write(const uint8_t *data, size_t len) {
    return uart_write_bytes(UART_NUM, data, len);
}

void uart_serial_printf(const char *fmt, ...) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (n > 0)
        uart_write_bytes(UART_NUM, buf, (size_t)n);
}

void uart_serial_print(const char *s) {
    if (s)
        uart_write_bytes(UART_NUM, (const uint8_t *)s, strlen(s));
}

void uart_serial_println(const char *s) {
    if (s)
        uart_write_bytes(UART_NUM, (const uint8_t *)s, strlen(s));
    uart_write_bytes(UART_NUM, (const uint8_t *)"\n", 1);
}

void uart_serial_print_int(int n) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", n);
    uart_serial_print(buf);
}

void uart_serial_println_int(int n) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%d\n", n);
    uart_serial_print(buf);
}
