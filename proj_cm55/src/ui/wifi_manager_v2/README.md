# Wi-Fi Manager V2 UI

This folder contains the new Wi-Fi Manager UI flow that mirrors the POC behavior while using the IPC protocol of this project.

## Files

- `wifi_manager_v2_screen.c/.h`
  - LVGL screen layout and interaction callbacks.
  - Buttons: `Scan`, `Show Cached`, `Disconnect`.
  - Header switch: `Fixed Pass ON/OFF` for locked AP testing.
  - AP list rendering and password dialog.
- `wifi_manager_v2_state.c/.h`
  - IPC-facing state synchronization with `cm55_ipc_app`.
  - Polling status, scan list, and debug log snapshots.
  - IPC action wrappers for scan/connect/disconnect/status.
- `wifi_manager_v2_types.h`
  - Shared snapshot and limits used by the screen/state modules.

## Entry Point

`run_example()` route `example_9` now calls:

`wifi_manager_v2_screen_create(lv_screen_active());`

Legacy dashboard files under `src/ui/widgets` are kept unchanged for reference.

## Fixed Password Mode

- Compile-time default: `WIFI_MANAGER_V2_USE_FIXED_PASS_DEFAULT` in `wifi_manager_v2_screen.c`.
- Fixed password string: `WIFI_MANAGER_V2_FIXED_PASSWORD` in `wifi_manager_v2_screen.c`.
- When enabled in UI, locked AP connect requests always use the fixed password and skip keyboard input.
