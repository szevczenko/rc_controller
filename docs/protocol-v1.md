# RC Protocol v1

Wire protocol used between TX and RX. Transport-agnostic: the same byte stream is passed to ESP-NOW, nRF24, or SX1280 without changes.

---

## General rules

- All multi-byte integers are **little-endian**.
- All packets end with a **CRC-16/CCITT-FALSE** (poly `0x1021`, init `0xFFFF`).
- Maximum raw packet size: `5 + 16*2 + 2 = 39 bytes` (header + 16 channels + CRC).
- ESP-NOW payload limit (v1.0): 250 bytes — current packets are well within budget.

---

## Packet types

| Value | Name | Direction |
|-------|------|-----------|
| `0x01` | `RC_PACKET_TYPE_RC` | TX → RX |
| `0x02` | `RC_PACKET_TYPE_TELEMETRY` | RX → TX |
| `0x03` | `RC_PACKET_TYPE_BIND` | TX ↔ RX |
| `0x04` | `RC_PACKET_TYPE_OTA` | TX → RX (future) |

---

## RC packet (type `0x01`)

Sent by TX at 50 Hz (v0.1).

```
Offset  Size  Field           Description
──────  ────  ─────────────   ─────────────────────────────────────────
0       1     version         Protocol version = 1
1       1     type            0x01
2       2     sequence        Monotonic counter, uint16 LE, rolls over
4       1     channel_count   Number of channels sent (1–16)
5       2*N   channels[N]     int16 LE, range -1000 to +1000
5+2N    2     crc             CRC-16/CCITT-FALSE LE, covers bytes 0..(5+2N-1)
```

Example for 4 channels (total 13 bytes):

```
00: 01           version=1
01: 01           type=RC
02: 2A 00        sequence=42 (LE)
04: 04           channel_count=4
05: 00 00        CH1=0
07: E8 03        CH2=1000
09: 18 FC        CH3=-1000
0B: F4 01        CH4=500
0D: XX XX        CRC-16
```

### Channel mapping (car model)

| Channel | Index | Signal |
|---------|-------|--------|
| CH1 | 0 | Throttle (ADC gimbal) |
| CH2 | 1 | Steering (ADC gimbal) |
| CH3 | 2 | SW1 ARM switch (+1000 = ON) |
| CH4 | 3 | SW2 MODE switch (+1000 = ON) |

---

## Telemetry packet (type `0x02`)

Sent by RX at 10 Hz.

```
Offset  Size  Field           Description
──────  ────  ─────────────   ─────────────────────────────────────────
0       1     version         Protocol version = 1
1       1     type            0x02
2       1     rssi            Signed dBm (int8)
3       1     link_quality    0–100 %
4       2     battery_mv      Battery voltage in mV, uint16 LE (0 = unknown)
6       4     uptime_ms       RX uptime in ms, uint32 LE
10      2     crc             CRC-16/CCITT-FALSE LE, covers bytes 0..9
```

Fixed size: **12 bytes**.

---

## Bind packet (type `0x03`)

Used during pairing (MR-17.5). Not part of normal operation.

```
Offset  Size  Field           Description
──────  ────  ─────────────   ─────────────────────────────────────────
0       1     version         1
1       1     type            0x03
2       1     subtype         0x01 = BIND_REQUEST  (TX → broadcast)
                              0x02 = BIND_RESPONSE (RX → TX unicast)
3       6     mac             Sender MAC address
9       2     crc             CRC-16/CCITT-FALSE LE
```

Fixed size: **11 bytes**.

---

## Sequence number

- `uint16`, range 0–65535, wraps around.
- TX increments by 1 per packet.
- RX validates via `rc_protocol_check_sequence()`:

| Result | Condition | Action |
|--------|-----------|--------|
| `RC_SEQ_OK` | `received == last + 1` (mod 65536) | Accept, update counter |
| `RC_SEQ_GAP` | `received` is ahead by ≤ 1000 | Accept, count missing as lost |
| `RC_SEQ_REPLAY` | `received` is behind | Reject |

---

## CRC-16/CCITT-FALSE

- Polynomial: `0x1021`
- Initial value: `0xFFFF`
- Input/output not reflected
- Covers all bytes before the CRC field
- Appended as 2 bytes, little-endian

Reference implementation (`rc_protocol.c`):

```c
static uint16_t crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int b = 0; b < 8; b++) {
            crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : (crc << 1);
        }
    }
    return crc;
}
```

---

## Failsafe

- RX triggers failsafe if no valid RC packet is received for **500 ms**.
- Failsafe state: throttle = 0, steering = 0, disarm.
- Recovery: automatic on next valid packet.

---

## Version history

| Version | Change |
|---------|--------|
| 1 | Initial release (v0.1.0) |
