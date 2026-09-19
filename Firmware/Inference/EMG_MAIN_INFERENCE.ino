// ─────────────────────────────────────────────────────────────
// EMG_MAIN_INFERENCE.ino
// Full pipeline on one board — Heltec LoRa32 V3 (ESP32-S3)
//
// Flow (every 200ms window):
//   ADC → Butterworth BP → RMS envelope →
//   Feature extraction → Normalize → SVM → LoRa TX
//
// Matches your existing files:
//   EMG_DATAFILTERING.ino  — ADC + filter style
//   SIGNAL_SMOOTHING.ino   — RMS envelope + same BP coefficients
//   FEATURE_EXTRACTION.ino — 6 features + normalization
//   SVM_INFERENCE.ino      — linear SVM classifier
//   LORA_TX.ino            — Heltec LoRa32 V3 packet TX
//
// BEFORE FLASHING:
//   1. Run CALIBRATION.ino and copy min/max values into
//      the normalization section below
//   2. Train SVM in Python, copy coef_ + intercept_ into
//      the SVM weights section below
//   3. Set LORA_BAND to match your region (868E6 EU, 915E6 US)
// ─────────────────────────────────────────────────────────────

#include "LoRaWan_APP.h"   // Heltec LoRa32 V3

// ════════════════════════════════════════════
//  CONFIG
// ════════════════════════════════════════════
#define EMG_PIN         7
#define SAMPLE_RATE     500          // Hz — matches your files
#define BAUD_RATE       115200
#define MA_WINDOW       15           // RMS smoother window — from SIGNAL_SMOOTHING.ino
#define WINDOW_SIZE     100          // 200ms @ 500Hz
#define NUM_FEATURES    6
#define NUM_CLASSES     3
#define TX_INTERVAL_MS  1000         // LoRa transmit every 1s

#define LORA_BAND       868E6        // 868 EU / 915E6 US
#define LORA_TX_POWER   14
#define LORA_SF         7
#define LORA_BW         125E3
#define LORA_CR         5
#define VBAT_PIN        1            // Heltec V3 battery ADC
#define VBAT_SCALE      4.9          // Voltage divider ratio on V3

const char* classLabel[NUM_CLASSES] = {
  "Light (0-2 kg)",
  "Medium (2-5 kg)",
  "Heavy (5+ kg)"
};

// ════════════════════════════════════════════
//  NORMALIZATION BOUNDS
//  Replace with output from CALIBRATION.ino
// ════════════════════════════════════════════
float min_MAV  = 0.0,  max_MAV  = 200.0;
float min_RMS  = 0.0,  max_RMS  = 200.0;
float min_WL   = 0.0,  max_WL   = 5000.0;
float min_ZC   = 0.0,  max_ZC   = 50.0;
float min_SSC  = 0.0,  max_SSC  = 50.0;
float min_IEMG = 0.0,  max_IEMG = 20000.0;

// ════════════════════════════════════════════
//  SVM WEIGHTS  (linear kernel, OvR)
//  Replace with: clf.coef_ and clf.intercept_
//  from your sklearn training script
// ════════════════════════════════════════════
float svm_w[NUM_CLASSES][NUM_FEATURES] = {
  { 2.10, -1.30,  0.80,  0.50, -0.20,  1.60 },  // class 0
  {-0.80,  0.40,  1.20, -0.30,  0.90, -0.50 },  // class 1
  {-1.30,  0.90, -2.00, -0.20, -0.70, -1.10 }   // class 2
};
float svm_b[NUM_CLASSES] = { 0.15, -0.10, -0.05 };

// ════════════════════════════════════════════
//  STATE
// ════════════════════════════════════════════
float maBuffer[MA_WINDOW];
int   maIndex  = 0;
float maSum    = 0;

float  emgWindow[WINDOW_SIZE];
int    windowIndex = 0;

// Last inference result (sent by LoRa)
uint8_t lastClass      = 0;
float   lastFeatures[NUM_FEATURES];
unsigned long lastTxMs = 0;

