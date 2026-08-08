Tak — teraz mam już wystarczająco dużo informacji, żeby zaprojektować sensowny plan. I po tych odpowiedziach **zmieniłbym trochę wcześniejsze założenia**: nie budowałbym tego jako „ESP-NOW RC car”, tylko jako **mały, własny system RC z wymienną warstwą radiową**.

ESP32-WROOM, S3 i C6 są obsługiwane przez ekosystem ESP-NOW, więc możemy zacząć od tego, co już masz. ([GitHub][1])

## 1. Cel projektu

Docelowo:

```text
                    ┌─────────────────────────┐
                    │       RC CONTROLLER     │
                    │                         │
                    │ ESP32-S3                │
                    │                         │
                    │ 2 × BetaFPV gimbal      │
                    │ 2 × 2-position switch  │
                    │ 3.5" ST7796 TFT         │
                    │                         │
                    │ RC Core                 │
                    │ Telemetry               │
                    │ Configuration            │
                    └────────────┬────────────┘
                                 │
                          RADIO ABSTRACTION
                                 │
                   ┌─────────────┼─────────────┐
                   │             │             │
                ESP-NOW       nRF24L01+      SX1280
                   │             │             │
                   └─────────────┼─────────────┘
                                 │
                              RX ESP32
                                 │
                  ┌──────────────┼──────────────┐
                  │              │              │
                PWM            CRSF*        Telemetry
                  │              │
                ESC/servo       FC
```

`*` CRSF zostawiłbym jako przyszły interfejs, szczególnie pod drona.

Pierwszy produkt:

> **v0.1.0 — RC Car MVP**

czyli tylko samochód, ale z fundamentami pod przyszłe funkcje.

---

# 2. Najważniejsza decyzja architektoniczna

Nie robimy:

```text
RC application
     ↓
ESP-NOW
```

Robimy:

```text
RC application
     ↓
RC protocol
     ↓
Radio interface
     ↓
ESP-NOW
```

Dzięki temu później:

```text
RC protocol
     ↓
Radio interface
     ├── ESP-NOW
     ├── nRF24L01+
     └── SX1280
```

**ESP-NOW nie będzie częścią protokołu RC.**

To jest moim zdaniem najważniejsza decyzja całego projektu.

---

# 2.1. Wymagania czasowe

```text
Packet rate:         50 Hz (v0.1), docelowo 150–250 Hz
End-to-end latency:  < 20 ms (gimbal → servo)
Failsafe timeout:    500 ms (brak pakietów → safe state)
Telemetry rate:      10 Hz (RX → TX)
Binding timeout:     5 s
```

Te wartości wpływają na design od MR-01 — timer ticki, priorytet tasków RTOS, rozmiar kolejek.

---

# 2.2. Bezpieczeństwo radiowe

```text
v0.1:  bez szyfrowania (uproszczenie, kontrolowane środowisko)
v0.2+: opcjonalne szyfrowanie CCMP (ESP-NOW native)
v0.3+: binding exchange key (shared secret przy parowaniu)
```

Bez szyfrowania każdy ESP32 w zasięgu może wysyłać pakiety do RX — akceptowalne tylko w fazie developmentu.

---

# 3. Proponowany podział software

Zrobiłbym projekt mniej więcej tak:

```text
rc-controller/
│
├── main/
│
├── components/
│   │
│   ├── rc_core/
│   │   ├── include/
│   │   └── src/
│   │
│   ├── rc_protocol/
│   │   ├── include/
│   │   └── src/
│   │
│   ├── radio/
│   │   ├── include/
│   │   └── src/
│   │
│   ├── radio_espnow/
│   │   ├── include/
│   │   └── src/
│   │
│   ├── input/
│   │
│   ├── output/
│   │
│   ├── telemetry/
│   │
│   ├── display/
│   │
│   ├── config/
│   │
│   └── ota/
│
├── app/
│   ├── transmitter/
│   └── receiver/
│
├── test/
│
├── docs/
│
└── partitions/
```

Nie wszystkie komponenty muszą istnieć od pierwszego MR.

**Organizacja buildu:**

Dwa osobne projekty ESP-IDF ze wspólnymi komponentami:

```text
rc-controller/
├── transmitter/     ← ESP-IDF project (target: ESP32-S3)
│   ├── main/
│   ├── CMakeLists.txt
│   └── sdkconfig.defaults
├── receiver/        ← ESP-IDF project (target: ESP32-WROOM)
│   ├── main/
│   ├── CMakeLists.txt
│   └── sdkconfig.defaults
├── components/      ← shared (EXTRA_COMPONENT_DIRS)
├── test/
├── docs/
└── partitions/
```

