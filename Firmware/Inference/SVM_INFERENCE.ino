// ─────────────────────────────────────────────────────────────
// SVM_INFERENCE.ino
// Reads 6 normalized features from Serial (from FEATURE_EXTRACTION.ino)
// and classifies weight category using a linear SVM.
//
// Weight classes:
//   0 = 0–2 kg  (light)
//   1 = 2–5 kg  (medium)
//   2 = 5+ kg   (heavy)
//
// HOW TO GET YOUR REAL SVM WEIGHTS:
//   1. Collect CSV data using EMG_DATAFILTERING.ino
//   2. Run FEATURE_EXTRACTION.ino, log output to a CSV
//   3. Train in Edge Impulse or Python (sklearn SVM, linear kernel)
//   4. Copy the support vectors + alphas + bias here
//      OR use Edge Impulse EON export and replace this file entirely
//
// The weights below are PLACEHOLDER — replace with your trained values.
// ─────────────────────────────────────────────────────────────

#define BAUD_RATE   115200
#define NUM_FEATURES 6
#define NUM_CLASSES  3

// ── SVM weights (one weight vector per class, OvR scheme) ──
// Format: w[class][feature] — replace with sklearn output
// python: clf.coef_  → shape (n_classes, n_features)
//         clf.intercept_ → shape (n_classes,)
float svm_w[NUM_CLASSES][NUM_FEATURES] = {
  { 2.10, -1.30,  0.80,  0.50, -0.20,  1.60 },   // class 0: 0–2 kg
  {-0.80,  0.40,  1.20, -0.30,  0.90, -0.50 },   // class 1: 2–5 kg
  {-1.30,  0.90, -2.00, -0.20, -0.70, -1.10 }    // class 2: 5+ kg
};

float svm_b[NUM_CLASSES] = { 0.15, -0.10, -0.05 };

const char* classLabel[NUM_CLASSES] = { "Light (0-2 kg)", "Medium (2-5 kg)", "Heavy (5+ kg)" };

// Input feature buffer
float features[NUM_FEATURES];
String inputLine = "";

void setup() {
  Serial.begin(BAUD_RATE);
  delay(2000);
  Serial.println("SVM Inference ready. Waiting for features...");
}

void loop() {
  // Read one CSV line from Serial (sent by FEATURE_EXTRACTION.ino)
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      if (inputLine.length() > 0) {
        if (parseFeatures(inputLine)) {
          int predictedClass = svmPredict();
          printResult(predictedClass);
        }
        inputLine = "";
      }
    } else if (c != '\r') {
      inputLine += c;
    }
  }
}

// Parse "f1,f2,f3,f4,f5,f6" into features[]
bool parseFeatures(String line) {
  int idx = 0;
  int start = 0;
  for (int i = 0; i <= line.length() && idx < NUM_FEATURES; i++) {
    if (i == line.length() || line[i] == ',') {
      features[idx++] = line.substring(start, i).toFloat();
      start = i + 1;
    }
  }
  return idx == NUM_FEATURES;
}

// Linear SVM: score = w · x + b, pick argmax
int svmPredict() {
  float maxScore = -1e9;
  int   bestClass = 0;

  for (int c = 0; c < NUM_CLASSES; c++) {
    float score = svm_b[c];
    for (int f = 0; f < NUM_FEATURES; f++) {
      score += svm_w[c][f] * features[f];
    }
    if (score > maxScore) {
      maxScore = score;
      bestClass = c;
    }
  }
  return bestClass;
}

void printResult(int cls) {
  Serial.print("Features: [");
  for (int i = 0; i < NUM_FEATURES; i++) {
    Serial.print(features[i], 3);
    if (i < NUM_FEATURES - 1) Serial.print(", ");
  }
  Serial.println("]");
  Serial.print("Predicted weight: ");
  Serial.println(classLabel[cls]);
  Serial.println("─────────────────");
}
