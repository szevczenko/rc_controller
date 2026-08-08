# Implementation Plan — RC Controller

Plan implementacji podzielony na fazy. Każdy krok zawiera konkretne pliki do stworzenia, API do zaimplementowania i kryteria akceptacji.

---

## Phase 0 — Project Skeleton (MR-01, MR-02)

### MR-01: Project skeleton

**Cel:** `idf.py build` przechodzi dla obu targetów.

**Pliki do stworzenia:**

```
transmitter/
├── CMakeLists.txt
├── main/
│   ├── CMakeLists.txt
│   └── main.c              # app_main() — loguje "TX starting"
├── sdkconfig.defaults
└── partitions.csv          # nvs, otadata, phy_init, ota_0, ota_1

receiver/
├── CMakeLists.txt
├── main/
│   ├── CMakeLists.txt
│   └── main.c              # app_main() — loguje "RX starting"
├── sdkconfig.defaults
└── partitions.csv

components/
├── rc_core/
│   ├── CMakeLists.txt
│   ├── include/rc_core.h
│   └── src/rc_core.c
├── rc_protocol/
│   ├── CMakeLists.txt
│   ├── include/rc_protocol.h
│   └── src/rc_protocol.c
├── radio/
│   ├── CMakeLists.txt
│   ├── include/radio.h
│   └── src/radio.c
└── config/
    ├── CMakeLists.txt
    ├── include/config.h
    └── src/config.c

.gitignore
README.md
```

**sdkconfig.defaults (transmitter):**
```
CONFIG_IDF_TARGET="esp32s3"
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"
```

**sdkconfig.defaults (receiver):**
```
CONFIG_IDF_TARGET="esp32"
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"
```

**partitions.csv:**
```
# Name,   Type, SubType, Offset,  Size
nvs,      data, nvs,     0x9000,  0x6000
otadata,  data, ota,     0xf000,  0x2000
phy_init, data, phy,     0x11000, 0x1000
ota_0,    app,  ota_0,   0x20000, 0x1C0000
ota_1,    app,  ota_1,   0x1E0000,0x1C0000
```

**Definition of Done:**
```bash
cd transmitter && idf.py set-target esp32s3 && idf.py build
cd receiver && idf.py set-target esp32 && idf.py build
```

---

### MR-02: CI/build matrix

**Pliki:**
```
.gitlab-ci.yml  (lub .github/workflows/build.yml)
```

**Matryca:**
- transmitter: ESP32-S3
- receiver: ESP32, ESP32-S3, ESP32-C6

---

## Phase 1 — Hardware Input (MR-03 → MR-07)

### MR-03: ADC abstraction

**Plik:** `components/input_adc/`

**API:**
```c
// include/input_adc.h
typedef struct {
    adc_unit_t unit;
    adc_channel_t channel;
    adc_atten_t atten;
} input_adc_config_t;

esp_err_t input_adc_init(const input_adc_config_t *config, size_t count);
uint16_t input_adc_read_raw(uint8_t index);
```

**DoD:** serial monitor pokazuje surowe wartości ADC.

---

### MR-04: Single gimbal

**Rozszerzenie** `input_adc` — konfiguracja jednego kanału.

**DoD:** gimbal BetaFPV podłączony, wartości 0–4095 w serial monitor.

---

### MR-05: Two gimbals

**API rozszerzenie:**
```c
typedef struct {
    int16_t throttle;  // raw ADC
    int16_t steering;  // raw ADC
} input_gimbal_raw_t;

esp_err_t input_gimbal_read(input_gimbal_raw_t *out);
```

**DoD:** oba gimbale raportują niezależne wartości.

---

### MR-06: Calibration

**Plik:** `components/input_adc/src/calibration.c`

**API:**
```c
typedef struct {
    uint16_t min;
    uint16_t center;
    uint16_t max;
} input_calibration_t;

esp_err_t input_calibration_load(uint8_t channel, input_calibration_t *cal);
esp_err_t input_calibration_save(uint8_t channel, const input_calibration_t *cal);
```

**DoD:** wartości min/center/max zapisane w NVS, przeżywają restart.

---

### MR-07: Switches

**Plik:** `components/input_switch/`

**API:**
```c
esp_err_t input_switch_init(const gpio_num_t *pins, size_t count);
bool input_switch_get(uint8_t index);
```

**DoD:** SW1/SW2 raportują stan ON/OFF.

---

## Phase 2 — RC Core (MR-08 → MR-11)

### MR-08: Channel model

**Plik:** `components/rc_core/include/rc_core.h`

**API:**
```c
#define RC_MAX_CHANNELS 16

typedef struct {
    int16_t channels[RC_MAX_CHANNELS];  // -1000 ... +1000
    uint8_t channel_count;
} rc_channel_state_t;
```

---

### MR-09: Channel normalization

