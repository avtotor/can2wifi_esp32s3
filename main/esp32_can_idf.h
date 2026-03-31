/*
 * esp32_can_idf.h - TWAI CAN driver interface
 * Uses ESP32-S3 built-in TWAI peripheral + external TJA1050 transceiver.
 * Single bus (CAN0); CAN1 stubbed out.
 */
#ifndef ESP32_CAN_IDF_H_
#define ESP32_CAN_IDF_H_

#include <stdint.h>
#include <stdbool.h>
#include "config_idf.h"

typedef union {
    uint8_t bytes[8];
    uint8_t uint8[8];
} CAN_DATA;

typedef struct {
    uint32_t id;
    uint32_t timestamp;  /* milliseconds when frame was received */
    bool extended;
    uint8_t length;
    uint8_t rtr;
    CAN_DATA data;
} CAN_FRAME;

/* CAN0 (TWAI) interface */
void can0_set_pins(int rxPin, int txPin);
bool can0_begin(uint32_t baud);
void can0_disable(void);
void can0_enable(void);
void can0_set_listen_only(bool mode);
int  can0_available(void);
void can0_read(CAN_FRAME *frame);
void can0_send_frame(CAN_FRAME *frame);

/* CAN1 stub — not implemented, all no-ops */
static inline void can1_begin(uint32_t baud, bool listenOnly) { (void)baud; (void)listenOnly; }
static inline void can1_disable(void) {}
static inline void can1_send_frame(CAN_FRAME *frame) { (void)frame; }

#endif /* ESP32_CAN_IDF_H_ */
