#ifndef CAN_MANAGER_IDF_H_
#define CAN_MANAGER_IDF_H_

#include "config_idf.h"
#include "esp32_can_idf.h"

typedef struct {
    uint32_t bitsPerQuarter;
    uint32_t bitsSoFar;
    uint8_t busloadPercentage;
} BUSLOAD;

#ifdef __cplusplus

class CANManager {
public:
    CANManager();
    void addBits(int offset, CAN_FRAME &frame);
    void sendFrame(CAN_COMMON *bus, CAN_FRAME &frame);
    void displayFrame(CAN_FRAME &frame, int whichBus);
    void loop();
    void setup();
private:
    BUSLOAD busLoad[NUM_BUSES];
    uint32_t busLoadTimer;
};
#endif

#endif
