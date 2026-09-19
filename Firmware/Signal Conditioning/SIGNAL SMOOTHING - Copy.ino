#define SAMPLE_RATE 500
#define BAUD_RATE 115200
#define EMG_PIN 7
#define MA_WINDOW 15
#define EMG_THRESHOLD 0  // Suppress very low noise (baseline)

float maBuffer[MA_WINDOW];
int maIndex = 0;
float maSum = 0;

void setup() {
  Serial.begin(BAUD_RATE);
  analogReadResolution(12);  // 12-bit ADC resolution (0–4095)
  delay(2000);
}

void loop() {
  static unsigned long past = 0;
  unsigned long present = micros();
  unsigned long interval = present - past;
  past = present;

  static long timer = 0;
  timer -= interval;

  if (timer < 0) {
    timer += 1000000 / SAMPLE_RATE;

    float raw = analogRead(EMG_PIN);

    // Apply Butterworth band-pass filter (20–40 Hz)
    float filtered = EMGFilter(raw);

    // Basic threshold to remove low-level background noise
    if (filtered < EMG_THRESHOLD) filtered = 0;

   // RMS envelope (Root Mean Square)
maSum -= sq(maBuffer[maIndex]);
maBuffer[maIndex] = filtered;
maSum += sq(filtered);
maIndex = (maIndex + 1) % MA_WINDOW;
float smoothed = sqrt(maSum / MA_WINDOW);


    // ✅ No gating here — every smoothed value gets shown
    Serial.println(smoothed);

    delay(10);  // Optional: for serial plot stability
  }
}

// Butterworth Band-Pass Filter (20–40 Hz at 500 Hz)
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
