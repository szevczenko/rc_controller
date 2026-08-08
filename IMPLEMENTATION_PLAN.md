# Implementation Plan — RC Controller

Plan implementacji podzielony na fazy. Każdy krok zawiera konkretne pliki do stworzenia, API do zaimplementowania i kryteria akceptacji.

---

## Phase 0 — Project Skeleton (MR-01, MR-02)

### MR-01: Project skeleton ✅

**Cel:** `idf.py build` przechodzi dla obu targetów.

**Struktura:**
```
transmitter/   ← ESP-IDF project (ESP32-S3)
receiver/      ← ESP-IDF project (ESP32-WROOM)
components/    ← shared via EXTRA_COMPONENT_DIRS
test/
docs/
partitions/
```

**partitions.csv** (obydwa projekty):
```
nvs,      data, nvs,     0x9000,  0x6000
otadata,  data, ota,     0xf000,  0x2000
phy_init, data, phy,     0x11000, 0x1000
ota_0,    app,  ota_0,   0x20000, 0x1C0000
ota_1,    app,  ota_1,   0x1E0000,0x1C0000
```

**Definition of Done:**
```bash
cd transmitter && idf.py set-target esp32s3 && idf.py build
cd receiver    && idf.py set-target esp32   && idf.py build
```

---

### MR-02: CI/build matrix ✅

**Plik:** `.github/workflows/build.yml`

Matryca: transmitter (esp32s3) + receiver (esp32, esp32s3, esp32c6). Kompilacja tylko, bez flashowania.

---

## Phase 1 — Hardware Input (MR-03 → MR-07)

### MR-03: ADC abstraction ✅

**Plik:** `components/input_adc/`

**API:**
```c
esp_err_t input_adc_init(const input_adc_config_t *config, size_t count);
uint16_t  input_adc_read_raw(uint8_t index);
```

**DoD:** serial monitor pokazuje surowe wartości ADC.

---

### MR-04: Single gimbal ✅

Konfiguracja jednego kanału ADC dla gimbala BetaFPV.

---

### MR-05: Two gimbals ✅

```c
typedef struct {
    int16_t throttle;
    int16_t steering;
} input_gimbal_raw_t;
```

---

### MR-06: Calibration ✅

**API:**
```c
esp_err_t input_calibration_load(uint8_t channel, input_calibration_t *cal);
esp_err_t input_calibration_save(uint8_t channel, const input_calibration_t *cal);
```

Namespace NVS: `input_cal`. **DoD:** wartości przeżywają restart.

---

### MR-07: Switches ✅

**Plik:** `components/input_switch/`

```c
esp_err_t input_switch_init(const gpio_num_t *pins, size_t count);
bool      input_switch_get(uint8_t index);
```

---

## Phase 2 — RC Core (MR-08 → MR-11)

### MR-08: Channel model ✅

```c
#define RC_MAX_CHANNELS 16

typedef struct {
    int16_t channels[RC_MAX_CHANNELS];
    uint8_t channel_count;
} rc_channel_state_t;
```

---

### MR-09: Channel normalization ✅

```c
int16_t rc_core_normalize(uint16_t raw, const rc_calibration_t *cal);
```

Zakres: -1000 … +1000.

---

### MR-10: Deadzone / center ✅

```c
typedef struct { int16_t deadzone; } rc_core_channel_config_t;

int16_t rc_core_apply_deadzone(int16_t value, const rc_core_channel_config_t *cfg);
```

---

### MR-10.5: Expo / rates ✅

```c
int16_t rc_core_apply_expo(int16_t value, uint8_t expo_factor);
```

`expo_factor`: 0 = liniowy, 100 = max expo. Wywoływany w TX main po deadzone.

---

### MR-11: Failsafe model ✅

```c
void rc_core_init(const rc_failsafe_config_t *config);
void rc_core_packet_received(uint32_t now_ms);
void rc_core_tick(uint32_t now_ms);
rc_link_state_t rc_core_get_link_state(void);
```

Timeout: 500 ms. Stany: `RC_STATE_NORMAL → RC_STATE_TIMEOUT → RC_STATE_FAILSAFE`.

---

## Phase 3 — Protocol (MR-12 → MR-15)

### MR-12: Packet format ✅

Specyfikacja: `docs/protocol-v1.md`.

```c
typedef struct {
    uint8_t  version;
    uint8_t  type;
    uint16_t sequence;
    uint8_t  channel_count;
    int16_t  channels[RC_MAX_CHANNELS];
} rc_packet_t;
```

---

### MR-13: Encoder ✅

```c
int rc_packet_encode(const rc_packet_t *packet, uint8_t *buf, size_t buf_size);
```

**Test:** unit test na PC (bez ESP32).

---

### MR-14: Decoder ✅