Wspólne komponenty (`rc_core`, `rc_protocol`, `radio`, `config`) są współdzielone przez oba projekty via `EXTRA_COMPONENT_DIRS` w CMake.

---

# 4. Warstwy

### `input`

Odpowiada wyłącznie za hardware:

```text
ADC
 ↓
input driver
 ↓
normalized value
```

Na przykład:

```c
typedef struct {
    int16_t throttle;
    int16_t steering;
    bool switch_1;
    bool switch_2;
} rc_input_t;
```

Nie chcę, żeby `rc_protocol` wiedział, że źródłem danych jest ADC.

---

### `rc_core`

Tutaj:

* deadzone
* center
* min/max
* expo
* trim
* failsafe logic
* później mixery
* kanały

Przykładowo:

```text
ADC 0...4095

       ↓

calibration

       ↓

-1000 ... +1000
```

Dla samochodu:

```text
CH1 = throttle
CH2 = steering
CH3 = switch 1
CH4 = switch 2
```

Później możemy dodać np.:

```text
CH5 = encoder
CH6 = button
...
```

---

# 5. `rc_protocol`

To jest **Twój właściwy protokół RC**.

Na początku nie komplikowałbym go.

Przykładowy pakiet:

```text
┌──────┬──────┬──────┬─────────────┬──────┐
│ VER  │ TYPE │ SEQ  │ CHANNELS    │ CRC  │
│ 1 B  │ 1 B  │ 2 B  │ variable    │ 2 B  │
└──────┴──────┴──────┴─────────────┴──────┘
```

Na przykład:

```text
VER       protocol version
TYPE      RC / telemetry / bind / OTA
SEQ       sequence number
CHANNELS  CH1...CH16
CRC       packet integrity
```

Nie używałbym bezpośrednio struktur C wysyłanych przez radio.

Czyli **nie**:

```c
esp_now_send(..., &struct_message, sizeof(struct_message));
```

tylko:

```c
rc_packet_encode(&packet, buffer, sizeof(buffer));
radio_send(buffer, length);
```

Dzięki temu później nRF24 i SX1280 dostają **dokładnie ten sam packet**.

---

# 6. Radio abstraction

Interfejs może wyglądać mniej więcej:

```c
typedef struct {
    esp_err_t (*init)(void);
    esp_err_t (*send)(const uint8_t *data, size_t len);
    esp_err_t (*set_channel)(uint8_t channel);
    int (*get_rssi)(void);
} radio_driver_t;
```

A aplikacja robi:

```c
radio_send(packet, packet_len);
```

Nie:

```c
esp_now_send(...);
```

To będzie bardzo ważne.

---

# 7. ESP-NOW

Pierwsza implementacja:

```text
radio/
   │
   └── radio_espnow/
           │
           └── ESP-NOW
```

ESP-NOW działa bez klasycznego połączenia Wi-Fi, a ESP-IDF udostępnia gotowy komponent/API. Aktualna dokumentacja opisuje m.in. wersje v1.0/v2.0 oraz zabezpieczenie CCMP. ([Espressif Systems][2])

Na początek ograniczyłbym własny packet do bardzo małego rozmiaru — zdecydowanie poniżej limitu 250 B starszej wersji ESP-NOW. ([Espressif Systems][2])

Realnie nasze pakiety będą miały raczej **kilkanaście–kilkadziesiąt bajtów**.

---

# 8. Odbiornik

RX będzie miał trzy główne części:

```text
ESP-NOW
   ↓
packet decoder
   ↓
RC state
   ↓
┌───────────────┐
│ output        │
├───────────────┤
│ throttle PWM  │
│ steering PWM  │
└───────────────┘
```

### Failsafe jest obowiązkowy

Nie robimy:

```text
brak pakietu → zostaw ostatnią wartość
```

tylko:

```text
pakiety OK
   ↓
normal operation

brak pakietów
   ↓
timeout
   ↓
FAILSAFE
   ↓
throttle = safe
steering = neutral
```

I to będzie częścią pierwszego release.

---

# 9. Telemetria

Zaprojektowałbym ją od początku jako kanał:

```text
TX ────────────────► RX
       RC packets

RX ────────────────► TX
       telemetry
```

Na przykład:

```text
RX telemetry:

battery_voltage
battery_current
temperature
rssi
link_quality
packet_loss
```

