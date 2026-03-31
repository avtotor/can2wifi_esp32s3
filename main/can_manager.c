#include "can_manager.h"
#include "esp32_can_idf.h"
#include "config_idf.h"
#include "gvret_comm.h"
#include "uart_serial.h"
#include "sys_io_idf.h"
#ifdef CAN_DEBUG
#include "driver/twai.h"
#include "esp_timer.h"
#endif

extern GVRET_Handler_t serialGVRET;
extern GVRET_Handler_t wifiGVRET;

static uint8_t s_frame_count = 0;

typedef struct {
    uint32_t bitsPerQuarter;
    uint32_t bitsSoFar;
    uint8_t  busloadPercentage;
} BusLoad;

static BusLoad s_busLoad[NUM_BUSES];

static void add_bits(int offset, CAN_FRAME *frame) {
    if (offset < 0 || offset >= NUM_BUSES) return;
    s_busLoad[offset].bitsSoFar += 41 + (frame->length * 9);
    if (frame->extended) s_busLoad[offset].bitsSoFar += 18;
}

void can_manager_send_frame(int bus_idx, CAN_FRAME *frame) {
    if (bus_idx == 0) can0_send_frame(frame);
    else              can1_send_frame(frame);
    add_bits(bus_idx, frame);
}

void can_manager_display_frame(CAN_FRAME *frame, int whichBus) {
    if (SysSettings.isWifiActive)
        gvret_send_frame_to_buffer(&wifiGVRET, frame, whichBus);
}

void can_manager_setup(void) {
    if (settings.CAN0_Enabled) {
        can0_set_pins(TWAI_RX_PIN, TWAI_TX_PIN);
        can0_enable();
        can0_set_listen_only(settings.CAN0ListenOnly);
        can0_begin(settings.CAN0Speed);
        uart_serial_print("\nCAN0 enabled at: ");
        uart_serial_print_int((int)settings.CAN0Speed);
        uart_serial_println(" bits/sec");
    } else {
        can0_disable();
    }
}

void can_manager_loop(void) {
#ifdef CAN_DEBUG
    static uint32_t lastDebug = 0;
    uint32_t nowMs = (uint32_t)(esp_timer_get_time() / 1000);
    if (nowMs - lastDebug >= 5000) {
        lastDebug = nowMs;
        twai_status_info_t st;
        if (twai_get_status_info(&st) == ESP_OK) {
            uart_serial_printf("[CAN_DEBUG] state=%d rx_err=%lu tx_err=%lu msgs_rx=%lu missed=%lu\n",
                (int)st.state, (unsigned long)st.rx_error_counter,
                (unsigned long)st.tx_error_counter, (unsigned long)st.msgs_to_rx,
                (unsigned long)st.rx_missed_count);
        }
    }
#endif
    CAN_FRAME incoming;
    size_t wifiLen   = gvret_num_available_bytes(&wifiGVRET);
    size_t serialLen = gvret_num_available_bytes(&serialGVRET);
    size_t maxLen    = (wifiLen > serialLen) ? wifiLen : serialLen;

    while (can0_available() > 0 && (maxLen < (WIFI_BUFF_SIZE - 80))) {
        can0_read(&incoming);
        add_bits(0, &incoming);

        if (s_frame_count >= CAN_RX_LED_TOGGLE) {
            toggleSD_LED();
            s_frame_count = 0;
        }
        s_frame_count++;

        can_manager_display_frame(&incoming, 0);
        wifiLen   = gvret_num_available_bytes(&wifiGVRET);
        serialLen = gvret_num_available_bytes(&serialGVRET);
        maxLen    = (wifiLen > serialLen) ? wifiLen : serialLen;
    }
}
