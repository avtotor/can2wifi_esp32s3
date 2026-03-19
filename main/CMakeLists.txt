idf_component_register(
    SRCS
        "main.c"
        "config_idf.c"
        "esp32_can_idf.c"
        "uart_serial.c"
        "logger_idf.c"
        "sys_io_idf.c"
        "can_manager.c"
        "gvret_comm.c"
        "wifi_manager_idf.c"
        "serial_console_idf.c"
    INCLUDE_DIRS "."
)

# Uncomment to enable TWAI debug output (status every 5s in idf.py monitor):
target_compile_definitions(${COMPONENT_LIB} PRIVATE CAN_DEBUG=1)
