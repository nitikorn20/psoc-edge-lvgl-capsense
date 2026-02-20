# Tools (CM33 NS)

This folder contains build-time helper scripts for BMM350 integration.

## Files

- `bmm350_fix_wrapper.bash`
  - Wrapper around vendor `bmm350_fix.bash`.
  - Ensures end-of-file/newline edge-cases are handled before running the fix script.

- `mtb_bmm350_i3c_timeout_patch.bash`
  - Patches `mtb_bmm350.c` I3C wait loops to add timeout protection.
  - Prevents infinite busy-wait lockups if I3C bus status does not progress.

## How it is used

`proj_cm33_ns/Makefile` runs both scripts in `PREBUILD` to patch shared middleware before compile.

## Important

- Scripts are expected to use LF line endings.
- If environment cannot run `bash`, prebuild can fail.
- When vendor library is officially fixed, these scripts can be removed.