/*
 * config_idf.h - ESP-IDF configuration
 */
#ifndef CONFIG_IDF_H_
#define CONFIG_IDF_H_

#include <stdint.h>
#include <stdbool.h>

/* TWAI (built-in CAN) pins -> TJA1050 transceiver module
 * TX: ESP32 GPIO -> TJA1050 TX (direct, 3.3V is OK for 5V input)
 * RX: TJA1050 RX -> 1k/2k voltage divider -> ESP32 GPIO (5V -> 3.33V)
 */
#define TWAI_TX_PIN   4
#define TWAI_RX_PIN   5

#define CAN_SPEED_500K  500000

#define SSID_NAME   "AVTOTOR_CAN"
#define WPA2KEY     "mypassword"

/* ESP32-S3: many dev kits use GPIO 2 for built-in LED; some use 48 (RGB) */
#define ESP32_BUILTIN_LED  2
#define CAN_RX_LED_TOGGLE  250

#define SER_BUFF_SIZE        1024
#define WIFI_BUFF_SIZE       2048
#define SER_BUFF_FLUSH_INTERVAL  20000

#define CFG_BUILD_NUM  420
#define CFG_VERSION    "ESP32_RET_SD v2.0 TWAI (Mar 2025)"
#define PREF_NAME      "ESP32_RET_SD"

#define NUM_BUSES  2
#define BLINK_SLOWNESS  100
#define MAX_CLIENTS  1

/* GVRET protocol states etc. are in gvret_comm.h */

typedef struct {
    uint32_t id;
    uint32_t mask;
    bool extended;
    bool enabled;
} FILTER;

typedef struct {
    uint32_t CAN0Speed;
    bool CAN0_Enabled;
    bool CAN0ListenOnly;
    uint32_t CAN1Speed;
    bool CAN1_Enabled;
    bool CAN1ListenOnly;
    bool useBinarySerialComm;
    uint8_t logLevel;
    uint8_t systemType;
    bool enableBT;
    char btName[32];
    bool enableLawicel;
    uint8_t wifiMode;
    char SSID[32];
    char WPA2Key[64];
} EEPROMSettings;

/* Telnet client: we store socket fd instead of WiFiClient */
typedef struct {
    int socket_fd;       /* -1 if not connected */
    bool connected;
} ClientNode;

typedef struct {
    uint8_t LED_CANTX;
    uint8_t LED_CANRX;
    uint8_t LED_SD;
    uint8_t LED_LOGGING;
    bool txToggle;
    bool rxToggle;
    bool sdToggle;
    bool logToggle;
    bool lawicelMode;
    int8_t numBuses;
    ClientNode clientNodes[MAX_CLIENTS];
    bool isWifiConnected;
    bool isWifiActive;
} SystemSettings;

extern EEPROMSettings settings;
extern SystemSettings SysSettings;

#endif /* CONFIG_IDF_H_ */
