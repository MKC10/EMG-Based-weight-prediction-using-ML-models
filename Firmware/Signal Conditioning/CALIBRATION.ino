// ─────────────────────────────────────────────────────────────
// CALIBRATION.ino
// Run this ONCE per subject before using SVM_INFERENCE.ino
//
// What it does:
//   - Prompts you to hold each weight class for 5 seconds
//   - Collects EMG windows, extracts features
//   - Prints min/max per feature to Serial
//   - Copy those numbers into FEATURE_EXTRACTION.ino's
//     normalization bounds section
//
// Weight classes (match your training labels):
//   0 = 0–2 kg
//   1 = 2–5 kg
//   2 = 5+ kg
// ─────────────────────────────────────────────────────────────

#define EMG_PIN      7
#define SAMPLE_RATE  500
#define BAUD_RATE    115200
#define WINDOW_SIZE  100
#define MA_WINDOW    15
#define NUM_CLASSES  3
#define WINDOWS_PER_CLASS 25   // 25 × 200ms = 5 seconds per class

float maBuffer[MA_WINDOW];
int   maIndex = 0;
float maSum   = 0;

float window[WINDOW_SIZE];
int   windowIndex = 0;
int   windowsCollected = 0;
int   currentClass = 0;
bool  collecting = false;

// Running min/max across all classes
float min_MAV  = 1e9, max_MAV  = -1e9;
float min_RMS  = 1e9, max_RMS  = -1e9;
float min_WL   = 1e9, max_WL   = -1e9;
float min_ZC   = 1e9, max_ZC   = -1e9;
float min_SSC  = 1e9, max_SSC  = -1e9;
float min_IEMG = 1e9, max_IEMG = -1e9;

const char* classNames[NUM_CLASSES] = {"0–2 kg", "2–5 kg", "5+ kg"};

void setup() {
  Serial.begin(BAUD_RATE);
  analogReadResolution(12);
  delay(2000);
  Serial.println("=== CALIBRATION MODE ===");
  Serial.println("Hold each weight when prompted. Keep muscle contracted.");
  promptNextClass();
}

void loop() {
  // Wait for ENTER to start each class
  if (!collecting) {
    if (Serial.available()) {
      Serial.read();   // consume the keypress
      collecting = true;
      windowsCollected = 0;
      windowIndex = 0;
      Serial.print("Recording ");
      Serial.print(classNames[currentClass]);
      Serial.println("...");
    }
    return;
  }

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

    maSum -= sq(maBuffer[maIndex]);
    maBuffer[maIndex] = filtered;
    maSum += sq(filtered);
    maIndex = (maIndex + 1) % MA_WINDOW;
    float smoothed = sqrt(maSum / MA_WINDOW);

    window[windowIndex++] = smoothed;

    if (windowIndex >= WINDOW_SIZE) {
      windowIndex = 0;
      float mav, rms, wl, zc, ssc, iemg;
      extractFeatures(mav, rms, wl, zc, ssc, iemg);
      updateMinMax(mav, rms, wl, zc, ssc, iemg);
      windowsCollected++;

      if (windowsCollected >= WINDOWS_PER_CLASS) {
        collecting = false;
        Serial.print("Done: ");
        Serial.println(classNames[currentClass]);
        currentClass++;

        if (currentClass >= NUM_CLASSES) {
          printResults();
        } else {
          promptNextClass();
        }
      }
    }
  }
}

void promptNextClass() {
  Serial.println();
  Serial.print(">>> Pick up ");
  Serial.print(classNames[currentClass]);
  Serial.println(" weight and press ENTER when ready...");
}

void extractFeatures(float &mav, float &rms, float &wl,
                     float &zc,  float &ssc, float &iemg) {
  float sum_abs = 0, sum_sq = 0, _wl = 0, _iemg = 0;
  int   _zc = 0, _ssc = 0;

  for (int i = 0; i < WINDOW_SIZE; i++) {
    float v = window[i];
    sum_abs += abs(v);
    sum_sq  += v * v;
    _iemg   += abs(v);
    if (i > 0) {
      _wl += abs(v - window[i-1]);
      if (i > 1) {
        if (((window[i-1] > 0) != (v > 0)) && abs(v - window[i-1]) >= 0.5)
          _zc++;
        float d1 = window[i-1] - window[i-2];
        float d2 = v           - window[i-1];
        if (((d1 > 0) != (d2 > 0)) && abs(d2 - d1) >= 0.5)
          _ssc++;
      }
    }
  }

  mav  = sum_abs / WINDOW_SIZE;
  rms  = sqrt(sum_sq / WINDOW_SIZE);
  wl   = _wl;
  zc   = (float)_zc;
  ssc  = (float)_ssc;
  iemg = _iemg;
}

void updateMinMax(float mav, float rms, float wl,
                  float zc,  float ssc, float iemg) {
  if (mav  < min_MAV)  min_MAV  = mav;   if (mav  > max_MAV)  max_MAV  = mav;
  if (rms  < min_RMS)  min_RMS  = rms;   if (rms  > max_RMS)  max_RMS  = rms;
  if (wl   < min_WL)   min_WL   = wl;    if (wl   > max_WL)   max_WL   = wl;
  if (zc   < min_ZC)   min_ZC   = zc;    if (zc   > max_ZC)   max_ZC   = zc;
  if (ssc  < min_SSC)  min_SSC  = ssc;   if (ssc  > max_SSC)  max_SSC  = ssc;
  if (iemg < min_IEMG) min_IEMG = iemg;  if (iemg > max_IEMG) max_IEMG = iemg;
}

void printResults() {
  Serial.println();
  Serial.println("=== CALIBRATION COMPLETE ===");
  Serial.println("Copy these into FEATURE_EXTRACTION.ino:");
  Serial.println();
  Serial.print("float min_MAV  = "); Serial.print(min_MAV,  4); Serial.println(";");
  Serial.print("float max_MAV  = "); Serial.print(max_MAV,  4); Serial.println(";");
  Serial.print("float min_RMS  = "); Serial.print(min_RMS,  4); Serial.println(";");
  Serial.print("float max_RMS  = "); Serial.print(max_RMS,  4); Serial.println(";");
  Serial.print("float min_WL   = "); Serial.print(min_WL,   4); Serial.println(";");
  Serial.print("float max_WL   = "); Serial.print(max_WL,   4); Serial.println(";");
  Serial.print("float min_ZC   = "); Serial.print(min_ZC,   4); Serial.println(";");
  Serial.print("float max_ZC   = "); Serial.print(max_ZC,   4); Serial.println(";");
  Serial.print("float min_SSC  = "); Serial.print(min_SSC,  4); Serial.println(";");
  Serial.print("float max_SSC  = "); Serial.print(max_SSC,  4); Serial.println(";");
  Serial.print("float min_IEMG = "); Serial.print(min_IEMG, 4); Serial.println(";");
  Serial.print("float max_IEMG = "); Serial.print(max_IEMG, 4); Serial.println(";");
}

// Butterworth BP (20–40 Hz @ 500 Hz) — from SIGNAL_SMOOTHING.ino
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
