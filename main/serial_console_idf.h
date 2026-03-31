#ifndef SERIAL_CONSOLE_IDF_H_
#define SERIAL_CONSOLE_IDF_H_

#include <stdint.h>

void serial_console_rcv_character(uint8_t chr);
void serial_console_print_menu(void);

#endif
