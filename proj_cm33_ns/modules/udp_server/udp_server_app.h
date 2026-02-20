#ifndef UDP_SERVER_APP_H_
#define UDP_SERVER_APP_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** Initializes UDP server (port 57345). Call before start/process/send. Returns true on success. */
bool udp_server_app_init(void);

/** Starts server socket; call after Wi-Fi connected. Idempotent. */
void udp_server_app_start(void);

/** Stops server socket and clears peers. */
void udp_server_app_stop(void);

/** Processes RX queue; invoke from main loop. */
void udp_server_app_process(void);

/** Sends data to last peer; no-op if no peer. */
void udp_server_app_send(const uint8_t *data, size_t length);

/** Sends LED toggle cmd ('0'/'1') to all tracked peers. */
void udp_server_app_send_led_toggle(void);

#endif