Nie wszystkie muszą działać w v0.1.

Na początku możemy mieć:

```text
RSSI
packet counter
packet loss
```

A kiedy podłączysz konkretny ESC/baterię, dołożymy właściwe dane.

---

# 10. Wyświetlacz

Twój ST7796 3.5" 320×480 potraktowałbym jako **osobny komponent**.

Nie mieszamy go z RC core.

```text
display/
   │
   ├── display_driver
   │
   └── display_ui
```

Docelowo:

```text
┌─────────────────────────┐
│ RC CAR            v0.3  │
├─────────────────────────┤
│                         │
│       ┌─────────┐       │
│       │         │       │
│       │  CAR    │       │
│       │         │       │
│       └─────────┘       │
│                         │
│ THR  ███████░░  +73     │
│ STR  ████░░░░░  -20     │
│                         │
│ LINK       98%          │
│ RSSI       -54 dBm      │
│ BATTERY    7.82 V       │
│                         │
│ SW1 ARM     ON          │
│ SW2 MODE    NORMAL      │
└─────────────────────────┘
```

Do ESP-IDF użyłbym **ESP LCD Panel API**, a nie budował własnej obsługi SPI od zera. ESP-IDF ma do tego dedykowane API.

UI możemy później oprzeć np. o LVGL, ale **nie potrzebujemy go w pierwszych MR-ach**.

---

# 11. OTA

Pierwszy release może być bez OTA, ale **partition table przygotowałbym od początku pod OTA**.

ESP-IDF wymaga dla klasycznego bezpiecznego OTA co najmniej dwóch slotów aplikacji (`ota_0`, `ota_1`) oraz partycji OTA data. ([Espressif Systems][3])

Czyli już v0.1:

```text
partition table

nvs
otadata
phy_init
ota_0
ota_1
```

Natomiast:

```text
v0.1
  OTA infrastructure: TAK
  OTA transport: NIE
  OTA UI: NIE
```

Potem:

```text
v0.x
  OTA over ESP-NOW
```

---

# 12. Konfiguracja

Od początku:

```text
NVS
 │
 ├── calibration
 ├── model
 ├── radio
 ├── failsafe
 └── display
```

Nie hardcodujemy np.:

```c
#define THROTTLE_CENTER 2048
```

tylko:

```text
NVS
 ↓
configuration
 ↓
RC core
```

To pozwoli zrobić później ekran konfiguracji.

---

# 13. Proponowane MR-y

I tutaj zrobiłbym to naprawdę małymi krokami.

## Milestone 0 — Development infrastructure

### MR-01 — Project skeleton

**Cel:** projekt ESP-IDF buduje się.

Dodajemy:

* CMake
* ESP-IDF
* `main`
* podstawowe komponenty
* `sdkconfig.defaults`
* README
* `.gitignore`

**Test:**

```bash
idf.py build
```

**Definition of Done:**

> świeży checkout → build bez ręcznych zmian.

---

### MR-02 — CI/build matrix

Dodajemy CI:

```text
ESP32
ESP32-S3
ESP32-C6
```

Nie musimy jeszcze testować hardware.

Cel: wykrywać błędy kompilacji.

---

# Milestone 1 — Hardware input

### MR-03 — ADC abstraction

Implementacja:

```text
ADC → raw value
```

API:

```c
input_adc_read(channel)
```

---

### MR-04 — Single gimbal

Obsługujemy jeden potencjometr BetaFPV.

Test:

```text
serial monitor

ADC = 2047
ADC = 2500
ADC = 4095
...
```

---

### MR-05 — Two gimbals

Dodajemy:

```text
throttle
steering
```

i diagnostykę.

---

### MR-06 — Calibration

Dodajemy:

```text
min
center
max
```

oraz zapis do NVS.

To będzie bardzo przydatne, bo potencjometry mają tolerancję i center nie musi być idealnie 2048.

---

### MR-07 — Switches

Dodajemy:

```text
SW1
SW2
```

Każdy:

```text
OFF / ON
```

---

# Milestone 2 — RC Core

### MR-08 — Channel model

Tworzymy abstrakcję:

```text
CH1
CH2
CH3
CH4
...
CH16
```

Samochód:

```text
CH1 throttle
CH2 steering
CH3 switch
CH4 switch
```

---

### MR-09 — Channel normalization

Wprowadzamy standardową reprezentację:

```text
-1000 ... +1000
```

dla analogów.

Switch:

```text
-1000
+1000
```

---

### MR-10 — Deadzone / center

