#include "wifi_manager_idf.h"
#include "config_idf.h"
#include "gvret_comm.h"
#include "uart_serial.h"
#include "esp_timer.h"
#include "sys_io_idf.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#define TELNET_PORT 23
#define OTA_PORT 3232
#define WIFI_CHANNEL 1
#define MAX_CLIENTS 1

extern GVRET_Comm_Handler wifiGVRET;

static int listen_fd = -1;
static int udp_bcast_fd = -1;   /* reused UDP socket, created once */
static uint32_t last_broadcast = 0;
static bool wifi_started = false;

static void wifi_init_softap(void) {
    esp_netif_create_default_wifi_ap();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    wifi_config_t wcfg = {};
    strncpy((char *)wcfg.ap.ssid, settings.SSID, sizeof(wcfg.ap.ssid) - 1);
    wcfg.ap.ssid_len = (uint8_t)strlen(settings.SSID);
    strncpy((char *)wcfg.ap.password, settings.WPA2Key, sizeof(wcfg.ap.password) - 1);
    wcfg.ap.channel = WIFI_CHANNEL;
    wcfg.ap.max_connection = 4;
    wcfg.ap.authmode = (strlen(settings.WPA2Key) >= 8) ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
    wcfg.ap.pmf_cfg.required = false;
    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &wcfg);
    esp_wifi_start();
    wifi_started = true;
}

static int open_tcp_listen(int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((uint16_t)port);
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        close(fd);
        return -1;
    }
    if (listen(fd, 2) != 0) {
        close(fd);
        return -1;
    }
    return fd;
}

void wifi_manager_setup(void) {
    if (settings.wifiMode != 2) return;
    esp_netif_init();
    esp_event_loop_create_default();
    wifi_init_softap();
    SysSettings.isWifiConnected = true;
    uart_serial_print("\nWifi SSID: ");
    uart_serial_println(settings.SSID);
    esp_netif_ip_info_t info;
    esp_netif_t *ap = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (ap && esp_netif_get_ip_info(ap, &info) == ESP_OK) {
        uint32_t ip = info.ip.addr;
        uart_serial_printf("IP address: %d.%d.%d.%d\n",
            (int)(ip & 0xff), (int)((ip >> 8) & 0xff),
            (int)((ip >> 16) & 0xff), (int)((ip >> 24) & 0xff));
    }
    listen_fd = open_tcp_listen(TELNET_PORT);
    if (listen_fd < 0) {
        uart_serial_println("Telnet server bind failed");
    } else {
        fcntl(listen_fd, F_SETFL, fcntl(listen_fd, F_GETFL, 0) | O_NONBLOCK);
        uart_serial_println("Telnet port 23 ready");
    }

    /* Create UDP broadcast socket once — reused every second */
    udp_bcast_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_bcast_fd >= 0) {
        int opt = 1;
        setsockopt(udp_bcast_fd, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt));
        fcntl(udp_bcast_fd, F_SETFL, fcntl(udp_bcast_fd, F_GETFL, 0) | O_NONBLOCK);
    }
}

static void accept_new_clients(void) {
    if (listen_fd < 0) return;
    struct sockaddr_in client_addr;
    socklen_t len = sizeof(client_addr);
    int fd = accept(listen_fd, (struct sockaddr *)&client_addr, &len);
    if (fd < 0) return;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (SysSettings.clientNodes[i].socket_fd < 0 || !SysSettings.clientNodes[i].connected) {
            if (SysSettings.clientNodes[i].socket_fd >= 0) close(SysSettings.clientNodes[i].socket_fd);
            SysSettings.clientNodes[i].socket_fd = fd;
            SysSettings.clientNodes[i].connected = true;
            setLED(SysSettings.LED_CANRX, 0);
            uart_serial_printf("New client #%d\n", i);
            return;
        }
    }
    close(fd);
}

static void poll_clients(void) {
    uint8_t buf[64];
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (SysSettings.clientNodes[i].socket_fd < 0 || !SysSettings.clientNodes[i].connected) continue;
        int n = recv(SysSettings.clientNodes[i].socket_fd, buf, sizeof(buf), MSG_DONTWAIT);
        if (n > 0) {
            SysSettings.isWifiActive = true;
            for (int j = 0; j < n; j++)
                wifiGVRET.processIncomingByte(buf[j]);
        } else if (n == 0 || (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
            close(SysSettings.clientNodes[i].socket_fd);
            SysSettings.clientNodes[i].socket_fd = -1;
            SysSettings.clientNodes[i].connected = false;
        }
    }
}

void wifi_manager_loop(void) {
    if (!wifi_started || listen_fd < 0) return;
    accept_new_clients();
    poll_clients();
    /* UDP broadcast every second — reuse static socket, no alloc per call */
    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
    if (udp_bcast_fd >= 0 && (now - last_broadcast > 1000)) {
        last_broadcast = now;
        struct sockaddr_in addr = {};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(0xFFFFFFFF);
        addr.sin_port = htons(17222);
        uint8_t ping[] = { 0x1C, 0xEF, 0xAC, 0xED };
        sendto(udp_bcast_fd, ping, 4, 0, (struct sockaddr *)&addr, sizeof(addr));
    }
}

void wifi_manager_send_buffered_data(void) {
    size_t len = wifiGVRET.numAvailableBytes();
    if (len == 0) return;
    uint8_t *buf = wifiGVRET.getBufferedBytes();
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (SysSettings.clientNodes[i].socket_fd >= 0 && SysSettings.clientNodes[i].connected)
            send(SysSettings.clientNodes[i].socket_fd, buf, len, MSG_DONTWAIT);
    }
    wifiGVRET.clearBufferedBytes();
}
