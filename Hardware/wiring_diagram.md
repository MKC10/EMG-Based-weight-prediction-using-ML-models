# Wiring Diagram — EMG Edge-ML Wearable

## System Overview

```
                    ┌─────────────────────┐
  Ag/AgCl           │   MyoWare 2.0       │
  Electrodes ───────│   EMG Sensor        │
  (on arm)          │   (red board)       │
                    └────────┬────────────┘
                             │ SIG (0–3.3V amplified)
                             │
                    ┌────────▼────────────┐
                    │  Heltec LoRa32 V3   │
                    │  (ESP32-S3)         │
                    │                     │
                    │  GPIO7 ◄── SIG      │
                    │  3.3V ──► VCC       │
                    │  GND  ──► GND       │
                    │                     │──► LoRa antenna
                    │  GPIO1 (VBAT ADC)   │◄── Li-Po battery (JST)
                    │                     │
                    │  GPIO25 ──► LED+    │──► 220Ω ──► LED ──► GND
                    └─────────────────────┘
```

---

## Pin Connection Table

### MyoWare 2.0 → Heltec LoRa32 V3

| MyoWare 2.0 Pin | Heltec LoRa32 V3 Pin | Wire Color | Notes |
|---|---|---|---|
| `+` (VCC) | `3.3V` | Red | Do NOT use 5V — MyoWare 2.0 is 3.3V compatible |
| `-` (GND) | `GND` | Black | Common ground |
| `SIG` | `GPIO7` (ADC1_CH6) | Yellow | Amplified EMG signal 0–3.3V |

### Electrode → MyoWare 2.0

| Electrode | MyoWare Connector | Placement |
|---|---|---|
| E1 (active) | `+IN` snap | Along muscle belly, ~20mm from E2 |
| E2 (active) | `-IN` snap | Along muscle belly, ~20mm from E1 |
| REF (reference) | `REF` snap | Bony landmark — elbow or wrist |

### LED Indicator → Heltec LoRa32 V3

| Component | Pin | Notes |
|---|---|---|
| LED anode (+) | `GPIO25` via 220Ω resistor | Lights on heavy load prediction |
| LED cathode (-) | `GND` | |

### Battery → Heltec LoRa32 V3

| Battery Wire | Board Connector | Notes |
|---|---|---|
| Red (+) | `BAT+` JST PH 2.0 | On-board charging + protection circuit |
| Black (-) | `BAT-` JST PH 2.0 | |

---

## Electrode Placement (Bicep Brachii)

```
         ┌──────────────────────────────────┐
         │           Upper Arm              │
         │                                  │
         │    [E1] ───── 20mm ───── [E2]    │
         │          (muscle belly)           │
         │       along fiber direction       │
         │                                  │
         │              [REF]               │
         │          (olecranon/elbow)        │
         └──────────────────────────────────┘

E1, E2  →  differential active electrodes
REF     →  reference on bony landmark (minimizes common-mode noise)
```

**Tips:**
- Clean skin with alcohol wipe before placing electrodes
- Press firmly for 10 seconds after placing — poor contact = noisy signal
- Keep electrode cables twisted together to reduce interference
- Replace electrodes every session — gel dries out after ~4 hours

---

## Power Budget

| Mode | Current Draw | Source |
|---|---|---|
| Active inference (ADC + CPU) | ~80 mA | Li-Po |
| LoRa TX burst (50ms) | ~120 mA | Li-Po |
| Average (1s TX interval) | ~85 mA | Li-Po |
| **Estimated runtime** | **~12 hours** | 2000 mAh Li-Po |
