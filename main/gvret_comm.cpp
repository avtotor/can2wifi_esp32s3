/*
 * GVRET protocol handler - ESP-IDF port
 */
#include "gvret_comm.h"
#include "can_manager.h"
#include "config_idf.h"
#include "logger_idf.h"
#include "esp_timer.h"
#include <stdio.h>
#include <string.h>

extern CANManager canManager;

/* esp_timer_get_time() returns microseconds — no division needed */
#define micros()  ((uint32_t)(esp_timer_get_time()))

GVRET_Comm_Handler::GVRET_Comm_Handler() {
    step = 0;
    state = IDLE;
    transmitBufferLength = 0;
}

void GVRET_Comm_Handler::processIncomingByte(uint8_t in_byte) {
    uint32_t busSpeed = 0;
    uint32_t now = micros();
    uint8_t temp8;

    switch (state) {
    case IDLE:
        if (in_byte == 0xF1) {
            state = GET_COMMAND;
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
            state = BUILD_CAN_FRAME;
            buff[0] = 0xF1;
            step = 0;
            break;
        case PROTO_TIME_SYNC:
            state = TIME_SYNC;
            step = 0;
            transmitBuffer[transmitBufferLength++] = 0xF1;
            transmitBuffer[transmitBufferLength++] = 1;
            transmitBuffer[transmitBufferLength++] = (uint8_t)(now & 0xFF);
            transmitBuffer[transmitBufferLength++] = (uint8_t)(now >> 8);
            transmitBuffer[transmitBufferLength++] = (uint8_t)(now >> 16);
            transmitBuffer[transmitBufferLength++] = (uint8_t)(now >> 24);
            break;
        case PROTO_DIG_INPUTS:
            temp8 = 0;
            transmitBuffer[transmitBufferLength++] = 0xF1;
            transmitBuffer[transmitBufferLength++] = 2;
            transmitBuffer[transmitBufferLength++] = temp8;
            transmitBuffer[transmitBufferLength++] = checksumCalc(buff, 2);
            state = IDLE;
            break;
        case PROTO_ANA_INPUTS:
            transmitBuffer[transmitBufferLength++] = 0xF1;
            transmitBuffer[transmitBufferLength++] = 3;
            for (int i = 0; i < 7; i++) {
                transmitBuffer[transmitBufferLength++] = 0;
                transmitBuffer[transmitBufferLength++] = 0;
            }
            transmitBuffer[transmitBufferLength++] = checksumCalc(buff, 9);
            state = IDLE;
            break;
        case PROTO_SET_DIG_OUT:
            state = SET_DIG_OUTPUTS;
            buff[0] = 0xF1;
            break;
        case PROTO_SETUP_CANBUS:
            state = SETUP_CANBUS;
            step = 0;
            buff[0] = 0xF1;
            break;
        case PROTO_GET_CANBUS_PARAMS:
            transmitBuffer[transmitBufferLength++] = 0xF1;
            transmitBuffer[transmitBufferLength++] = 6;
            transmitBuffer[transmitBufferLength++] = settings.CAN0_Enabled + ((unsigned char)settings.CAN0ListenOnly << 4);
            transmitBuffer[transmitBufferLength++] = settings.CAN0Speed & 0xFF;
            transmitBuffer[transmitBufferLength++] = (settings.CAN0Speed >> 8) & 0xFF;
            transmitBuffer[transmitBufferLength++] = (settings.CAN0Speed >> 16) & 0xFF;
            transmitBuffer[transmitBufferLength++] = (settings.CAN0Speed >> 24) & 0xFF;
            transmitBuffer[transmitBufferLength++] = 0;
            transmitBuffer[transmitBufferLength++] = settings.CAN1Speed & 0xFF;
            transmitBuffer[transmitBufferLength++] = (settings.CAN1Speed >> 8) & 0xFF;
            transmitBuffer[transmitBufferLength++] = (settings.CAN1Speed >> 16) & 0xFF;
            transmitBuffer[transmitBufferLength++] = (settings.CAN1Speed >> 24) & 0xFF;
            state = IDLE;
            break;
        case PROTO_GET_DEV_INFO:
            transmitBuffer[transmitBufferLength++] = 0xF1;
            transmitBuffer[transmitBufferLength++] = 7;
            transmitBuffer[transmitBufferLength++] = CFG_BUILD_NUM & 0xFF;
            transmitBuffer[transmitBufferLength++] = (CFG_BUILD_NUM >> 8) & 0xFF;
            transmitBuffer[transmitBufferLength++] = 0x20;
            transmitBuffer[transmitBufferLength++] = 0;
            transmitBuffer[transmitBufferLength++] = 0;
            transmitBuffer[transmitBufferLength++] = 0;
            state = IDLE;
            break;
        case PROTO_SET_SW_MODE:
            buff[0] = 0xF1;
            state = SET_SINGLEWIRE_MODE;
            step = 0;
            break;
        case PROTO_KEEPALIVE:
            transmitBuffer[transmitBufferLength++] = 0xF1;
            transmitBuffer[transmitBufferLength++] = 0x09;
            transmitBuffer[transmitBufferLength++] = 0xDE;
            transmitBuffer[transmitBufferLength++] = 0xAD;
            state = IDLE;
            break;
        case PROTO_SET_SYSTYPE:
            buff[0] = 0xF1;
            state = SET_SYSTYPE;
            step = 0;
            break;
        case PROTO_ECHO_CAN_FRAME:
            state = ECHO_CAN_FRAME;
            buff[0] = 0xF1;
            step = 0;
            break;
        case PROTO_GET_NUMBUSES:
            transmitBuffer[transmitBufferLength++] = 0xF1;
            transmitBuffer[transmitBufferLength++] = 12;
            transmitBuffer[transmitBufferLength++] = SysSettings.numBuses;
            state = IDLE;
            break;
        case PROTO_GET_EXT_BUSES:
            transmitBuffer[transmitBufferLength++] = 0xF1;
            transmitBuffer[transmitBufferLength++] = 13;
            for (int u = 2; u < 17; u++) transmitBuffer[transmitBufferLength++] = 0;
            step = 0;
            state = IDLE;
            break;
        case PROTO_SET_EXT_BUSES:
            state = SETUP_EXT_BUSES;
            step = 0;
            buff[0] = 0xF1;
            break;
        }
        break;
    case BUILD_CAN_FRAME:
        buff[1 + step] = in_byte;
        switch (step) {
        case 0: build_out_frame.id = in_byte; break;
        case 1: build_out_frame.id |= in_byte << 8; break;
        case 2: build_out_frame.id |= in_byte << 16; break;
        case 3:
            build_out_frame.id |= in_byte << 24;
            if (build_out_frame.id & (1u << 31)) {
                build_out_frame.id &= 0x7FFFFFFF;
                build_out_frame.extended = true;
            } else build_out_frame.extended = false;
            break;
        case 4: out_bus = in_byte & 3; break;
        case 5:
            build_out_frame.length = in_byte & 0xF;
            if (build_out_frame.length > 8) build_out_frame.length = 8;
            break;
        default:
            if (step < (int)(6 + build_out_frame.length)) {
                build_out_frame.data.uint8[step - 6] = in_byte;
            } else {
                state = IDLE;
                build_out_frame.rtr = 0;
                if (out_bus == 0) canManager.sendFrame(&CAN0, build_out_frame);
                if (out_bus == 1) canManager.sendFrame(&CAN1, build_out_frame);
            }
            break;
        }
        step++;
        break;
    case TIME_SYNC:
        state = IDLE;
        break;
    case SET_DIG_OUTPUTS:
        state = IDLE;
        break;
    case GET_DIG_INPUTS:
    case GET_ANALOG_INPUTS:
        state = IDLE;
        break;
    case SETUP_CANBUS:
        switch (step) {
        case 0: build_int = in_byte; break;
        case 1: build_int |= in_byte << 8; break;
        case 2: build_int |= in_byte << 16; break;
        case 3:
            build_int |= in_byte << 24;
            busSpeed = build_int & 0xFFFFF;
            if (busSpeed > 1000000) busSpeed = 1000000;
            if (build_int > 0) {
                if (build_int & 0x80000000ul) {
                    settings.CAN0_Enabled = (build_int & 0x40000000ul) != 0;
                    settings.CAN0ListenOnly = (build_int & 0x20000000ul) != 0;
                } else settings.CAN0_Enabled = true;
                settings.CAN0Speed = busSpeed;
            } else settings.CAN0_Enabled = false;
            if (settings.CAN0_Enabled) {
                CAN0.begin(settings.CAN0Speed, 255, 255);
                CAN0.setListenOnlyMode(settings.CAN0ListenOnly);
                CAN0.watchFor();
            } else CAN0.disable();
            break;
        case 4: build_int = in_byte; break;
        case 5: build_int |= in_byte << 8; break;
        case 6: build_int |= in_byte << 16; break;
        case 7:
            build_int |= in_byte << 24;
            busSpeed = build_int & 0xFFFFF;
            if (busSpeed > 1000000) busSpeed = 1000000;
            if (build_int > 0 && SysSettings.numBuses > 1) {
                if (build_int & 0x80000000ul) {
                    settings.CAN1_Enabled = (build_int & 0x40000000ul) != 0;
                    settings.CAN1ListenOnly = (build_int & 0x20000000ul) != 0;
                } else settings.CAN1_Enabled = true;
                settings.CAN1Speed = busSpeed;
            } else settings.CAN1_Enabled = false;
            if (settings.CAN1_Enabled) {
                CAN1.begin(settings.CAN1Speed, 255, 255);
                CAN1.setListenOnlyMode(settings.CAN1ListenOnly);
                CAN1.watchFor();
            } else CAN1.disable();
            state = IDLE;
            break;
        }
        step++;
        break;
    case GET_CANBUS_PARAMS:
    case GET_DEVICE_INFO:
        break;
    case SET_SINGLEWIRE_MODE:
        state = IDLE;
        break;
    case SET_SYSTYPE:
        settings.systemType = in_byte;
        state = IDLE;
        break;
    case ECHO_CAN_FRAME:
        buff[1 + step] = in_byte;
        switch (step) {
        case 0: build_out_frame.id = in_byte; break;
        case 1: build_out_frame.id |= in_byte << 8; break;
        case 2: build_out_frame.id |= in_byte << 16; break;
        case 3:
            build_out_frame.id |= in_byte << 24;
            if (build_out_frame.id & (1u << 31)) {
                build_out_frame.id &= 0x7FFFFFFF;
                build_out_frame.extended = true;
            } else build_out_frame.extended = false;
            break;
        case 4: out_bus = in_byte & 1; break;
        case 5:
            build_out_frame.length = in_byte & 0xF;
            if (build_out_frame.length > 8) build_out_frame.length = 8;
            break;
        default:
            if (step < (int)(6 + build_out_frame.length))
                build_out_frame.data.bytes[step - 6] = in_byte;
            else {
                state = IDLE;
                canManager.displayFrame(build_out_frame, 0);
            }
            break;
        }
        step++;
        break;
    case SETUP_EXT_BUSES:
        switch (step) {
        case 0: case 1: case 2: case 3: case 4: case 5: case 6: case 7:
        case 8: case 9: case 10:
            build_int = in_byte; /* simplified: just consume bytes */
            break;
        case 11:
            build_int |= in_byte << 24;
            state = IDLE;
            break;
        }
        step++;
        break;
    }
}

