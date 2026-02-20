# PSOC&trade; Edge MCU: Multi-Core LVGL Demo with IPC, Wi‑Fi, and Display

---

**Last updated:** 2026-02-10

> This repository is in development. See [CHANGELOG.md](./CHANGELOG.md) for recent changes.

---

This project demonstrates a multi-core graphics application using the **Light and Versatile Graphics Library (LVGL)** on the PSOC&trade; Edge MCU. It coordinates tasks between the Cortex&reg;-M33 and Cortex&reg;-M55 cores using Inter-Processor Communication (IPC), and includes Wi‑Fi scanning, DSI display with touch, cross-core logging, and a modular UI stack.

## Features

- **Multi-Core Architecture**
  - **`proj_cm33_s`**: Secure Cortex&reg;-M33; secure boot, then hands off to non-secure CM33.
  - **`proj_cm33_ns`**: Non-secure Cortex&reg;-M33; system management, Wi‑Fi scanning, IPC pipe, and log transport to CM55.
  - **`proj_cm55`**: Cortex&reg;-M55; LVGL UI, display/touch, IPC pipe, RTOS stats, event bus, and datetime/CLIB support.
- **Graphics (LVGL)**
  - GPU/GFXSS (Graphics Subsystem) for rendering.
  - Flex/grid/stack layouts via **`lib_ui_layout`**; dashboard-style widgets and tabbed UI.
  - DSI display support with configurable panel (e.g. Waveshare 7" / 4.3", 10.1" TFT); touch via GT911 CTP where applicable.
- **IPC Pipe**
  - Shared definitions in **`shared/include/ipc_communication.h`**; message structure `ipc_msg_t` (client_id, cmd, value, data[]).
  - CM33: periodic counter messages and LED toggle on send; receives `IPC_CMD_START` from CM55.
  - CM55: sends `IPC_CMD_START` periodically; receives and processes counter/log messages.
- **Log Transport (CM33 → CM55)**
  - **`log_queue`**: FreeRTOS queue + `log_queue_printf()` on CM33 (non-blocking).
  - **`log_ipc_transport`**: Worker task on CM33 that forwards queue messages to CM55 via IPC with `IPC_CMD_LOG`; CM55 prints them on its UART so CM33 logs appear on one console.
- **Wi‑Fi (CM33)**
  - **`wifi_scan_task`**: Periodic Wi‑Fi scan (SSID, RSSI, channel, MAC, security); configurable filters (SSID, MAC, band, RSSI); results can be sent to CM55 via log transport.
- **Error Handling**
  - Custom error handlers on both cores: disable interrupts, assert, then infinite loop with LED blink for visual feedback.
- **RTOS & System**
  - FreeRTOS on CM33 and CM55; tickless idle using LPTimer. RTC and CLIB support initialized on CM33; CM55 uses RTC for datetime.
- **TESA / UI Stack (CM55)**
  - **tesa_display**: Display and LVGL init/tick.
  - **tesa_rtos_stats**: RTOS statistics for UI or debug.
  - **tesa_event_bus**, **tesa/utils/tesa_datetime**, **tesa/logging**: Event bus, datetime helpers, and logging utilities used by examples and UI.

### Architecture overview

```mermaid
flowchart LR
    subgraph CM33s["proj_cm33_s"]
        Boot[Secure boot]
    end
    subgraph CM33ns["proj_cm33_ns"]
        IPC33[IPC pipe]
        Log[log_queue + log_ipc_transport]
        WiFi[wifi_scan_task]
        Boot --> CM33ns
        IPC33 --> Log
        IPC33 --> WiFi
    end
    subgraph CM55["proj_cm55"]
        IPC55[IPC pipe]
        LVGL[LVGL + display/touch]
        UI[UI / widgets / tabview]
        IPC55 --> LVGL
        LVGL --> UI
    end
    Boot -->|jump NS| CM33ns
    CM33ns -->|Cy_SysEnableCM55| CM55
    CM33ns <-->|IPC Pipe EP1/EP2| CM55
```

---

## Supported Hardware

### Target Kits (`TARGET`)

- **PSOC&trade; Edge E84 Evaluation Kit** (`APP_KIT_PSE84_EVAL_EPC2`) &ndash; *Default / Tested*
- **PSOC&trade; Edge E84 Evaluation Kit** (`APP_KIT_PSE84_EVAL_EPC4`)
- **PSOC&trade; Edge E84 AI Kit** (`APP_KIT_PSE84_AI`)

### Supported Displays

Configured via `common.mk` (`CONFIG_DISPLAY`):

- **Waveshare 7-inch Raspberry Pi DSI LCD (C)** (1024×600) &ndash; *Currently configured*
- **Waveshare 4.3-inch Raspberry Pi DSI LCD** (800×480)
- **10.1-inch TFT** (WF101JTYAHMNB0) (1024×600)

Touch: GT911 CTP driver used where applicable (e.g. `touch-ctp-gt911.mtb` in `proj_cm55`).

---

## Software Requirements

- [ModusToolbox&trade; Software](https://www.infineon.com/modustoolbox) v3.6 or later.

### Using Git Bash (make in PATH)

ModusToolbox installs GNU make under `tools_3.x\modus-shell\bin`. To use it from Git Bash, add that directory to your PATH.

1. The make path is typically `C:\Users\<You>\ModusToolbox\tools_3.6\modus-shell\bin` (e.g. `C:/Users/drsanti/ModusToolbox/tools_3.6/modus-shell/bin/make.exe`).
2. In Git Bash, edit `~/.bashrc` (or `~/.bash_profile`) and add (use Git Bash style: `/c/Users/drsanti/ModusToolbox/tools_3.6/modus-shell/bin`):

   ```bash
   export PATH="/c/Users/drsanti/ModusToolbox/tools_3.6/modus-shell/bin:$PATH"
   ```
3. Restart Git Bash or run `source ~/.bashrc`. Then `make library-manager` and `make getlibs` will work from the repo root.

See [docs/BUILD_SETUP.md](docs/BUILD_SETUP.md) for more detail.

## Getting Started

### 1. Configure target and display

Edit **`common.mk`** (in each project directory that includes it) to set target and display:

```makefile
TARGET=APP_KIT_PSE84_EVAL_EPC2
CONFIG_DISPLAY=WS7P0DSI_RPI_DISP
```

Use the Library Manager (`make library-manager`) when changing target so BSP and launch configs stay in sync.

### 2. Build

From the **repository root** (top-level Makefile):

```bash
make getlibs
make clean build -j8
```

This builds all three projects: `proj_cm33_s`, `proj_cm33_ns`, `proj_cm55`.

### 3. Program

```bash
make program
```

### 4. Run and observe

- CM55 UART: LVGL application output, IPC messages, and **CM33 log output** (via log transport).
- CM33: Wi‑Fi scan results and IPC counter activity are forwarded to CM55 console when using `log_queue_printf()`.

## Project layout (summary)

| Path | Purpose |
|------|--------|
| `proj_cm33_s/` | Secure boot; jumps to CM33 NS. |
| `proj_cm33_ns/` | IPC pipe, log_queue, log_ipc_transport, wifi_scan_task, RTC, LPTimer; enables CM55. |
| `proj_cm55/` | LVGL, display, touch, IPC pipe, TESA (display, LVGL, RTOS stats, event bus, datetime, logging), UI/tabview/widgets. |
| `shared/` | IPC definitions and low-level pipe setup (CM33/CM55). |
| `lib_ui_layout/` | LVGL 9.2 layout helpers (flex, grid, stack, clean containers, cards). |
| `common.mk`, `common_app.mk` | Shared make settings (target, display, tools). |
| `configs/` | Boot/signing config (e.g. `boot_with_extended_boot.json`). |

## Documentation

- [Switching target kits](./docs/target_switch.md) &ndash; How to change `TARGET` (e.g. EPC2, EPC4, AI kit) and rebuild.
- [IPC Communication](./docs/ipc_communication.md) &ndash; IPC pipe design, channels, init flow, message handling, and robustness (e.g. CM55 pipe startup check).
- [Log transport (CM33→CM55)](./docs/log.md) &ndash; log_queue, log_ipc_transport, `IPC_CMD_LOG`, and implementation notes.

---

© Infineon Technologies AG, 2025.