Dodajemy:

```text
center
deadzone
```

To szczególnie ważne dla samochodu.

---

### MR-10.5 — Expo / rates

Dodajemy krzywą exponencjalną na throttle i steering:

```text
output = input^expo_factor
```

Konfigurowalny via NVS. Expo drastycznie poprawia kontrolowalność przy małych wychyleniach.

---

### MR-11 — Failsafe model

Jeszcze bez radia.

Testujemy:

```text
normal
→ timeout
→ failsafe
→ recovery
```

---

# Milestone 3 — Protocol

### MR-12 — Packet format

Definiujemy **RC Protocol v1**.

Dokument:

```text
docs/protocol-v1.md
```

To ważny MR.

---

### MR-13 — Encoder

```c
rc_packet_encode()
```

Testy jednostkowe:

```text
packet → bytes
```

---

### MR-14 — Decoder

```c
rc_packet_decode()
```

Test:

```text
encode
 ↓
decode
 ↓
same data
```

---

### MR-15 — CRC + sequence

Dodajemy:

```text
CRC
SEQ
```

Testujemy:

```text
correct packet → accepted

corrupted packet → rejected (CRC mismatch)

future sequence → accepted, count gap as packet loss

past sequence → rejected (replay protection)

rollover (uint16 overflow) → handled via modular arithmetic
```

---

# Milestone 4 — Radio & ESP-NOW

### MR-16 — Radio abstraction

Definiujemy interfejs `radio_driver_t` + implementacja loopback/mock do testów:

```c
typedef struct {
    esp_err_t (*init)(void);
    esp_err_t (*send)(const uint8_t *data, size_t len);
    esp_err_t (*set_channel)(uint8_t channel);
    int (*get_rssi)(void);
} radio_driver_t;
```

Test: encode → loopback → decode → validate.

To jest moment, w którym architektura „wymiennego radia" zaczyna działać.

---

### MR-17 — ESP-NOW driver

Implementacja `radio_driver_t` dla ESP-NOW:

```text
ESP32 TX
   ↓
radio_espnow (implements radio_driver_t)
   ↓
ESP32 RX
```

Test: wysłanie i odbiór surowych bajtów między dwoma ESP32.

---

### MR-17.5 — Binding / pairing

Mechanizm parowania TX ↔ RX:

```text
TX broadcast BIND_REQUEST
   ↓
RX responds BIND_RESPONSE (MAC + capabilities)
   ↓
TX saves peer to NVS
RX saves TX to NVS
```

Bez tego w MR-18 trzeba hardcodować MAC adres.

---

### MR-18 — RC packet over ESP-NOW

```text
gimbals
 ↓
RC core
 ↓
packet
 ↓
radio
 ↓
ESP-NOW
 ↓
RX
```

Na RX pokazujemy dane przez UART.

---

### MR-19 — Link statistics

Dodajemy:

```text
RSSI
packet TX
packet RX
packet loss
sequence
```

---

### MR-20 — Bidirectional telemetry

RX → TX:

```text
RSSI
link quality
packet counters
```

I mamy już prawdziwy dwukierunkowy link.

---

# Milestone 5 — Car

### MR-21 — PWM output

Jeden kanał:

```text
CH1 → PWM
```

---

### MR-22 — Servo steering

```text
CH2 → servo
```

---

### MR-23 — ESC throttle

```text
CH1 → ESC
```

---

### MR-23.5 — Arming logic

Logika bezpieczeństwa przy starcie:

```text
power on → throttle disabled (disarmed)
SW1 = ARM + throttle at neutral → armed
SW1 = DISARM → throttle disabled
```

Zapobiega sytuacji, w której po włączeniu RX z gimbalem nie na zerze auto natychmiast jedzie.

---

### MR-24 — RX failsafe

Integrujemy:

```text
radio timeout
 ↓
RC failsafe
 ↓
ESC safe
servo neutral
disarm
```

**Ten MR traktowałbym jako bardzo ważny safety milestone.**

---

### MR-25 — End-to-end car

Finalnie:

```text
       TX
 ┌──────────────┐
 │ gimbal       │
 │ gimbal       │
 │ switch       │
 │ switch       │
 └──────┬───────┘
        │
    ESP-NOW
        │
        ▼
 ┌──────────────┐
 │ RX ESP32     │
 │              │
 │ PWM → ESC    │
 │ PWM → servo  │
 └──────────────┘
```

**To jest kandydat na `v0.1.0`.**

---

# Milestone 6 — Display

