idf_component_register(
    SRCS
        "main.cpp"
        "config_idf.c"
        "esp32_can_idf.cpp"
        "uart_serial.cpp"
        "logger_idf.cpp"
        "sys_io_idf.cpp"
        "can_manager.cpp"
        "gvret_comm.cpp"
        "wifi_manager_idf.cpp"
        "serial_console_idf.cpp"
    INCLUDE_DIRS "."
)