// ════════════════════════════════════════════
//  SETUP
// ════════════════════════════════════════════
void setup() {
  Serial.begin(BAUD_RATE);
  analogReadResolution(12);

  // LoRa init
  Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);
  Radio.Init(NULL);
  Radio.SetChannel(LORA_BAND);
  Radio.SetTxConfig(
    MODEM_LORA, LORA_TX_POWER, 0,
    LORA_BW, LORA_SF, LORA_CR,
    8, false, true, 0, 0, false, 3000
  );

  memset(maBuffer,    0, sizeof(maBuffer));
  memset(emgWindow,   0, sizeof(emgWindow));
  memset(lastFeatures, 0, sizeof(lastFeatures));

  delay(2000);
  Serial.println("EMG inference started.");
}

// ════════════════════════════════════════════
//  MAIN LOOP  — runs at 500 Hz via timer
// ════════════════════════════════════════════
void loop() {
  static unsigned long past = 0;
  unsigned long present  = micros();
  unsigned long interval = present - past;
  past = present;

  static long timer = 0;
  timer -= interval;

  if (timer < 0) {
    timer += 1000000 / SAMPLE_RATE;

    // ── 1. ADC read ──
    float raw = analogRead(EMG_PIN);

    // ── 2. Butterworth BP filter (20–40 Hz @ 500 Hz) ──
    //    Exact coefficients from SIGNAL_SMOOTHING.ino
    float filtered = EMGFilter(raw);
    if (filtered < 0) filtered = 0;

    // ── 3. RMS envelope (moving average) ──
    //    Same logic as SIGNAL_SMOOTHING.ino
    maSum -= sq(maBuffer[maIndex]);
    maBuffer[maIndex] = filtered;
    maSum += sq(filtered);
    maIndex = (maIndex + 1) % MA_WINDOW;
    float smoothed = sqrt(maSum / MA_WINDOW);

    // ── 4. Fill 200ms window ──
    emgWindow[windowIndex++] = smoothed;

    if (windowIndex >= WINDOW_SIZE) {
      windowIndex = 0;

      // ── 5. Extract + normalize features ──
      extractFeatures();

      // ── 6. SVM inference ──
      lastClass = svmPredict();

      // ── 7. Print to serial monitor ──
      printResult();
    }
  }

  // ── 8. LoRa TX on 1s interval (independent of window) ──
  if (millis() - lastTxMs >= TX_INTERVAL_MS) {
    lastTxMs = millis();
    sendLoRaPacket();
  }
}

// ════════════════════════════════════════════
//  FEATURE EXTRACTION
//  MAV · RMS · WL · ZC · SSC · IEMG
//  then normalize each to [0,1]
// ════════════════════════════════════════════
void extractFeatures() {
  float sum_abs = 0, sum_sq = 0, wl = 0, iemg = 0;
  int   zc = 0, ssc = 0;

  for (int i = 0; i < WINDOW_SIZE; i++) {
    float v = emgWindow[i];
    sum_abs += abs(v);
    sum_sq  += v * v;
    iemg    += abs(v);

    if (i > 0) {
      wl += abs(v - emgWindow[i-1]);
      if (i > 1) {
        // Zero crossing with noise threshold
        if (((emgWindow[i-1] > 0) != (v > 0)) && abs(v - emgWindow[i-1]) >= 0.5)
          zc++;
        // Slope sign change with noise threshold
        float d1 = emgWindow[i-1] - emgWindow[i-2];
        float d2 = v              - emgWindow[i-1];
        if (((d1 > 0) != (d2 > 0)) && abs(d2 - d1) >= 0.5)
          ssc++;
      }
    }
  }

  float raw[NUM_FEATURES] = {
    sum_abs / WINDOW_SIZE,      // MAV
    sqrt(sum_sq / WINDOW_SIZE), // RMS
    wl,                         // WL
    (float)zc,                  // ZC
    (float)ssc,                 // SSC
    iemg                        // IEMG
  };

  float bounds_min[NUM_FEATURES] = { min_MAV, min_RMS, min_WL, min_ZC, min_SSC, min_IEMG };
  float bounds_max[NUM_FEATURES] = { max_MAV, max_RMS, max_WL, max_ZC, max_SSC, max_IEMG };

  for (int i = 0; i < NUM_FEATURES; i++) {
    float n = (bounds_max[i] > bounds_min[i])
              ? (raw[i] - bounds_min[i]) / (bounds_max[i] - bounds_min[i])
              : 0;
    lastFeatures[i] = constrain(n, 0.0f, 1.0f);
  }
}

