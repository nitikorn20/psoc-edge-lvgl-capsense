---
description: Build all cores of the PSoC8 project
---

To build the project (CM33_S, CM33_NS, CM55), run:

// turbo
1. Execute the build command (PowerShell syntax for AI Agent):
```powershell
$env:PATH = "C:\Users\drsanti\ModusToolbox\tools_3.6\modus-shell\bin;" + $env:PATH; $env:CY_COMPILER_LLVM_ARM_DIR = "d:/dev/LLVM/LLVM-ET-Arm-19.1.5-Windows-x86_64"; make build -j8
```

2. Verify that the output ends with `Exit code: 0`.
