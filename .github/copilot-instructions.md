# RC Controller — Copilot Instructions

## Project

Custom RC transmitter/receiver system on ESP32 (ESP-IDF v5.x, C11). TX = ESP32-S3, RX = ESP32-WROOM.

## Architecture Invariants

- Radio abstraction layer — never call ESP-NOW API from application code
- Protocol is just bytes — encode/decode without knowing the transport
- Failsafe always active — no packet for 500ms = safe state
- Arming required before motor output
- NVS for all configuration — no hardcoded calibration values
- Channel range: -1000 to +1000 (normalized)

## Style

- ESP-IDF conventions: `esp_err_t`, `ESP_LOG*`, FreeRTOS tasks/queues
- One `TAG` per file: `static const char *TAG = "module";`
- Components self-contained with `CMakeLists.txt` + `include/` + `src/`
- Unit tests for protocol/core run on host (no ESP32 needed)
- Polish language in docs and comments where natural; English for code identifiers and API