**API:**
```c
// Converts raw ADC (0–4095) to normalized (-1000...+1000) using calibration
int16_t rc_core_normalize(uint16_t raw, const input_calibration_t *cal);
```

---

### MR-10: Deadzone / center

**API:**
```c
typedef struct {
    int16_t deadzone;   // e.g. 50 = ±5%
    int16_t center;     // calibrated center offset
} rc_core_channel_config_t;

int16_t rc_core_apply_deadzone(int16_t value, const rc_core_channel_config_t *cfg);
```

---

### MR-10.5: Expo / rates

**API:**
```c
// expo_factor: 0 = linear, 100 = max expo
int16_t rc_core_apply_expo(int16_t value, uint8_t expo_factor);
```

---

### MR-11: Failsafe model

**API:**
```c
typedef enum {
    RC_STATE_NORMAL,
    RC_STATE_TIMEOUT,
    RC_STATE_FAILSAFE,
} rc_link_state_t;

typedef struct {
    int16_t failsafe_values[RC_MAX_CHANNELS];
    uint32_t timeout_ms;  // default 500
} rc_failsafe_config_t;

rc_link_state_t rc_core_get_link_state(void);
void rc_core_packet_received(void);  // resets timeout
void rc_core_tick(uint32_t now_ms);  // checks timeout
```

---

## Phase 3 — Protocol (MR-12 → MR-15)

### MR-12: Packet format

**Plik:** `docs/protocol-v1.md` + `components/rc_protocol/include/rc_protocol.h`

**Struktura pakietu:**
```c
typedef enum {
    RC_PACKET_TYPE_RC       = 0x01,
    RC_PACKET_TYPE_TELEMETRY = 0x02,
    RC_PACKET_TYPE_BIND     = 0x03,
    RC_PACKET_TYPE_OTA      = 0x04,
} rc_packet_type_t;

typedef struct {
    uint8_t version;
    uint8_t type;
    uint16_t sequence;
    uint8_t channel_count;
    int16_t channels[RC_MAX_CHANNELS];
} rc_packet_t;

#define RC_PROTOCOL_VERSION 1
```

---

### MR-13: Encoder

**API:**
```c
// Returns encoded length, or negative on error
int rc_packet_encode(const rc_packet_t *packet, uint8_t *buffer, size_t buffer_size);
```

**Test:** unit test na PC (bez ESP32).

---

### MR-14: Decoder

**API:**
```c
// Returns ESP_OK on success, ESP_ERR_INVALID_CRC on CRC failure
esp_err_t rc_packet_decode(const uint8_t *buffer, size_t len, rc_packet_t *packet);
```

**Test:** encode → decode → memcmp.

---

### MR-15: CRC + sequence

**Logika:**
```c
// Sequence validation
typedef enum {
    RC_SEQ_OK,           // correct next sequence
    RC_SEQ_GAP,          // future sequence (packet loss detected)
    RC_SEQ_REPLAY,       // past sequence (rejected)
} rc_seq_result_t;

rc_seq_result_t rc_protocol_check_sequence(uint16_t received, uint16_t *last_seq);
```

CRC: CRC-16/CCITT appended na końcu pakietu.

---

## Phase 4 — Radio & ESP-NOW (MR-16 → MR-20)

### MR-16: Radio abstraction

**Plik:** `components/radio/include/radio.h`

**API:**
```c
typedef void (*radio_rx_callback_t)(const uint8_t *data, size_t len, int rssi);

typedef struct {
    esp_err_t (*init)(radio_rx_callback_t rx_cb);
    esp_err_t (*send)(const uint8_t *data, size_t len);
    esp_err_t (*set_channel)(uint8_t channel);
    int (*get_rssi)(void);
    const char *name;
} radio_driver_t;

esp_err_t radio_init(const radio_driver_t *driver, radio_rx_callback_t rx_cb);
esp_err_t radio_send(const uint8_t *data, size_t len);
```

**Plik:** `components/radio/src/radio_loopback.c` — mock do testów.

---

### MR-17: ESP-NOW driver

**Plik:** `components/radio_espnow/`

**Implementacja:** `radio_driver_t` z użyciem `esp_now_send()` / `esp_now_register_recv_cb()`.

---

### MR-17.5: Binding / pairing

**Plik:** `components/radio_espnow/src/binding.c`

**API:**
```c
esp_err_t radio_espnow_start_bind(uint32_t timeout_ms);
esp_err_t radio_espnow_wait_bind(uint32_t timeout_ms);
bool radio_espnow_is_bound(void);
```

**Przechowywanie:** peer MAC w NVS namespace `radio`.

---

### MR-18: RC packet over ESP-NOW

**Integracja:** TX main loop:
```c
while (1) {
    input_gimbal_read(&raw);
    rc_core_process(&raw, &state);
    rc_packet_encode(&packet, buffer, sizeof(buffer));
    radio_send(buffer, len);
    vTaskDelay(pdMS_TO_TICKS(20));  // 50 Hz
}
```

