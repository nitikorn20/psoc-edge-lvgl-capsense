#ifndef CM33_IPC_PIPE_H
#define CM33_IPC_PIPE_H

#include "ipc_communication.h"
#include "user_buttons.h"
#include <stdbool.h>

bool cm33_ipc_pipe_start(void);

bool cm33_ipc_send_gyro_data(const gyro_data_t *data, uint32_t sequence);
bool cm33_ipc_send_button_event(const button_event_t *event);
bool cm33_ipc_send_wifi_scan_results(const wifi_info_t *results, uint32_t count);
bool cm33_ipc_send_wifi_scan_complete(const ipc_wifi_scan_complete_t *scan_complete);
bool cm33_ipc_send_wifi_status(const ipc_wifi_status_t *status);
bool cm33_ipc_send_sensorhub_sample(const ipc_sensorhub_sample_t *sample);
bool cm33_ipc_send_sensorhub_status(const ipc_sensorhub_status_t *status);

#endif /* CM33_IPC_PIPE_H */
