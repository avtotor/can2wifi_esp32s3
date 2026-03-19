/*
 * gvret_comm.c - GVRET protocol handler (ESP-IDF, pure C)
 */
#include "gvret_comm.h"
#include "can_manager.h"
#include "config_idf.h"
#include "logger_idf.h"
#include "serial_console_idf.h"
#include "esp_timer.h"
#include <stdio.h>
#include <string.h>

/* esp_timer_get_time() returns microseconds */
#define micros()  ((uint32_t)(esp_timer_get_time()))

static uint8_t checksum_calc(uint8_t *buffer, int length) {
    uint8_t val = 0;
    for (int c = 0; c < length; c++) val ^= buffer[c];
    return val;
}

void gvret_init(GVRET_Handler_t *h) {
    memset(h, 0, sizeof(*h));
    h->state = IDLE;
}

void gvret_process_byte(GVRET_Handler_t *h, uint8_t in_byte) {
    uint32_t busSpeed = 0;
    uint32_t now = micros();
    uint8_t  temp8;

    switch (h->state) {
    case IDLE:
        if (in_byte == 0xF1) {
            h->state = GET_COMMAND;
        } else if (in_byte == 0xE7) {
            settings.useBinarySerialComm = true;
            SysSettings.lawicelMode = false;
        } else {
            serial_console_rcv_character(in_byte);
        }
        break;

    case GET_COMMAND:
        switch (in_byte) {
        case PROTO_BUILD_CAN_FRAME:
            h->state = BUILD_CAN_FRAME;
            h->buff[0] = 0xF1;
            h->step = 0;
            break;
        case PROTO_TIME_SYNC:
            h->state = TIME_SYNC;
            h->step = 0;
            h->transmitBuffer[h->transmitBufferLength++] = 0xF1;
            h->transmitBuffer[h->transmitBufferLength++] = 1;
            h->transmitBuffer[h->transmitBufferLength++] = (uint8_t)(now & 0xFF);
            h->transmitBuffer[h->transmitBufferLength++] = (uint8_t)(now >> 8);
            h->transmitBuffer[h->transmitBufferLength++] = (uint8_t)(now >> 16);
            h->transmitBuffer[h->transmitBufferLength++] = (uint8_t)(now >> 24);
            break;
        case PROTO_DIG_INPUTS:
            temp8 = 0;
            h->transmitBuffer[h->transmitBufferLength++] = 0xF1;
            h->transmitBuffer[h->transmitBufferLength++] = 2;
            h->transmitBuffer[h->transmitBufferLength++] = temp8;
            h->transmitBuffer[h->transmitBufferLength++] = checksum_calc(h->buff, 2);
            h->state = IDLE;
            break;
        case PROTO_ANA_INPUTS:
            h->transmitBuffer[h->transmitBufferLength++] = 0xF1;
            h->transmitBuffer[h->transmitBufferLength++] = 3;
            for (int i = 0; i < 7; i++) {
                h->transmitBuffer[h->transmitBufferLength++] = 0;
                h->transmitBuffer[h->transmitBufferLength++] = 0;
            }
            h->transmitBuffer[h->transmitBufferLength++] = checksum_calc(h->buff, 9);
            h->state = IDLE;
            break;
        case PROTO_SET_DIG_OUT:
            h->state = SET_DIG_OUTPUTS;
            h->buff[0] = 0xF1;
            break;
        case PROTO_SETUP_CANBUS:
            h->state = SETUP_CANBUS;
            h->step = 0;
            h->buff[0] = 0xF1;
            break;
        case PROTO_GET_CANBUS_PARAMS:
            h->transmitBuffer[h->transmitBufferLength++] = 0xF1;
            h->transmitBuffer[h->transmitBufferLength++] = 6;
            h->transmitBuffer[h->transmitBufferLength++] = settings.CAN0_Enabled + ((unsigned char)settings.CAN0ListenOnly << 4);
            h->transmitBuffer[h->transmitBufferLength++] = settings.CAN0Speed & 0xFF;
            h->transmitBuffer[h->transmitBufferLength++] = (settings.CAN0Speed >> 8) & 0xFF;
            h->transmitBuffer[h->transmitBufferLength++] = (settings.CAN0Speed >> 16) & 0xFF;
            h->transmitBuffer[h->transmitBufferLength++] = (settings.CAN0Speed >> 24) & 0xFF;
            h->transmitBuffer[h->transmitBufferLength++] = 0;
            h->transmitBuffer[h->transmitBufferLength++] = settings.CAN1Speed & 0xFF;
            h->transmitBuffer[h->transmitBufferLength++] = (settings.CAN1Speed >> 8) & 0xFF;
            h->transmitBuffer[h->transmitBufferLength++] = (settings.CAN1Speed >> 16) & 0xFF;
            h->transmitBuffer[h->transmitBufferLength++] = (settings.CAN1Speed >> 24) & 0xFF;
            h->state = IDLE;
            break;
        case PROTO_GET_DEV_INFO:
            h->transmitBuffer[h->transmitBufferLength++] = 0xF1;
            h->transmitBuffer[h->transmitBufferLength++] = 7;
            h->transmitBuffer[h->transmitBufferLength++] = CFG_BUILD_NUM & 0xFF;
            h->transmitBuffer[h->transmitBufferLength++] = (CFG_BUILD_NUM >> 8) & 0xFF;
            h->transmitBuffer[h->transmitBufferLength++] = 0x20;
            h->transmitBuffer[h->transmitBufferLength++] = 0;
            h->transmitBuffer[h->transmitBufferLength++] = 0;
            h->transmitBuffer[h->transmitBufferLength++] = 0;
            h->state = IDLE;
            break;
        case PROTO_SET_SW_MODE:
            h->buff[0] = 0xF1;
            h->state = SET_SINGLEWIRE_MODE;
            h->step = 0;
            break;
        case PROTO_KEEPALIVE:
            h->transmitBuffer[h->transmitBufferLength++] = 0xF1;
            h->transmitBuffer[h->transmitBufferLength++] = 0x09;
            h->transmitBuffer[h->transmitBufferLength++] = 0xDE;
            h->transmitBuffer[h->transmitBufferLength++] = 0xAD;
            h->state = IDLE;
            break;
        case PROTO_SET_SYSTYPE:
            h->buff[0] = 0xF1;
            h->state = SET_SYSTYPE;
            h->step = 0;
            break;
        case PROTO_ECHO_CAN_FRAME:
            h->state = ECHO_CAN_FRAME;
            h->buff[0] = 0xF1;
            h->step = 0;
            break;
        case PROTO_GET_NUMBUSES:
            h->transmitBuffer[h->transmitBufferLength++] = 0xF1;
            h->transmitBuffer[h->transmitBufferLength++] = 12;
            h->transmitBuffer[h->transmitBufferLength++] = SysSettings.numBuses;
            h->state = IDLE;
            break;
        case PROTO_GET_EXT_BUSES:
            h->transmitBuffer[h->transmitBufferLength++] = 0xF1;
            h->transmitBuffer[h->transmitBufferLength++] = 13;
            for (int u = 2; u < 17; u++) h->transmitBuffer[h->transmitBufferLength++] = 0;
            h->step = 0;
            h->state = IDLE;
            break;
        case PROTO_SET_EXT_BUSES:
            h->state = SETUP_EXT_BUSES;
            h->step = 0;
            h->buff[0] = 0xF1;
            break;
        }
        break;

    case BUILD_CAN_FRAME:
        h->buff[1 + h->step] = in_byte;
        switch (h->step) {
        case 0: h->build_out_frame.id = in_byte; break;
        case 1: h->build_out_frame.id |= in_byte << 8; break;
        case 2: h->build_out_frame.id |= in_byte << 16; break;
        case 3:
            h->build_out_frame.id |= in_byte << 24;
            if (h->build_out_frame.id & (1u << 31)) {
                h->build_out_frame.id &= 0x7FFFFFFF;
                h->build_out_frame.extended = true;
            } else {
                h->build_out_frame.extended = false;
            }
            break;
        case 4: h->out_bus = in_byte & 3; break;
        case 5:
            h->build_out_frame.length = in_byte & 0xF;
            if (h->build_out_frame.length > 8) h->build_out_frame.length = 8;
            break;
        default:
            if (h->step < (int)(6 + h->build_out_frame.length)) {
                h->build_out_frame.data.uint8[h->step - 6] = in_byte;
            } else {
                h->state = IDLE;
                h->build_out_frame.rtr = 0;
                can_manager_send_frame(h->out_bus, &h->build_out_frame);
            }
            break;
        }
        h->step++;
        break;

    case TIME_SYNC:
        h->state = IDLE;
        break;

    case SET_DIG_OUTPUTS:
        h->state = IDLE;
        break;

    case GET_DIG_INPUTS:
    case GET_ANALOG_INPUTS:
        h->state = IDLE;
        break;

    case SETUP_CANBUS:
        switch (h->step) {
        case 0: h->build_int = in_byte; break;
        case 1: h->build_int |= in_byte << 8; break;
        case 2: h->build_int |= in_byte << 16; break;
        case 3:
            h->build_int |= in_byte << 24;
            busSpeed = h->build_int & 0xFFFFF;
            if (busSpeed > 1000000) busSpeed = 1000000;
            if (h->build_int > 0) {
                if (h->build_int & 0x80000000ul) {
                    settings.CAN0_Enabled   = (h->build_int & 0x40000000ul) != 0;
                    settings.CAN0ListenOnly = (h->build_int & 0x20000000ul) != 0;
                } else {
                    settings.CAN0_Enabled = true;
                }
                settings.CAN0Speed = busSpeed;
            } else {
                settings.CAN0_Enabled = false;
            }
            if (settings.CAN0_Enabled) {
                can0_set_listen_only(settings.CAN0ListenOnly);
                can0_begin(settings.CAN0Speed);
            } else {
                can0_disable();
            }
            break;
        case 4: h->build_int = in_byte; break;
        case 5: h->build_int |= in_byte << 8; break;
        case 6: h->build_int |= in_byte << 16; break;
        case 7:
            h->build_int |= in_byte << 24;
            busSpeed = h->build_int & 0xFFFFF;
            if (busSpeed > 1000000) busSpeed = 1000000;
            if (h->build_int > 0 && SysSettings.numBuses > 1) {
                if (h->build_int & 0x80000000ul) {
                    settings.CAN1_Enabled   = (h->build_int & 0x40000000ul) != 0;
                    settings.CAN1ListenOnly = (h->build_int & 0x20000000ul) != 0;
                } else {
                    settings.CAN1_Enabled = true;
                }
                settings.CAN1Speed = busSpeed;
            } else {
                settings.CAN1_Enabled = false;
            }
            if (settings.CAN1_Enabled) {
                can1_begin(settings.CAN1Speed, settings.CAN1ListenOnly);
            } else {
                can1_disable();
            }
            h->state = IDLE;
            break;
        }
        h->step++;
        break;

    case GET_CANBUS_PARAMS:
    case GET_DEVICE_INFO:
        break;

    case SET_SINGLEWIRE_MODE:
        h->state = IDLE;
        break;

    case SET_SYSTYPE:
        settings.systemType = in_byte;
        h->state = IDLE;
        break;

    case ECHO_CAN_FRAME:
        h->buff[1 + h->step] = in_byte;
        switch (h->step) {
        case 0: h->build_out_frame.id = in_byte; break;
        case 1: h->build_out_frame.id |= in_byte << 8; break;
        case 2: h->build_out_frame.id |= in_byte << 16; break;
        case 3:
            h->build_out_frame.id |= in_byte << 24;
            if (h->build_out_frame.id & (1u << 31)) {
                h->build_out_frame.id &= 0x7FFFFFFF;
                h->build_out_frame.extended = true;
            } else {
                h->build_out_frame.extended = false;
            }
            break;
        case 4: h->out_bus = in_byte & 1; break;
        case 5:
            h->build_out_frame.length = in_byte & 0xF;
            if (h->build_out_frame.length > 8) h->build_out_frame.length = 8;
            break;
        default:
            if (h->step < (int)(6 + h->build_out_frame.length)) {
                h->build_out_frame.data.bytes[h->step - 6] = in_byte;
            } else {
                h->state = IDLE;
                can_manager_display_frame(&h->build_out_frame, 0);
            }
            break;
        }
        h->step++;
        break;

    case SETUP_EXT_BUSES:
        switch (h->step) {
        case 0: case 1: case 2: case 3: case 4:
        case 5: case 6: case 7: case 8: case 9: case 10:
            h->build_int = in_byte;
            break;
        case 11:
            h->build_int |= in_byte << 24;
            h->state = IDLE;
            break;
        }
        h->step++;
        break;
    }
}

