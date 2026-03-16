/*
 * esp32_can_idf.h - TWAI CAN driver interface
 * Uses ESP32-S3 built-in TWAI peripheral + external TJA1050 transceiver.
 * Single bus (CAN0); CAN1 stubbed.
 */
#ifndef ESP32_CAN_IDF_H_
#define ESP32_CAN_IDF_H_

#include <stdint.h>
#include <stdbool.h>
#include "config_idf.h"

#ifdef __cplusplus
extern "C" {
#endif

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

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

class CAN_COMMON {
public:
    virtual ~CAN_COMMON() {}
    virtual void setCANPins(int rxPin, int txPin) = 0;
    virtual bool begin(uint32_t baud, int rxPin = 255, int txPin = 255) = 0;
    virtual void disable() = 0;
    virtual void enable() = 0;
    virtual void setListenOnlyMode(bool mode) = 0;
    virtual void watchFor() = 0;
    virtual int available() = 0;
    virtual void read(CAN_FRAME &frame) = 0;
    virtual void sendFrame(CAN_FRAME &frame) = 0;
};

/* TWAI CAN bus — used as CAN0 */
class ESP32CAN : public CAN_COMMON {
public:
    ESP32CAN();
    void setCANPins(int rxPin, int txPin);
    bool begin(uint32_t baud, int rxPin = 255, int txPin = 255);
    void disable();
    void enable();
    void setListenOnlyMode(bool mode);
    void watchFor();
    int available();
    void read(CAN_FRAME &frame);
    void sendFrame(CAN_FRAME &frame);
private:
    int _rxPin;
    int _txPin;
    bool _enabled;
    bool _listenOnly;
    uint32_t _baud;
    bool _started;
};

/* Stub for second bus (not used) */
class ESP32CAN_Stub : public CAN_COMMON {
public:
    void setCANPins(int rxPin, int txPin) override { (void)rxPin; (void)txPin; }
    bool begin(uint32_t baud, int rxPin = 255, int txPin = 255) override { (void)baud; (void)rxPin; (void)txPin; return false; }
    void disable() override {}
    void enable() override {}
    void setListenOnlyMode(bool mode) override { (void)mode; }
    void watchFor() override {}
    int available() override { return 0; }
    void read(CAN_FRAME &frame) override { (void)frame; }
    void sendFrame(CAN_FRAME &frame) override { (void)frame; }
};

extern ESP32CAN CAN0;
extern ESP32CAN_Stub CAN1;
#endif

#endif /* ESP32_CAN_IDF_H_ */
