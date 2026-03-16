#include "can_manager.h"
#include "esp32_can_idf.h"
#include "config_idf.h"
#include "gvret_comm.h"
#include "uart_serial.h"
#include "sys_io_idf.h"

uint8_t CAN_frame_count;

extern GVRET_Comm_Handler serialGVRET;
extern GVRET_Comm_Handler wifiGVRET;

CANManager::CANManager() {}

void CANManager::setup() {
    if (settings.CAN0_Enabled) {
        if (settings.systemType == 0) {
            CAN0.setCANPins(TWAI_RX_PIN, TWAI_TX_PIN);
            CAN0.enable();
            CAN0.begin(settings.CAN0Speed);
            uart_serial_print("\nCAN0 enabled at: ");
            uart_serial_print_int((int)settings.CAN0Speed);
            uart_serial_println(" bits/sec");
        }
        if (settings.CAN0ListenOnly)
            CAN0.setListenOnlyMode(true);
        else
            CAN0.setListenOnlyMode(false);
        CAN0.watchFor();
    } else {
        CAN0.disable();
    }
}

void CANManager::addBits(int offset, CAN_FRAME &frame) {
    if (offset < 0 || offset >= NUM_BUSES) return;
    busLoad[offset].bitsSoFar += 41 + (frame.length * 9);
    if (frame.extended) busLoad[offset].bitsSoFar += 18;
}

void CANManager::sendFrame(CAN_COMMON *bus, CAN_FRAME &frame) {
    bus->sendFrame(frame);
    int whichBus = (bus == &CAN0) ? 0 : 1;
    addBits(whichBus, frame);
}

void CANManager::displayFrame(CAN_FRAME &frame, int whichBus) {
    if (SysSettings.isWifiActive)
        wifiGVRET.sendFrameToBuffer(frame, whichBus);
    /* no WiFi client — skip serial buffer write, it would only be cleared */
}

void CANManager::loop() {
    CAN_FRAME incoming;
    size_t wifiLength = wifiGVRET.numAvailableBytes();
    size_t serialLength = serialGVRET.numAvailableBytes();
    size_t maxLength = (wifiLength > serialLength) ? wifiLength : serialLength;

    while (CAN0.available() > 0 && (maxLength < (WIFI_BUFF_SIZE - 80))) {
        CAN0.read(incoming);
        addBits(0, incoming);

        if (CAN_frame_count >= CAN_RX_LED_TOGGLE) {
            toggleSD_LED();
            CAN_frame_count = 0;
        }
        CAN_frame_count++;

        displayFrame(incoming, 0);
        wifiLength = wifiGVRET.numAvailableBytes();
        serialLength = serialGVRET.numAvailableBytes();
        maxLength = (wifiLength > serialLength) ? wifiLength : serialLength;
    }
}