Nie robiłbym ekranu wcześniej niż działający samochód.

### MR-26 — ST7796 driver

```text
ESP32-S3
   ↓ SPI
ST7796
```

Test:

```text
solid color
```

---

### MR-27 — Basic graphics

Test:

```text
text
lines
rectangles
```

---

### MR-28 — RC dashboard

Wyświetlamy:

```text
THROTTLE
STEERING
SW1
SW2
RSSI
LINK
```

---

### MR-29 — Telemetry dashboard

Dodajemy dane RX.

---

# Milestone 7 — Configuration

### MR-30 — Configuration component

```text
config_load()
config_save()
```

NVS.

---

### MR-31 — Model configuration

Struktura:

```text
Model
 ├── name
 ├── channel mapping
 ├── calibration
 ├── failsafe
 └── radio settings
```

---

### MR-32 — Model selection

Na ekranie:

```text
CAR
DRONE
...
```

Na początku wystarczy jeden model.

---

# Milestone 8 — OTA

### MR-33 — OTA partition infrastructure

Tak naprawdę część z tego zrobimy już w MR-01.

---

### MR-34 — Local OTA

Najpierw OTA przez zwykłe Wi-Fi.

Dzięki temu testujemy sam mechanizm ESP-IDF niezależnie od ESP-NOW.

---

### MR-35 — OTA protocol

Dodajemy typy:

```text
PACKET_TYPE_OTA_BEGIN
PACKET_TYPE_OTA_DATA
PACKET_TYPE_OTA_END
PACKET_TYPE_OTA_STATUS
```

---

### MR-36 — OTA over ESP-NOW

```text
TX
 │
 │ firmware chunks
 ▼
RX
 │
 ▼
ota_0 / ota_1
```

---

### MR-37 — OTA UI

Na ekranie:

```text
Updating RX...

████████░░ 82%

Do not power off
```

---

# Milestone 9 — przyszłe radio

### MR-38 — nRF24 radio interface

Dodajemy:

```text
radio_nrf24
```

bez zmiany:

```text
RC Core
Protocol
Telemetry
UI
```

---

### MR-39 — SX1280 radio interface

Analogicznie:

```text
radio_sx1280
```

I wtedy możemy zacząć robić testy:

```text
ESP-NOW
vs
nRF24
vs
SX1280
```

pod względem:

* latency
* packet loss
* range
* power consumption
* link recovery

---

# 14. Wersje projektu

Proponuję nie robić jednego wielkiego `v1.0`.

### `v0.1.0`

**RC Car MVP**

* 2 gimbale
* 2 switch
* ESP-NOW
* TX
* RX
* throttle
* steering
* failsafe
* podstawowa telemetryka
* brak OTA
* brak ekranu lub minimalny test ekranu

### `v0.2.0`

**RC Controller**

* ST7796
* dashboard
* konfiguracja
* calibration
* NVS
* telemetryka

### `v0.3.0`

**OTA**

* OTA TX
* OTA RX
* firmware transfer przez ESP-NOW

### `v0.4.0`

**Multi-model**

```text
CAR
DRONE
...
```

### `v0.5.0`

**Alternative radio**

nRF24 / SX1280.

### `v1.0.0`

Dopiero wtedy:

> własny system RC gotowy do poważniejszego użycia.

---

# 15. Co z dronem?

Nie dodawałbym go do pierwszych MR-ów.

Ale już teraz protokół powinien mieć:

```text
CH1...CH16
```

a nie tylko:

```text
throttle
steering
```

Dzięki temu później:

```text
CAR

CH1 throttle
CH2 steering
CH3 switch
CH4 switch
```

a:

```text
DRONE

CH1 roll
CH2 pitch
CH3 throttle
CH4 yaw
CH5 arm
CH6 mode
...
```

Nie trzeba zmieniać radia.

---

# 16. CRSF

Tutaj też proponuję **nie implementować CRSF w pierwszym release**, ale przygotować miejsce:

```text
rc_core
    │
    ▼
rc_protocol
    │
    ├── radio → ESP-NOW
    │
    └── output → CRSF
```

Wtedy przyszły odbiornik może mieć:

```text
ESP32 RX
 │
 ├── PWM → ESC
 │
 ├── PWM → servo
 │
 └── UART → CRSF → FC
```

I tym samym jeden odbiornik może obsługiwać samochód oraz później drona.

---

# 17. Co z wyborem ESP32?

Na **TX wybrałbym ESP32-S3**.

Powód jest praktyczny:

