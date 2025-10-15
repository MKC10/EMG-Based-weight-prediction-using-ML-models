#define EMG_PIN 7
#define SAMPLE_RATE 500
#define BAUD_RATE 115200

#define MAX_SAMPLES 7000           // 14 sec @ 500 Hz
#define RECORD_DURATION_MS 14000   // Total duration in milliseconds
#define DELAY_BETWEEN_SAMPLES 3000 // Pause before next round

float emgData[MAX_SAMPLES];
int sampleCount = 0;
unsigned long lastSampleTime = 0;
bool recording = false;
unsigned long recordStartTime = 0;
unsigned long cycleStartTime = 0;

void setup() {
  Serial.begin(BAUD_RATE);
  analogReadResolution(12);
  delay(2000);
  Serial.println("EMG logger started. Prepare for lift...");
  delay(DELAY_BETWEEN_SAMPLES);
  recording = true;
  recordStartTime = millis();
}

void loop() {
  // Start new recording automatically after delay
  if (!recording && millis() - cycleStartTime >= DELAY_BETWEEN_SAMPLES) {
    Serial.println("\nRecording for 14 seconds... Lift now!");
    sampleCount = 0;
    recording = true;
    recordStartTime = millis();
  }

  // Sample during recording period
  if (recording && micros() - lastSampleTime >= 1000000 / SAMPLE_RATE) {
    lastSampleTime = micros();

    if (sampleCount < MAX_SAMPLES) {
      int raw = analogRead(EMG_PIN);
      float emg = EMGFilter(raw);
      if (emg < 0) emg = 0;
      if (emg < 30) emg = 0;
      emgData[sampleCount++] = emg;
    }

    // Stop recording after 14 seconds
    if (millis() - recordStartTime >= RECORD_DURATION_MS) {
      recording = false;
      float avgEMG = calculateAverageEMG();

      Serial.println("Recording complete.");
      Serial.println("Enter weight in kg:");

      // Wait for user to enter the weight shown on the dynamometer
      while (!Serial.available()) {}
      String weightInput = Serial.readStringUntil('\n'); 
      weightInput.trim();

      Serial.print("Recorded → EMG: ");
      Serial.print(avgEMG, 2);
      Serial.print(" , Weight: ");
      Serial.println(weightInput);

      Serial.println("\nPrepare for next lift...");
      cycleStartTime = millis();
    }
  }
}

// Butterworth Band-Pass Filter (1–40 Hz @ 500 Hz sample rate)
float EMGFilter(float input) {
  float output = input;
  {
    static float z1, z2;
    float x = output - -1.889033 * z1 - 0.894874 * z2;
    output = 0.00208035 * x + 0.0041607 * z1 + 0.00208035 * z2;
    z2 = z1; z1 = x;
  }
  {
    static float z1, z2;
    float x = output - -1.788477 * z1 - 0.800802 * z2;
    output = 1.000000 * x + -2.000000 * z1 + 1.000000 * z2;
    z2 = z1; z1 = x;
  }
  return output;
}

float calculateAverageEMG() {
  if (sampleCount == 0) return 0;
  float sum = 0;
  for (int i = 0; i < sampleCount; i++) {
    sum += emgData[i];
  }
  return sum / sampleCount;
}