// ════════════════════════════════════════════
//  SVM  — linear kernel, one-vs-rest
//  score = w · x + b  → argmax
// ════════════════════════════════════════════
uint8_t svmPredict() {
  float maxScore = -1e9;
  uint8_t best   = 0;

  for (int c = 0; c < NUM_CLASSES; c++) {
    float score = svm_b[c];
    for (int f = 0; f < NUM_FEATURES; f++)
      score += svm_w[c][f] * lastFeatures[f];
    if (score > maxScore) { maxScore = score; best = c; }
  }
  return best;
}

// ════════════════════════════════════════════
//  LORA TX
//  Packet (12 bytes):
//   [0]    class (0/1/2)
//   [1]    battery %
//   [2–5]  feat_MAV  (float)
//   [6–9]  feat_RMS  (float)
//   [10-11] battery mV (uint16)
// ════════════════════════════════════════════
void sendLoRaPacket() {
  uint16_t battMv = readBatteryMv();
  uint8_t  battPct = (uint8_t)constrain(map(battMv, 3300, 4200, 0, 100), 0, 100);

  uint8_t pkt[12];
  pkt[0] = lastClass;
  pkt[1] = battPct;
  memcpy(&pkt[2], &lastFeatures[0], 4);   // MAV
  memcpy(&pkt[6], &lastFeatures[1], 4);   // RMS
  pkt[10] = (uint8_t)(battMv >> 8);
  pkt[11] = (uint8_t)(battMv & 0xFF);

  Radio.Send(pkt, sizeof(pkt));

  Serial.print("[LoRa TX] Class=");
  Serial.print(lastClass);
  Serial.print("  Batt=");
  Serial.print(battMv);
  Serial.println("mV");
}

uint16_t readBatteryMv() {
  int raw = analogRead(VBAT_PIN);
  return (uint16_t)((raw / 4095.0) * 3.3 * VBAT_SCALE * 1000);
}

// ════════════════════════════════════════════
//  SERIAL DEBUG
// ════════════════════════════════════════════
void printResult() {
  Serial.print("[FEATURES] MAV=");  Serial.print(lastFeatures[0], 3);
  Serial.print(" RMS=");            Serial.print(lastFeatures[1], 3);
  Serial.print(" WL=");             Serial.print(lastFeatures[2], 3);
  Serial.print(" ZC=");             Serial.print(lastFeatures[3], 3);
  Serial.print(" SSC=");            Serial.print(lastFeatures[4], 3);
  Serial.print(" IEMG=");           Serial.println(lastFeatures[5], 3);
  Serial.print("[PREDICT]  ");
  Serial.println(classLabel[lastClass]);
  Serial.println("─────────────────────────────");
}

// ════════════════════════════════════════════
//  BUTTERWORTH BANDPASS FILTER  20–40 Hz @ 500 Hz
//  Coefficients from SIGNAL_SMOOTHING.ino (unchanged)
// ════════════════════════════════════════════
float EMGFilter(float input) {
  float output = input;
  {
    static float z1, z2;
    float x = output - -1.5610180758007182 * z1 - 0.6413515380575631 * z2;
    output = 0.020083365564211236 * x + 0.04016673112842247 * z1 + 0.020083365564211236 * z2;
    z2 = z1; z1 = x;
  }
  {
    static float z1, z2;
    float x = output - -1.1429805025399011 * z1 - 0.4128015980961887 * z2;
    output = 1.0 * x + -2.0 * z1 + 1.0 * z2;
    z2 = z1; z1 = x;
  }
  return output;
}