```text
ESP32-S3
├── dużo RAM
├── dobry wybór pod TFT/LVGL
├── wystarczająco dużo GPIO
└── Wi-Fi/ESP-NOW
```

Na RX:

**zwykły ESP32-WROOM** wystarczy z dużym zapasem.

C6 zachowałbym do eksperymentów.

Czyli:

```text
TX = ESP32-S3
RX = ESP32-WROOM
```

Nie ma potrzeby robić całego projektu multiplatformowego od pierwszego dnia.

---

# 18. Jedna rzecz, którą zrobiłbym inaczej niż typowy projekt hobby

Od początku zrobiłbym **hardware-in-the-loop-ish testy protokołu**.

Na PC:

```text
test:
    generate RC packet
          ↓
       encode
          ↓
      corrupt?
          ↓
       decode
          ↓
       validate
```

Czyli większość `rc_protocol` możemy testować bez podłączania ESP32.

To bardzo pomoże utrzymać projekt, gdy później dojdzie nRF24/SX1280.

---

# 19. Ostateczna architektura

Docelowo widzę to tak:

```text
                       ┌─────────────────────┐
                       │        UI           │
                       │                     │
                       │ TFT / buttons       │
                       └──────────┬──────────┘
                                  │
                       ┌──────────▼──────────┐
                       │     RC CORE         │
                       │                     │
                       │ channels            │
                       │ calibration         │
                       │ mixer               │
                       │ failsafe            │
                       │ models              │
                       └──────────┬──────────┘
                                  │
                       ┌──────────▼──────────┐
                       │    RC PROTOCOL      │
                       │                     │
                       │ RC packets          │
                       │ telemetry           │
                       │ binding              │
                       │ OTA                  │
                       └──────────┬──────────┘
                                  │
                       ┌──────────▼──────────┐
                       │   RADIO INTERFACE   │
                       └─────┬────┬────┬─────┘
                             │    │    │
                         ESP-NOW nRF24 SX1280
                             │    │    │
                             └────┴────┘
                                  │
                              RX ESP32
                                  │
                 ┌────────────────┼────────────────┐
                 │                │                │
                 ▼                ▼                ▼
                PWM              CRSF          Telemetry
                 │                │
              ESC/servo          FC
```

I to jest architektura, którą **warto utrzymać przez cały projekt**.

---

## Jedna korekta przed rozpoczęciem implementacji

Przed napisaniem drivera wyświetlacza zrobiłbym osobny MR, który **identyfikuje dokładny kontroler, rozdzielczość (320×480 vs inne warianty), pinout SPI, napięcia i ewentualny touch**. Nie zakładałbym tego na podstawie samego opisu aukcji.

I jeszcze jedna rzecz: Twoje BetaFPV Nano Joystick są **potencjometrycznymi gimbalami**, więc calibration/deadzone/center powinny być absolutnie pierwszorzędnym elementem `rc_core`, a nie dodatkiem na końcu. ([Reddit][4])

### Moja propozycja następnego kroku

Zanim zaczniemy pisać **MR-01**, możemy jeszcze ustalić **specyfikację repozytorium i konkretne kontrakty API**:

```text
components/
├── rc_core
├── rc_protocol
├── radio
├── radio_espnow
├── input_adc
├── input_switch
├── output_pwm
├── telemetry
├── config
├── display
└── ota
```

oraz dokładnie zdefiniować:

* `rc_packet_t`
* format pakietu na wire
* kanały CH1–CH16
* zakres wartości
* sequence number
* CRC
* heartbeat
* telemetry packet
* failsafe timeout
* `radio_driver_t`
* wersjonowanie protokołu

**To zrobiłbym jako „Architecture v0.1” przed pierwszym kodem**, bo późniejsze nRF24/SX1280 i OTA będą wtedy naturalnym rozszerzeniem, a nie przepisywaniem projektu.

[1]: https://github.com/espressif/esp-now/blob/master/User_Guide.md?utm_source=chatgpt.com "esp-now/User_Guide.md at master · espressif/esp-now · GitHub"
[2]: https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32/api-reference/network/esp_now.html?utm_source=chatgpt.com "ESP-NOW - ESP32 - — ESP-IDF Programming Guide v5.5 documentation"
[3]: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/ota.html?utm_source=chatgpt.com "Over The Air Updates (OTA) - ESP32 - — ESP-IDF Programming Guide latest documentation"
[4]: https://www.reddit.com/r/fpv/comments/st0t53?utm_source=chatgpt.com "literadio 3 info"
