---
description: Build and flash the PSoC8 project to hardware
---

To build all cores (CM33_S, CM33_NS, CM55) and program the flash, run:

// turbo
1. Execute the build and program command (PowerShell syntax for AI Agent):
```powershell
$env:PATH = "C:\Users\drsanti\ModusToolbox\tools_3.6\modus-shell\bin;" + $env:PATH; $env:CY_COMPILER_LLVM_ARM_DIR = "d:/dev/LLVM/LLVM-ET-Arm-19.1.5-Windows-x86_64"; make program
```

2. Verify that the output ends with `verified X bytes` and `Exit code: 0`.