void GVRET_Comm_Handler::sendFrameToBuffer(CAN_FRAME &frame, int whichBus) {
    uint8_t temp;
    size_t writtenBytes;
    if (settings.useBinarySerialComm) {
        if (frame.extended) frame.id |= 1u << 31;
        transmitBuffer[transmitBufferLength++] = 0xF1;
        transmitBuffer[transmitBufferLength++] = 0;
        uint32_t now = micros();
        transmitBuffer[transmitBufferLength++] = (uint8_t)(now & 0xFF);
        transmitBuffer[transmitBufferLength++] = (uint8_t)(now >> 8);
        transmitBuffer[transmitBufferLength++] = (uint8_t)(now >> 16);
        transmitBuffer[transmitBufferLength++] = (uint8_t)(now >> 24);
        transmitBuffer[transmitBufferLength++] = (uint8_t)(frame.id & 0xFF);
        transmitBuffer[transmitBufferLength++] = (uint8_t)(frame.id >> 8);
        transmitBuffer[transmitBufferLength++] = (uint8_t)(frame.id >> 16);
        transmitBuffer[transmitBufferLength++] = (uint8_t)(frame.id >> 24);
        transmitBuffer[transmitBufferLength++] = frame.length + (uint8_t)(whichBus << 4);
        for (int c = 0; c < frame.length; c++)
            transmitBuffer[transmitBufferLength++] = frame.data.uint8[c];
        temp = 0;
        transmitBuffer[transmitBufferLength++] = temp;
    } else {
        writtenBytes = (size_t)sprintf((char *)&transmitBuffer[transmitBufferLength], "%lu,%lX,", (unsigned long)frame.timestamp, (unsigned long)frame.id);
        transmitBufferLength += writtenBytes;
        if (frame.extended) {
            sprintf((char *)&transmitBuffer[transmitBufferLength], "true,");
            transmitBufferLength += 5;
        } else {
            sprintf((char *)&transmitBuffer[transmitBufferLength], "false,");
            transmitBufferLength += 6;
        }
        writtenBytes = (size_t)sprintf((char *)&transmitBuffer[transmitBufferLength], "Rx,%i,%i,", whichBus, frame.length);
        transmitBufferLength += writtenBytes;
        for (int c = 0; c < frame.length; c++) {
            writtenBytes = (size_t)sprintf((char *)&transmitBuffer[transmitBufferLength], "%x,", frame.data.uint8[c]);
            transmitBufferLength += writtenBytes;
        }
        sprintf((char *)&transmitBuffer[transmitBufferLength], "\n");
        transmitBufferLength += 1;
    }
}

size_t GVRET_Comm_Handler::numAvailableBytes() { return (size_t)transmitBufferLength; }
void GVRET_Comm_Handler::clearBufferedBytes() { transmitBufferLength = 0; }
uint8_t *GVRET_Comm_Handler::getBufferedBytes() { return transmitBuffer; }

uint8_t GVRET_Comm_Handler::checksumCalc(uint8_t *buffer, int length) {
    uint8_t valu = 0;
    for (int c = 0; c < length; c++) valu ^= buffer[c];
    return valu;
}
