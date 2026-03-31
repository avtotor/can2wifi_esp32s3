#include "serial_console_idf.h"
#include "config_idf.h"
#include "uart_serial.h"
#include "logger_idf.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static char cmd_buffer[80];
static int  ptr_buffer = 0;

static void handle_console_cmd(void) {
    if (ptr_buffer == 0) return;
    cmd_buffer[ptr_buffer] = '\0';
    if (cmd_buffer[0] == 'h' || cmd_buffer[0] == 'H' || cmd_buffer[0] == '?') {
        serial_console_print_menu();
    } else if (cmd_buffer[0] == 'R') {
        Logger_console("Reset to factory defaults (not implemented)\n");
    }
    ptr_buffer = 0;
}

void serial_console_rcv_character(uint8_t chr) {
    if (chr == 10 || chr == 13) {
        handle_console_cmd();
        ptr_buffer = 0;
    } else {
        cmd_buffer[ptr_buffer++] = (char)chr;
        if (ptr_buffer >= 79) ptr_buffer = 79;
    }
}

void serial_console_print_menu(void) {
    uart_serial_printf("Build number: %d\n", CFG_BUILD_NUM);
    uart_serial_println("System Menu:");
    uart_serial_println("Short Commands: h = help, R = reset defaults");
    Logger_console("LOGLEVEL=%d\n", settings.logLevel);
    Logger_console("CAN0EN=%d CAN0SPEED=%lu CAN0LISTENONLY=%d\n",
        settings.CAN0_Enabled, (unsigned long)settings.CAN0Speed, settings.CAN0ListenOnly);
    Logger_console("WIFIMODE=%d SSID=%s\n", settings.wifiMode, settings.SSID);
}
