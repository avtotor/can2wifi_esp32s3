#ifndef GVRET_COMM_IDF_H_
#define GVRET_COMM_IDF_H_

#include "config_idf.h"
#include "esp32_can_idf.h"
#include <stdint.h>
#include <stddef.h>

typedef enum {
    IDLE, GET_COMMAND, BUILD_CAN_FRAME, TIME_SYNC, GET_DIG_INPUTS, GET_ANALOG_INPUTS,
    SET_DIG_OUTPUTS, SETUP_CANBUS, GET_CANBUS_PARAMS, GET_DEVICE_INFO, SET_SINGLEWIRE_MODE,
    SET_SYSTYPE, ECHO_CAN_FRAME, SETUP_EXT_BUSES
} GVRETState;

typedef enum {
    PROTO_BUILD_CAN_FRAME = 0, PROTO_TIME_SYNC = 1, PROTO_DIG_INPUTS = 2, PROTO_ANA_INPUTS = 3,
    PROTO_SET_DIG_OUT = 4, PROTO_SETUP_CANBUS = 5, PROTO_GET_CANBUS_PARAMS = 6, PROTO_GET_DEV_INFO = 7,
    PROTO_SET_SW_MODE = 8, PROTO_KEEPALIVE = 9, PROTO_SET_SYSTYPE = 10, PROTO_ECHO_CAN_FRAME = 11,
    PROTO_GET_NUMBUSES = 12, PROTO_GET_EXT_BUSES = 13, PROTO_SET_EXT_BUSES = 14
} GVRETProtocol;

typedef struct {
    CAN_FRAME    build_out_frame;
    int          out_bus;
    uint8_t      buff[20];
    int          step;
    GVRETState   state;
    uint32_t     build_int;
    uint8_t      transmitBuffer[WIFI_BUFF_SIZE];
    int          transmitBufferLength;
} GVRET_Handler_t;

void   gvret_init(GVRET_Handler_t *h);
void   gvret_process_byte(GVRET_Handler_t *h, uint8_t in_byte);
void   gvret_send_frame_to_buffer(GVRET_Handler_t *h, CAN_FRAME *frame, int whichBus);
size_t gvret_num_available_bytes(GVRET_Handler_t *h);
uint8_t *gvret_get_buffered_bytes(GVRET_Handler_t *h);
void   gvret_clear_buffered_bytes(GVRET_Handler_t *h);

#endif
