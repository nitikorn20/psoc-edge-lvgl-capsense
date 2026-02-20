# SensorHub V1 Components

This folder contains UI sub-components used by `sensorhub_v1_screen.c`.

## Layout Mapping

- `dashboard/` - Dashboard tab content:
  - Text summary for all sensors
  - IPC debug log textarea
- `pot/` - POT tab:
  - Donut (arc) gauge
  - Percentage and mV details
- `bmi270/` - BMI270 tab:
  - Accel/gyro text
  - Trend chart
- `capsense_touch/` - CAPSENSE tab:
  - BTN0/BTN1 states
  - Slider bar
- `capsense_touch/` folder name is kept for compatibility with existing includes, but the rendered UI is CAPSENSE-only.
- `bmm350/` - BMM350 tab:
  - Compass needle
  - Heading + cardinal direction
  - Magnetic field details

## Design Rule

Each component owns only its tab UI and render logic.  
IPC polling/state collection remains in `sensorhub_v1_state.*`.
