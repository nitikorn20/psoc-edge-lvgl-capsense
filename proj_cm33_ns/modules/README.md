# Modules Index (CM33 NS)

This index summarizes module structure under `proj_cm33_ns/modules` and points to each module manual.

## Module Map

| Module | Purpose | Manual |
|---|---|---|
| `cm33_system` | CM33 platform/system bootstrap and CM55 release | `cm33_system/CM33_SYSTEM.md` |
| `error_handler` | Common fatal/error handling utilities | `error_handler/USER_MANUAL.md` |
| `event_bus` | Internal app event dispatch bus | `event_bus/USER_MANUAL.md` |
| `ipc_log` | IPC log transport/helpers | `ipc_log/USER_MANUAL.md` |
| `sensorhub_manager` | SensorHub orchestration and sampling (POT/BMI/CAP/TOUCH/BMM) | `sensorhub_manager/SENSORHUB_MANAGER.md` |
| `udp_server` | UDP server lifecycle and networking glue | `udp_server/UDP_SERVER.md` |
| `wifi_connect` | Wi-Fi connect state/task wrapper | `wifi_connect/WIFI_CONNECT.md` |
| `wifi_manager` | Wi-Fi manager command queue + state model | `wifi_manager/WIFI_MANAGER.md` |
| `wifi_profile_nvm` | Persist/load one Wi-Fi profile in user NVM | `wifi_profile_nvm/WIFI_PROFILE_NVM.md` |

## Notes

- Keep each module self-contained in its own folder (`.c`, `.h`, module `.md`).
- Keep one canonical manual per module (uppercase module name style where applicable).
- For new modules, follow the same structure used by existing module manuals.