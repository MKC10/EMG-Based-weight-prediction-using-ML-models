// ─────────────────────────────────────────────────────────────
// FEATURE_EXTRACTION.ino
// Extracts 6 time-domain EMG features from a 200ms window.
// Feeds directly into SVM_INFERENCE.ino
//
// Matches: EMG_DATAFILTERING.ino + SIGNAL_SMOOTHING.ino
//   - Same pin: GPIO 7
//   - Same ADC: 12-bit, 500 Hz
//   - Same filter: Butterworth BP (20–40 Hz) + RMS envelope
// ─────────────────────────────────────────────────────────────

#define EMG_PIN      7
#define SAMPLE_RATE  500
#define BAUD_RATE    115200

// 200ms window @ 500 Hz = 100 samples
#define WINDOW_SIZE  100

// RMS smoothing (same as SIGNAL_SMOOTHING.ino)
#define MA_WINDOW    15

float maBuffer[MA_WINDOW];
int   maIndex = 0;
float maSum   = 0;

float window[WINDOW_SIZE];
int   windowIndex = 0;

// ── Features (populated by extractFeatures) ──
float feat_MAV  = 0;   // Mean Absolute Value
float feat_RMS  = 0;   // Root Mean Square
float feat_WL   = 0;   // Waveform Length
float feat_ZC   = 0;   // Zero Crossing count
float feat_SSC  = 0;   // Slope Sign Changes
float feat_IEMG = 0;   // Integrated EMG

// ── Normalization bounds (from your calibration session) ──
// Replace these with values from CALIBRATION.ino output
float min_MAV  = 0,    max_MAV  = 200;
float min_RMS  = 0,    max_RMS  = 200;
float min_WL   = 0,    max_WL   = 5000;
float min_ZC   = 0,    max_ZC   = 50;
float min_SSC  = 0,    max_SSC  = 50;
float min_IEMG = 0,    max_IEMG = 20000;

void setup() {
  Serial.begin(BAUD_RATE);
  analogReadResolution(12);
  delay(2000);
  Serial.println("Feature extraction ready.");
}

void loop() {
  static unsigned long past = 0;
  unsigned long present  = micros();
  unsigned long interval = present - past;
  past = present;

  static long timer = 0;
  timer -= interval;

  if (timer < 0) {
    timer += 1000000 / SAMPLE_RATE;

    float raw      = analogRead(EMG_PIN);
    float filtered = EMGFilter(raw);
    if (filtered < 0) filtered = 0;

    // RMS envelope (same as SIGNAL_SMOOTHING.ino)
    maSum -= sq(maBuffer[maIndex]);
    maBuffer[maIndex] = filtered;
    maSum += sq(filtered);
    maIndex = (maIndex + 1) % MA_WINDOW;
    float smoothed = sqrt(maSum / MA_WINDOW);

    // Fill window
    window[windowIndex++] = smoothed;

    if (windowIndex >= WINDOW_SIZE) {
      windowIndex = 0;
      extractFeatures();
      printFeatures();   // sends to Serial — wire to SVM_INFERENCE.ino
    }
  }
}

void extractFeatures() {
  float sum_abs = 0, sum_sq = 0, wl = 0, iemg = 0;
  int   zc = 0, ssc = 0;

  for (int i = 0; i < WINDOW_SIZE; i++) {
    float v = window[i];
    sum_abs += abs(v);
    sum_sq  += v * v;
    iemg    += abs(v);

    if (i > 0) {
      wl += abs(v - window[i - 1]);

      // Zero crossing — needs sign change AND amplitude above noise floor
      if (i > 1) {
        if (((window[i-1] > 0) != (v > 0)) && abs(v - window[i-1]) >= 0.5)
          zc++;

        // Slope sign change
        float d1 = window[i-1] - window[i-2];
        float d2 = v           - window[i-1];
        if (((d1 > 0) != (d2 > 0)) && abs(d2 - d1) >= 0.5)
          ssc++;
      }
    }
  }

  feat_MAV  = sum_abs / WINDOW_SIZE;
  feat_RMS  = sqrt(sum_sq / WINDOW_SIZE);
  feat_WL   = wl;
  feat_ZC   = (float)zc;
  feat_SSC  = (float)ssc;
  feat_IEMG = iemg;

  // Normalize each feature to [0, 1] using calibration bounds
  feat_MAV  = normalize(feat_MAV,  min_MAV,  max_MAV);
  feat_RMS  = normalize(feat_RMS,  min_RMS,  max_RMS);
  feat_WL   = normalize(feat_WL,   min_WL,   max_WL);
  feat_ZC   = normalize(feat_ZC,   min_ZC,   max_ZC);
  feat_SSC  = normalize(feat_SSC,  min_SSC,  max_SSC);
  feat_IEMG = normalize(feat_IEMG, min_IEMG, max_IEMG);
}

float normalize(float val, float mn, float mx) {
  if (mx == mn) return 0;
  float n = (val - mn) / (mx - mn);
  if (n < 0) n = 0;
  if (n > 1) n = 1;
  return n;
}

void printFeatures() {
  // CSV format: MAV,RMS,WL,ZC,SSC,IEMG
  // SVM_INFERENCE.ino reads this line from Serial
  Serial.print(feat_MAV,  4); Serial.print(",");
  Serial.print(feat_RMS,  4); Serial.print(",");
  Serial.print(feat_WL,   4); Serial.print(",");
  Serial.print(feat_ZC,   4); Serial.print(",");
  Serial.print(feat_SSC,  4); Serial.print(",");
  Serial.println(feat_IEMG, 4);
}

// Butterworth Band-Pass Filter (20–40 Hz @ 500 Hz)
// Exact coefficients from SIGNAL_SMOOTHING.ino
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