void gvret_send_frame_to_buffer(GVRET_Handler_t *h, CAN_FRAME *frame, int whichBus) {
    size_t writtenBytes;
    if (settings.useBinarySerialComm) {
        if (frame->extended) frame->id |= 1u << 31;
        uint32_t now = micros();
        h->transmitBuffer[h->transmitBufferLength++] = 0xF1;
        h->transmitBuffer[h->transmitBufferLength++] = 0;
        h->transmitBuffer[h->transmitBufferLength++] = (uint8_t)(now & 0xFF);
        h->transmitBuffer[h->transmitBufferLength++] = (uint8_t)(now >> 8);
        h->transmitBuffer[h->transmitBufferLength++] = (uint8_t)(now >> 16);
        h->transmitBuffer[h->transmitBufferLength++] = (uint8_t)(now >> 24);
        h->transmitBuffer[h->transmitBufferLength++] = (uint8_t)(frame->id & 0xFF);
        h->transmitBuffer[h->transmitBufferLength++] = (uint8_t)(frame->id >> 8);
        h->transmitBuffer[h->transmitBufferLength++] = (uint8_t)(frame->id >> 16);
        h->transmitBuffer[h->transmitBufferLength++] = (uint8_t)(frame->id >> 24);
        h->transmitBuffer[h->transmitBufferLength++] = frame->length + (uint8_t)(whichBus << 4);
        for (int c = 0; c < frame->length; c++)
            h->transmitBuffer[h->transmitBufferLength++] = frame->data.uint8[c];
        h->transmitBuffer[h->transmitBufferLength++] = 0;
    } else {
        writtenBytes = (size_t)sprintf((char *)&h->transmitBuffer[h->transmitBufferLength],
                                       "%lu,%lX,",
                                       (unsigned long)frame->timestamp,
                                       (unsigned long)frame->id);
        h->transmitBufferLength += writtenBytes;
        if (frame->extended) {
            sprintf((char *)&h->transmitBuffer[h->transmitBufferLength], "true,");
            h->transmitBufferLength += 5;
        } else {
            sprintf((char *)&h->transmitBuffer[h->transmitBufferLength], "false,");
            h->transmitBufferLength += 6;
        }
        writtenBytes = (size_t)sprintf((char *)&h->transmitBuffer[h->transmitBufferLength],
                                       "Rx,%i,%i,", whichBus, frame->length);
        h->transmitBufferLength += writtenBytes;
        for (int c = 0; c < frame->length; c++) {
            writtenBytes = (size_t)sprintf((char *)&h->transmitBuffer[h->transmitBufferLength],
                                           "%x,", frame->data.uint8[c]);
            h->transmitBufferLength += writtenBytes;
        }
        sprintf((char *)&h->transmitBuffer[h->transmitBufferLength], "\n");
        h->transmitBufferLength += 1;
    }
}

size_t   gvret_num_available_bytes(GVRET_Handler_t *h) { return (size_t)h->transmitBufferLength; }
void     gvret_clear_buffered_bytes(GVRET_Handler_t *h) { h->transmitBufferLength = 0; }
uint8_t *gvret_get_buffered_bytes(GVRET_Handler_t *h)   { return h->transmitBuffer; }