```c
esp_err_t rc_packet_decode(const uint8_t *buf, size_t len, rc_packet_t *packet);
```

**Test:** encode → decode → memcmp.

---

### MR-15: CRC + sequence ✅

CRC-16/CCITT-FALSE (poly `0x1021`, init `0xFFFF`).

```c
rc_seq_result_t rc_protocol_check_sequence(uint16_t received, uint16_t *last_seq);
```

Wyniki: `RC_SEQ_OK`, `RC_SEQ_GAP` (packet loss), `RC_SEQ_REPLAY` (rejected). Rollover obsługiwany arytmetyką modularną.

---

## Phase 4 — Radio & ESP-NOW (MR-16 → MR-20)

### MR-16: Radio abstraction ✅

```c
typedef struct {
    esp_err_t (*init)(radio_rx_callback_t rx_cb);
    esp_err_t (*send)(const uint8_t *data, size_t len);
    esp_err_t (*set_channel)(uint8_t channel);
    int       (*get_rssi)(void);
    const char *name;
} radio_driver_t;

esp_err_t radio_init(const radio_driver_t *driver, radio_rx_callback_t rx_cb);
esp_err_t radio_send(const uint8_t *data, size_t len);
```

Loopback driver: `radio_loopback_get_driver()`.

---

### MR-17: ESP-NOW driver ✅

```c
const radio_driver_t *radio_espnow_get_driver(void);
```

Implementacja `radio_driver_t` używając `esp_now_send()` i `esp_now_register_recv_cb()`.

---

### MR-17.5: Binding / pairing ✅

```c
esp_err_t radio_espnow_start_bind(uint32_t timeout_ms);
esp_err_t radio_espnow_wait_bind(uint32_t timeout_ms);
bool      radio_espnow_is_bound(void);
esp_err_t radio_espnow_save_peer(const uint8_t peer_mac[6]);
esp_err_t radio_espnow_load_peer(uint8_t peer_mac[6]);
```

Peer MAC przechowywany w NVS namespace `radio`.

---

### MR-18: RC packet over ESP-NOW ✅

TX main loop (50 Hz):
```
input_adc → normalize → deadzone → expo → rc_packet_encode → radio_send
```

RX radio_rx_cb:
```
radio_rx_cb → rc_packet_decode → rc_core_packet_received → output_pwm_set
```

---

### MR-19: Link statistics ✅

```c
typedef struct {
    int8_t   rssi;
    uint32_t packets_rx;
    uint32_t packets_lost;
    uint8_t  link_quality;
    uint16_t last_sequence;
} rc_link_stats_t;

void rc_link_stats_update(rc_link_stats_t *stats, int rssi, bool packet_lost, uint16_t sequence);
```

---

### MR-20: Bidirectional telemetry ✅

RX → TX: `rc_telemetry_packet_t` co 100 ms (10 Hz):
```c
int       rc_telemetry_encode(const rc_telemetry_packet_t *pkt, uint8_t *buf, size_t buf_size);
esp_err_t rc_telemetry_decode(const uint8_t *buf, size_t len, rc_telemetry_packet_t *pkt);
```

---

## Phase 5 — Car (MR-21 → MR-25)

### MR-21: PWM output ✅

**Plik:** `components/output_pwm/`

```c
typedef struct {
    gpio_num_t pin;
    uint32_t   freq_hz;
    uint16_t   min_us;
    uint16_t   max_us;
} output_pwm_config_t;

esp_err_t output_pwm_init(const output_pwm_config_t *config, size_t count);
esp_err_t output_pwm_set(uint8_t index, int16_t value);  /* -1000…+1000 */
```

---

### MR-22: Servo steering ✅

CH_STEERING (index 1) → `output_pwm_set(1, steering)`. 50 Hz, 1000–2000 µs.

---

### MR-23: ESC throttle ✅

CH_THROTTLE (index 0) → `output_pwm_set(0, throttle)`. Standard PWM 50 Hz.

---

### MR-23.5: Arming logic ✅

```c
bool rc_arm_update(rc_arm_state_t *arm, const rc_channel_state_t *channels,
                   uint8_t arm_switch_ch, uint8_t throttle_ch);
```

Motor output = 0 dopóki `!arm.armed`. Throttle musi być na neutralnym przy armowaniu.

---

### MR-24: RX failsafe ✅

```c
static void failsafe_apply(void) {
    output_pwm_set(0, 0);   /* ESC safe */
    output_pwm_set(1, 0);   /* servo center */
    s_arm.armed = false;    /* disarm */
}
```

Wywołany gdy `rc_core_get_link_state() == RC_STATE_FAILSAFE`.

---

### MR-25: End-to-end car ✅

Kompletna integracja TX → ESP-NOW → RX → PWM. **To jest v0.1.0.**

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
