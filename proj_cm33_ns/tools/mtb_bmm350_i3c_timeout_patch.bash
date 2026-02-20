#!/usr/bin/env bash
#
# Patch Infineon's mtb_bmm350.c to avoid infinite busy-wait loops
# in the I3C read/write helpers. Without a timeout, a bus error can
# hang the calling task forever (observed as "init prints once, then
# no more heartbeat/update").
#
# This patch is intentionally:
# - Idempotent (safe to run every build)
# - Minimal (only touches the wait loops + adds a timeout macro)
#
set -euo pipefail

target_file="${1:-}"

if [[ -z "${target_file}" ]]; then
  echo "Usage: $0 <path-to-mtb_bmm350.c>" >&2
  exit 2
fi

if [[ ! -f "${target_file}" ]]; then
  echo "Error: target file not found: ${target_file}" >&2
  exit 1
fi

python - <<'PY' "${target_file}"
from __future__ import annotations

import pathlib
import re
import sys

path = pathlib.Path(sys.argv[1])
text = path.read_text(encoding="utf-8", errors="replace")

if "MTB_BMM350_I3C_XFER_TIMEOUT_US" not in text:
    text = text.replace(
        "#define _I3C_CMD_LENGTH            (1u)",
        "#define _I3C_CMD_LENGTH            (1u)\n"
        "\n"
        "/*\n"
        " * Patch: prevent infinite spin in I3C xfer completion waits.\n"
        " * If the bus status never reaches the expected value, return COM_FAIL\n"
        " * instead of hanging forever.\n"
        " */\n"
        "#define MTB_BMM350_I3C_XFER_TIMEOUT_US      (5000u)\n",
        1,
    )

def patch_wait_loop(kind: str, expected: str) -> None:
    global text
    # Match the original busy-wait loop (no timeout).
    pat = re.compile(
        r"while\s*\(\s*"
        + re.escape(expected)
        + r"\s*!=\s*\n\s*Cy_I3C_GetBusStatus\(_bmm350_i3c_hw,\s*_bmm350_i3c_context\)\s*\)\s*\n"
        r"\s*\{\s*\n\s*/\*\s*Wait\s+for\s+"
        + re.escape(kind)
        + r"\s+to\s+complete\s*\*/\s*\n\s*\}",
        re.MULTILINE,
    )

    repl = (
        "    uint32_t timeout_us = MTB_BMM350_I3C_XFER_TIMEOUT_US;\n\n"
        "    while ("
        + expected
        + " !=\n"
        "           Cy_I3C_GetBusStatus(_bmm350_i3c_hw, _bmm350_i3c_context))\n"
        "    {\n"
        "        if (timeout_us == 0U)\n"
        "        {\n"
        "            Cy_I3C_Resume(_bmm350_i3c_hw, _bmm350_i3c_context);\n"
        "            return BMM350_E_COM_FAIL;\n"
        "        }\n\n"
        "        timeout_us--;\n"
        "        Cy_SysLib_DelayUs(1U);\n"
        "    }"
    )

    text, n = pat.subn(repl, text, count=1)
    if n == 0:
        raise SystemExit(
            f"Failed to patch {kind} wait loop in {path} (pattern not found)."
        )


if "MTB_BMM350_I3C_XFER_TIMEOUT_US" in text and "timeout_us = MTB_BMM350_I3C_XFER_TIMEOUT_US" not in text:
    patch_wait_loop("read", "CY_I3C_CONTROLLER_I3C_SDR_RD_XFER")
    patch_wait_loop("write", "CY_I3C_CONTROLLER_I3C_SDR_WR_XFER")

path.write_text(text, encoding="utf-8")
PY
