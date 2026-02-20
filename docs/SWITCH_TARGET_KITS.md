# Switching Target Kits (TARGET)

This project supports multiple PSoC Edge E84 target kits. Use the steps below to switch from one kit to another.

## Supported targets

| TARGET value | Kit |
|--------------|-----|
| `APP_KIT_PSE84_EVAL_EPC2` | PSoC Edge E84 Evaluation Kit (EPC2) – default / tested |
| `APP_KIT_PSE84_EVAL_EPC4` | PSoC Edge E84 Evaluation Kit (EPC4) |
| `APP_KIT_PSE84_AI` | PSoC Edge E84 AI Kit |

## Git Bash: make in PATH

If you use Git Bash and get `make: command not found`, add ModusToolbox’s make to your PATH. ModusToolbox installs GNU make under `tools_3.x\modus-shell\bin` (e.g. `C:\Users\drsanti\ModusToolbox\tools_3.6\modus-shell\bin`).

1. **Find the make bin path**
   Under `%USERPROFILE%\ModusToolbox\tools_3.x`, open `modus-shell\bin`; that directory contains `make.exe`. Or run in Command Prompt: `dir /s /b "%USERPROFILE%\ModusToolbox\make.exe"` and use the `bin` directory that contains it.

2. **Add to Git Bash**
   Convert the path to Git Bash form (e.g. `C:\Users\drsanti\ModusToolbox\tools_3.6\modus-shell\bin` → `/c/Users/drsanti/ModusToolbox/tools_3.6/modus-shell/bin`). Edit `~/.bashrc` (or `~/.bash_profile`) and add:
   ```bash
   export PATH="/c/Users/drsanti/ModusToolbox/tools_3.6/modus-shell/bin:$PATH"
   ```
   Replace the path with your actual `modus-shell\bin` directory if different. Save, then run `source ~/.bashrc` or restart Git Bash.

3. **Verify**
   From the repo root: `make --version` and `make getlibs`.

See [BUILD_SETUP.md](BUILD_SETUP.md) for more detail.

## How to switch

### 1. Set TARGET in common.mk

Edit **`common.mk`** at the repository root and set `TARGET` to the desired kit:

```makefile
TARGET=APP_KIT_PSE84_EVAL_EPC2
```

Change to one of:

- `TARGET=APP_KIT_PSE84_EVAL_EPC4`
- `TARGET=APP_KIT_PSE84_AI`

All three projects (`proj_cm33_s`, `proj_cm33_ns`, `proj_cm55`) include this file, so this single change applies to every project.

### 2. Run Library Manager

From the repository root, run:

```bash
make library-manager
```

This resolves and installs the correct BSP for the new target and keeps launch configurations in sync.

### 3. Rebuild

```bash
make getlibs
make clean build -j8
```

Then program as usual:

```bash
make program
```

## Kit-specific behavior

- **EPC2 / EPC4**: Only the BSP and target name differ; no extra build defines.
- **AI Kit** (`APP_KIT_PSE84_AI`): The CM55 build adds the define `USE_KIT_PSE84_AI`, which is used in `proj_cm55/modules/lvgl_display/core/display_i2c_config.h` to select the AI kit display I2C controller. No manual code changes are required when switching to the AI kit.
