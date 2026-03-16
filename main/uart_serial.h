#ifndef UART_SERIAL_H_
#define UART_SERIAL_H_

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void uart_serial_init(void);
int uart_serial_available(void);
uint8_t uart_serial_read(void);
size_t uart_serial_write(const uint8_t *data, size_t len);
void uart_serial_printf(const char *fmt, ...);
void uart_serial_print(const char *s);
void uart_serial_println(const char *s);
void uart_serial_print_int(int n);
void uart_serial_println_int(int n);

#ifdef __cplusplus
}
#endif

#endif
