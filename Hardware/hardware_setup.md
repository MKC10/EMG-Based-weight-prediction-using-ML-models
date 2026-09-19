# Hardware Setup Guide — EMG Edge-ML Wearable

## What You Need

See `bom.csv` for the full component list with costs and suppliers.

**Core components:**
- Heltec LoRa32 V3 (ESP32-S3) — main MCU, runs full pipeline
- MyoWare 2.0 Muscle Sensor — EMG amplifier (the red board)
- Ag/AgCl surface EMG electrodes (disposable, 3 per session)
- 3.7V Li-Po 2000mAh battery with JST PH 2.0 connector
- 5mm blue LED + 220Ω resistor — inference output indicator

---

## Step 1 — Install Heltec Board Support

1. Open Arduino IDE → **File → Preferences**
2. Add to Additional Boards Manager URLs:
   ```
   https://resource.heltec.cn/download/package_heltec_esp32_index.json
   ```
3. **Tools → Board Manager** → search `Heltec ESP32` → Install
4. Select board: **Heltec WiFi LoRa 32(V3)**
5. Select port: whichever COM port appears when you plug in the Heltec via USB-C

---

## Step 2 — Wire MyoWare 2.0 to Heltec LoRa32 V3

```
MyoWare 2.0          Heltec LoRa32 V3
─────────────        ────────────────
    +    ──────────►  3.3V
    -    ──────────►  GND
   SIG   ──────────►  GPIO7
```

> ⚠️ Use 3.3V only — MyoWare 2.0 operates at 3.3V.
> Do not connect to 5V or you will damage the sensor.

For full pin tables and electrode placement diagram see `wiring_diagram.md`.

---

## Step 3 — Place Electrodes on Arm

Target muscle: **Bicep Brachii** (upper arm)

1. Clean skin with alcohol wipe, let dry 30 seconds
2. Snap **E1** and **E2** onto MyoWare 2.0's active snaps
3. Snap **REF** onto the reference snap
4. Place E1 and E2 **along the muscle belly**, ~20mm apart
5. Place REF on a **bony landmark** — inner elbow or wrist
6. Press each electrode firmly for 10 seconds
7. Flash `EMG_DATAFILTERING.ino` to the Heltec and open Serial Monitor
   at 115200 baud — you should see non-zero EMG values when you flex

**Signal looks wrong?**
- Values stuck at 0 → check SIG wire on GPIO7 and VCC on 3.3V
- Values noisy/jumping → re-clean skin, re-press electrodes, check REF placement
- Values saturated (4095 always) → MyoWare gain too high, move electrodes slightly apart

---

## Step 4 — Data Collection

1. Wire MyoWare to Heltec as in Step 2
2. Flash `EMG_DATAFILTERING.ino` to the Heltec LoRa32 V3
3. Open Serial Monitor at 115200 baud
4. Follow prompts — hold each weight for 14 seconds when asked
5. Copy Serial output to a `.csv` file, label each row with the weight class
6. Repeat for at least 5 sessions per weight class

---

## Step 5 — Run Calibration

1. Flash `CALIBRATION.ino` to the Heltec LoRa32 V3
2. Open Serial Monitor at 115200 baud
3. Follow on-screen prompts — hold each weight class when asked
4. Copy the printed `min_`/`max_` values into `EMG_MAIN_INFERENCE.ino`'s
   normalization section

---

## Step 6 — Flash Main Inference Firmware

1. Open `EMG_MAIN_INFERENCE.ino` in Arduino IDE
2. Paste your calibration values into the normalization section
3. Paste your trained SVM weights into the `svm_w` / `svm_b` arrays
4. Set `LORA_BAND` to match your region:
   - `868E6` → Europe
   - `915E6` → US / India
5. Flash to Heltec LoRa32 V3
6. Open Serial Monitor → flex your arm → you should see:
   ```
   [FEATURES] MAV=0.412 RMS=0.389 WL=0.601 ...
   [PREDICT]  Medium (2-5 kg)
   [LoRa TX]  Class=1  Batt=3842mV
   ```

---

## Step 7 — Attach Battery for Portable Use

1. Connect 3.7V Li-Po to Heltec's JST PH 2.0 `BAT` connector
2. Heltec's on-board BMS handles charging automatically via USB-C
3. Check battery voltage in Serial output — `Batt=` field
4. Low battery warning fires below 3300mV

---

## Troubleshooting

| Symptom | Likely cause | Fix |
|---|---|---|
| No Serial output | Wrong COM port or baud | Check Tools → Port, set monitor to 115200 |
| EMG signal = 0 always | SIG wire disconnected | Re-check GPIO7 connection |
| Prediction always class 0 | Normalization bounds wrong | Re-run CALIBRATION.ino |
| LoRa TX fails | Wrong frequency band | Change LORA_BAND in config |
| Battery drains in <2hr | High TX rate | Increase TX_INTERVAL_MS to 2000 |
| Noisy signal during flex | Poor electrode contact | Replace electrodes, clean skin |
