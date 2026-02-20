# IPC Log Module – User Manual

**Author:** Asst. Prof. Santi Nuratch, Ph.D  
**Organization:** Thailand Embedded Systems Association (TESA)

---

## 1. Overview

The IPC log module provides non-blocking logging from the PSoC Edge **Cortex-M33** (non-secure) core to the **Cortex-M55** core over the IPC pipe. Log strings are formatted with standard `printf`-style arguments, queued in a FreeRTOS queue, and sent by a dedicated worker task. When the module is enabled, `printf` can be redirected via a macro so that existing `printf` calls go to the IPC log with no code changes. The module can be disabled at compile time with zero overhead.

---

## 2. Features

- **Non-blocking** – `ipc_log_printf` (and redirected `printf`) enqueue messages; the caller does not wait for IPC transmission.
- **Dedicated worker task** – A FreeRTOS task in the transport layer drains the queue and sends messages to CM55 using the project’s IPC pipe and command type (e.g. `IPC_CMD_LOG`).
- **Transparent redirect** – Including `ipc_log.h` redefines `printf` to `ipc_log_printf`, so existing `printf` calls are sent over IPC when the module is enabled.
- **Optional** – Define `DISABLE_IPC_LOGGING` to remove the worker, queue, and IPC traffic; `ipc_log_printf` and `ipc_log_init` become no-ops.

---

## 3. Dependencies

- **FreeRTOS** – Queue and task for the log queue and transport worker.
- **stdio** – `vsnprintf` for formatting (requires retarget-io or equivalent to be initialized for the C library I/O layer).
- **IPC pipe** – Infineon IPC pipe middleware; CM33 side must be able to send to CM55. CM55 must handle the log command (e.g. `IPC_CMD_LOG`) and output the received string.

---

## 4. Integration

### 4.1 Makefile

Paths below are relative to the CM33 project directory (the directory containing the Makefile).

- **INCLUDES** – Add the module directory so the compiler finds `ipc_log.h` and `ipc_log_transport.h`:
  ```makefile
  INCLUDES += modules/ipc_log
  ```
- **SOURCES** – Many build systems (e.g. ModusToolbox) auto-discover `.c` files under the project tree. If yours does not, add both implementation files explicitly:
  ```makefile
  SOURCES += modules/ipc_log/ipc_log.c modules/ipc_log/ipc_log_transport.c
  ```
- **Optional – disable IPC logging** (e.g. for production): add a define so the module compiles as no-ops with zero overhead:
  ```makefile
  DEFINES += DISABLE_IPC_LOGGING
  ```

### 4.2 Initialization

Initialize the C library I/O (e.g. retarget-io) first, then the IPC log transport. The transport initializes the log queue and worker and starts the sender task.

```c
#include "retarget_io_init.h"
#include "ipc_log_transport.h"

int main(void) {
  init_retarget_io();
  if (!ipc_log_transport_init()) {
    handle_error(NULL);
  }
  vTaskStartScheduler();
}
```

### 4.3 Logging

Use `printf` as usual in any file that includes `ipc_log.h`. When IPC logging is enabled, those calls are redirected to the IPC queue.

```c
#include "ipc_log.h"

void my_function(void) {
  printf("Temperature is %d degC\n", temp);
}
```

Without including `ipc_log.h`, `printf` remains the standard implementation (e.g. UART if retarget-io is used).

---

## 5. API Reference

### 5.1 Lifecycle (when not disabled)

| Function | Description |
|----------|-------------|
| `ipc_log_init()` | Creates the log queue. Called internally by `ipc_log_transport_init()`; typically the application only calls the transport init. Returns true on success. |
| `ipc_log_transport_init()` | Initializes the transport (creates queue via `ipc_log_init()` and starts the worker task). Call once after `init_retarget_io()`. Returns true on success. |

### 5.2 Logging

| Function | Description |
|----------|-------------|
| `ipc_log_printf(format, ...)` | Formats with `vsnprintf` into a queue message and enqueues it. When `DISABLE_IPC_LOGGING` is defined, this is a no-op stub. |
| `printf(...)` | When `ipc_log.h` is included and IPC logging is enabled, expanded to `ipc_log_printf(...)`. |

---

## 6. Types and Constants

### 6.1 log_msg_t (in ipc_log.h)

| Field | Type | Description |
|-------|------|-------------|
| message | char[LOG_MESSAGE_SIZE] | Formatted log string (null-terminated within the buffer). |

### 6.2 Constants (in ipc_log.h)

| Name | Value | Description |
|------|-------|-------------|
| LOG_QUEUE_LENGTH | 16 | Number of log messages the queue can hold. |
| LOG_MESSAGE_SIZE | 128 | Maximum length of one log message (including null terminator). |

When the queue is full, `ipc_log_printf` blocks until space is available. The worker task sends messages to CM55 using the project’s IPC command (e.g. `IPC_CMD_LOG`).

---

## 7. Usage Examples

**Basic logging after transport init:**

```c
#include "ipc_log.h"
printf("System started\n");
printf("Value: %d\n", value);
```

**Explicit ipc_log_printf (same effect if printf is redirected):**

```c
#include "ipc_log.h"
ipc_log_printf("Error code: 0x%02X\n", err);
```

**Conditional compile:**

```c
#ifdef DISABLE_IPC_LOGGING
#define LOG_INFO(...) ((void)0)
#else
#define LOG_INFO(...) printf(__VA_ARGS__)
#endif
```

---

## 8. Limits and Notes

- **retarget-io required** – `vsnprintf` depends on the C library I/O layer. Call `init_retarget_io()` (or equivalent) before `ipc_log_transport_init()`. Logs are sent over IPC; physical UART is not required for the log content, but the library must be initialized.
- **Pre-scheduler logs** – If large amounts of text are printed before `vTaskStartScheduler()`, the queue can fill because the worker task is not yet running; the caller blocks until space is available.
- **Queue full** – When the queue is full, `ipc_log_printf` / `printf` blocks (portMAX_DELAY) until space becomes available.
- **CM55 side** – The CM55 application must handle the log IPC command (e.g. `IPC_CMD_LOG`) and output the received string (e.g. to console or display).
- **Disable for release** – Define `DISABLE_IPC_LOGGING` in the build to remove the queue, worker task, and all IPC log traffic with zero runtime cost.
