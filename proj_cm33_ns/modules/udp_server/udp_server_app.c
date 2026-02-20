#include "udp_server_app.h"
#include "udp_server_lib.h"

#include "cy_secure_sockets.h"
#include "cy_wcm.h"
#include <stdio.h>
#include <string.h>

#define UDP_SERVER_APP_PORT (57345U)         /* Bind port. */
#define UDP_SERVER_APP_MAX_PEERS (4U)
#define UDP_SERVER_APP_MAX_PAYLOAD (256U)
#define UDP_SERVER_APP_RX_QUEUE_LEN (8U)
#define UDP_SERVER_APP_RECV_TIMEOUT_MS (1000U)
#define UDP_SERVER_APP_PROCESS_MAX_PACKETS (4U)  /* Max packets per process call. */
#define UDP_SERVER_APP_LED_ON_CMD ('1')
#define UDP_SERVER_APP_LED_OFF_CMD ('0')
#define UDP_SERVER_APP_LED_ON_ACK "LED ON ACK"
#define UDP_SERVER_APP_LED_OFF_ACK "LED OFF ACK"

static udp_server_t s_udp_server;
static udp_server_config_t s_udp_config;
static udp_server_callbacks_t s_udp_callbacks;
static bool s_udp_initialized = false;
static bool s_led_state_on = false;

static void udp_server_app_print_ipv4(uint32_t ipv4)  /* ipv4 in little-endian. */
{
  (void)printf("%u.%u.%u.%u",
               (unsigned int)(ipv4 & 0xFFU),
               (unsigned int)((ipv4 >> 8) & 0xFFU),
               (unsigned int)((ipv4 >> 16) & 0xFFU),
               (unsigned int)((ipv4 >> 24) & 0xFFU));
}

static void on_udp_data(udp_server_t *server, const uint8_t *data, size_t length,
                        const cy_socket_sockaddr_t *peer, void *user_ctx)
{
  (void)server;
  (void)peer;
  (void)user_ctx;

  if (0U == length)
  {
    return;
  }

  if (length >= sizeof(UDP_SERVER_APP_LED_ON_ACK) - 1U)
  {
    char buf[32];
    size_t copy_len = length;
    if (copy_len >= sizeof(buf))
    {
      copy_len = sizeof(buf) - 1U;
    }
    (void)memcpy(buf, data, copy_len);
    buf[copy_len] = '\0';
    if (0 == strcmp(buf, UDP_SERVER_APP_LED_ON_ACK))
    {
      s_led_state_on = true;
    }
    else if (0 == strcmp(buf, UDP_SERVER_APP_LED_OFF_ACK))
    {
      s_led_state_on = false;
    }
  }
}

bool udp_server_app_init(void)
{
  if (s_udp_initialized)
  {
    return true;
  }

  (void)memset(&s_udp_config, 0, sizeof(s_udp_config));
  s_udp_config.port = (uint16_t)UDP_SERVER_APP_PORT;
  s_udp_config.bind_ip_v4 = 0U;
  s_udp_config.max_peers = (uint16_t)UDP_SERVER_APP_MAX_PEERS;
  s_udp_config.max_payload_size = (uint16_t)UDP_SERVER_APP_MAX_PAYLOAD;
  s_udp_config.rx_queue_length = (uint16_t)UDP_SERVER_APP_RX_QUEUE_LEN;
  s_udp_config.recv_timeout_ms = UDP_SERVER_APP_RECV_TIMEOUT_MS;

  (void)memset(&s_udp_callbacks, 0, sizeof(s_udp_callbacks));
  s_udp_callbacks.on_data = on_udp_data;
  s_udp_callbacks.on_peer_added = NULL;
  s_udp_callbacks.on_peer_evicted = NULL;
  s_udp_callbacks.on_error = NULL;
  s_udp_callbacks.user_ctx = NULL;

  if (CY_RSLT_SUCCESS != udp_server_lib_init(&s_udp_server, &s_udp_config, &s_udp_callbacks))
  {
    return false;
  }

  s_udp_initialized = true;
  (void)printf("UDP server initialized (port %u, starts on Wi-Fi connect)\n",
               (unsigned int)UDP_SERVER_APP_PORT);
  return true;
}

void udp_server_app_start(void)
{
  cy_rslt_t result;

  if (!s_udp_initialized)
  {
    return;
  }

  (void)cy_socket_init();
  result = udp_server_socket_start(&s_udp_server);
  if (CY_RSLT_SUCCESS == result)
  {
    cy_wcm_ip_address_t ip_addr;
    (void)memset(&ip_addr, 0, sizeof(ip_addr));
    if (CY_RSLT_SUCCESS == cy_wcm_get_ip_addr(CY_WCM_INTERFACE_TYPE_STA, &ip_addr))
    {
      uint32_t ipv4 = ip_addr.ip.v4;
      (void)printf("UDP server started at ");
      udp_server_app_print_ipv4(ipv4);
      (void)printf(":%u\n", (unsigned int)UDP_SERVER_APP_PORT);
    }
    else
    {
      (void)printf("UDP server started on port %u (IP unknown)\n", (unsigned int)UDP_SERVER_APP_PORT);
    }
  }
}

void udp_server_app_stop(void)
{
  if (!s_udp_initialized)
  {
    return;
  }

  (void)udp_server_stop(&s_udp_server);
  (void)printf("UDP server stopped\n");
}

void udp_server_app_process(void)
{
  if (!s_udp_initialized)
  {
    return;
  }

  (void)udp_server_process(&s_udp_server, UDP_SERVER_APP_PROCESS_MAX_PACKETS);
}

void udp_server_app_send(const uint8_t *data, size_t length)
{
  if ((!s_udp_initialized) || (NULL == data) || (0U == length))
  {
    return;
  }

  if (0U < udp_server_get_peer_count(&s_udp_server))
  {
    (void)udp_server_send(&s_udp_server, data, length);
  }
}

void udp_server_app_send_led_toggle(void)
{
  uint8_t cmd;
  uint16_t i;
  cy_socket_sockaddr_t peer;
  uint16_t peer_count = udp_server_get_peer_count(&s_udp_server);

  if (!s_udp_initialized)
  {
    return;
  }

  if (0U >= peer_count)
  {
    return;
  }

  cmd = s_led_state_on ? (uint8_t)UDP_SERVER_APP_LED_OFF_CMD : (uint8_t)UDP_SERVER_APP_LED_ON_CMD;

  for (i = 0U; i < UDP_SERVER_APP_MAX_PEERS; i++)
  {
    if (udp_server_get_peer(&s_udp_server, i, &peer))
    {
      (void)udp_server_send_to(&s_udp_server, &cmd, 1U, &peer);
    }
  }
}