---

### MR-19: Link statistics

**API:**
```c
typedef struct {
    int8_t rssi;
    uint32_t packets_tx;
    uint32_t packets_rx;
    uint8_t packet_loss_percent;
    uint16_t last_sequence;
} rc_link_stats_t;

void rc_link_stats_get(rc_link_stats_t *stats);
```

---

### MR-20: Bidirectional telemetry

**Telemetry packet (RX → TX):**
```c
typedef struct {
    uint8_t version;
    uint8_t type;  // RC_PACKET_TYPE_TELEMETRY
    int8_t rssi;
    uint8_t link_quality;
    uint16_t battery_mv;
    uint32_t uptime_ms;
} rc_telemetry_packet_t;
```

---

## Phase 5 — Car (MR-21 → MR-25)

### MR-21: PWM output

**Plik:** `components/output_pwm/`

**API:**
```c
typedef struct {
    gpio_num_t pin;
    uint32_t freq_hz;
    uint16_t min_us;
    uint16_t max_us;
} output_pwm_config_t;

esp_err_t output_pwm_init(const output_pwm_config_t *config, size_t count);
esp_err_t output_pwm_set(uint8_t index, int16_t value);  // -1000...+1000
```

---

### MR-22: Servo steering

Konfiguracja: 1000–2000 µs, 50 Hz, CH2 → servo.

---

### MR-23: ESC throttle

Konfiguracja: 1000–2000 µs, ESC protocol (50 Hz standard PWM), CH1 → ESC.

---

### MR-23.5: Arming logic

**API:**
```c
typedef struct {
    bool armed;
    bool arm_switch_on;
    bool throttle_at_neutral;
} rc_arm_state_t;

bool rc_arm_check(const rc_channel_state_t *channels, uint8_t arm_switch_channel);
```

**Logika:** motor output = 0 dopóki `!armed`.

---

### MR-24: RX failsafe

**Integracja pełna:**
```
radio timeout → failsafe state → ESC=0, servo=center, disarm
```

---

### MR-25: End-to-end car

**Test integracyjny:** TX z gimbalami steruje samochodem RC. Failsafe działa po wyłączeniu TX.

**To jest v0.1.0.**

---

## Phase 6 — Display (MR-26 → MR-29)

### MR-26: ST7796 driver

**Plik:** `components/display/`

Użycie ESP LCD Panel API (`esp_lcd_panel_io_spi` + `esp_lcd_new_panel_st7796`).

---

### MR-27: Basic graphics

Prymitywy: tekst, linie, prostokąty. Framebuffer lub direct draw.

---

### MR-28: RC dashboard

Wyświetlanie: THR, STR, SW1, SW2, RSSI, LINK.

---

### MR-29: Telemetry dashboard

Wyświetlanie danych telemetrycznych z RX.

---

## Phase 7 — Configuration (MR-30 → MR-32)

### MR-30: Configuration component

**API:**
```c
esp_err_t config_load(const char *namespace, const char *key, void *out, size_t size);
esp_err_t config_save(const char *namespace, const char *key, const void *data, size_t size);
esp_err_t config_reset(const char *namespace);
```

---

### MR-31: Model configuration

Struktura modelu: name, channel mapping, calibration, failsafe, radio settings.

---

### MR-32: Model selection

UI na ekranie: wybór modelu z NVS.

---

## Phase 8 — OTA (MR-33 → MR-37)

### MR-33: OTA partition infrastructure

Już zrobione w MR-01 (partitions.csv).

---

### MR-34: Local OTA (Wi-Fi)

Test mechanizmu `esp_ota_begin()` / `esp_ota_write()` / `esp_ota_end()` przez HTTP.

---

### MR-35: OTA protocol

Nowe typy pakietów w `rc_protocol`.

---

### MR-36: OTA over ESP-NOW

Transfer firmware w chunkach przez radio.

---

### MR-37: OTA UI

Progress bar na TFT.

---

## Phase 9 — Alternative Radio (MR-38 → MR-39)

### MR-38: nRF24L01+ driver

Implementacja `radio_driver_t` dla nRF24. Zero zmian w RC Core / Protocol / UI.

---

### MR-39: SX1280 driver

Implementacja `radio_driver_t` dla SX1280. Testy porównawcze latency/range/loss.

---

## Kluczowe zasady implementacji

1. **Żadnych hardcoded wartości** — wszystko przez NVS/config
2. **Protokół niezależny od radia** — encode/decode testowalne na PC
3. **Failsafe zawsze aktywny** — brak pakietów = safe state
4. **Arming wymagany** — throttle nie działa bez explicit arm
5. **Komponenty ESP-IDF** — każdy moduł jako osobny component z CMakeLists.txt
6. **RTOS tasks** — input task (high priority), radio task, display task (low priority)
7. **Nie optymalizuj przedwcześnie** — 50 Hz wystarczy na v0.1
