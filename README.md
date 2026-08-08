# RC Controller

Custom RC transmitter/receiver system built on ESP32.

## Hardware

| Role        | MCU          |
|-------------|--------------|
| Transmitter | ESP32-S3     |
| Receiver    | ESP32-WROOM  |

## Build

Requires [ESP-IDF v5.x](https://docs.espressif.com/projects/esp-idf/en/latest/).

```bash
# Transmitter (ESP32-S3)
cd transmitter
idf.py set-target esp32s3
idf.py build

# Receiver (ESP32-WROOM)
cd receiver
idf.py set-target esp32
idf.py build
```

## Project layout

```
transmitter/   — ESP-IDF project for TX (ESP32-S3)
receiver/      — ESP-IDF project for RX (ESP32-WROOM)
components/    — shared components (rc_core, rc_protocol, radio, config)
test/          — host-side unit tests
docs/          — protocol and architecture docs
```

## Architecture

- Radio transport is fully abstracted — application code never calls ESP-NOW directly.
- RC protocol encodes/decodes to raw bytes, transport-agnostic.
- Failsafe: 500 ms without a packet → safe state.
- Arming required before any motor output.
- All calibration and configuration stored in NVS.
- Channel range: −1000 … +1000.
