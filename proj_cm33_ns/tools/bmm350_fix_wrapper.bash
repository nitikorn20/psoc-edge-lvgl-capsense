#!/usr/bin/env bash
#
# Wrapper for Infineon's bmm350_fix.bash.
# Ensures the Bosch SensorAPI file has a trailing newline so that the
# "while read" loop inside the original script does not drop the last line.
#
set -euo pipefail

fix_script="${1:-}"
target_file="${2:-}"

if [[ -z "${fix_script}" || -z "${target_file}" ]]; then
  echo "Usage: $0 <path-to-bmm350_fix.bash> <path-to-bmm350.c>" >&2
  exit 2
fi

if [[ ! -f "${fix_script}" ]]; then
  echo "Error: fix script not found: ${fix_script}" >&2
  exit 1
fi

if [[ ! -f "${target_file}" ]]; then
  echo "Error: target file not found: ${target_file}" >&2
  exit 1
fi

if [[ -s "${target_file}" ]]; then
  last_byte="$(tail -c 1 "${target_file}" || true)"
  if [[ "${last_byte}" != $'\n' ]]; then
    printf '\n' >> "${target_file}"
  fi
fi

bash "${fix_script}" "${target_file}"

ifdef_line="$(grep -nE '^[[:space:]]*#ifdef[[:space:]]+BMM350_USE_FIXED_POINT([[:space:]]|$)' "${target_file}" | tail -n 1 | cut -d: -f1 || true)"
if [[ -n "${ifdef_line}" ]]; then
  endif_line="$(grep -nE '^[[:space:]]*#endif([[:space:]]|$)' "${target_file}" | tail -n 1 | cut -d: -f1 || true)"
  if [[ -z "${endif_line}" || "${endif_line}" -lt "${ifdef_line}" ]]; then
    printf '\n#endif\n' >> "${target_file}"
  fi
fi

