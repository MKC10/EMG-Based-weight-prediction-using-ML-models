// ─────────────────────────────────────────────────────────────
// LORA_TX.ino
// Transmits EMG inference result over LoRa (Heltec LoRa32 V3)
//
// Reads predicted class + feature data from Serial
// (output of SVM_INFERENCE.ino) and broadcasts a compact
// LoRa packet every 1 second.
//
// Packet format (12 bytes):
//   [0]     uint8  predicted class (0/1/2)
//   [1]     uint8  confidence proxy (0–100)
//   [2–5]   float  feat_MAV  (normalized)
//   [6–9]   float  feat_RMS  (normalized)
//   [10–11] uint16 battery_mv
//
// Board: Heltec WiFi LoRa 32 V3
// Library: Heltec ESP32 (install via Arduino board manager)
// Frequency: 868 MHz (EU) — change to 915E6 for US
// ─────────────────────────────────────────────────────────────

#include "LoRaWan_APP.h"   // Heltec LoRa32 V3 library

#define BAUD_RATE       115200
#define LORA_BAND       868E6     // 868 MHz EU — use 915E6 for US/India
#define LORA_TX_POWER   14        // dBm
#define LORA_SF         7         // Spreading factor (7–12; lower = faster)
#define LORA_BW         125E3     // Bandwidth Hz
#define LORA_CR         5         // Coding rate 4/5
#define TX_INTERVAL_MS  1000      // Transmit every 1 second

// Battery ADC pin (Heltec LoRa32 V3 built-in)
#define VBAT_PIN        1         // ADC1 channel on Heltec V3
#define VBAT_SCALE      4.9       // Voltage divider ratio on V3

// ── State from SVM_INFERENCE ──
uint8_t  predicted_class = 0;
uint8_t  confidence      = 0;
float    feat_MAV        = 0;
float    feat_RMS        = 0;
String   inputLine       = "";
unsigned long lastTx     = 0;

void setup() {
  Serial.begin(BAUD_RATE);
  delay(2000);

  // Init LoRa
  Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);
  Radio.Init(NULL);
  Radio.SetChannel(LORA_BAND);
  Radio.SetTxConfig(
    MODEM_LORA,
    LORA_TX_POWER,
    0,            // FSK freq dev — unused
    LORA_BW,
    LORA_SF,
    LORA_CR,
    8,            // preamble length
    false,        // fixed length
    true,         // CRC on
    0, 0,         // frequency hopping — off
    false,        // IQ inversion
    3000          // TX timeout ms
  );

  Serial.println("LoRa TX ready.");
}

void loop() {
  // Read inference result lines from Serial
  // Expected format: "CLASS:1,CONF:82,MAV:0.45,RMS:0.38"
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      if (inputLine.length() > 0) {
        parseInferenceResult(inputLine);
        inputLine = "";
      }
    } else if (c != '\r') {
      inputLine += c;
    }
  }

  // Transmit on interval
  if (millis() - lastTx >= TX_INTERVAL_MS) {
    lastTx = millis();
    sendLoRaPacket();
  }
}

void parseInferenceResult(String line) {
  // Parse "CLASS:1,CONF:82,MAV:0.45,RMS:0.38"
  predicted_class = parseField(line, "CLASS:").toInt();
  confidence      = (uint8_t)parseField(line, "CONF:").toInt();
  feat_MAV        = parseField(line, "MAV:").toFloat();
  feat_RMS        = parseField(line, "RMS:").toFloat();
}

String parseField(String line, String key) {
  int start = line.indexOf(key);
  if (start == -1) return "0";
  start += key.length();
  int end = line.indexOf(',', start);
  if (end == -1) end = line.length();
  return line.substring(start, end);
}

uint16_t readBatteryMv() {
  int raw = analogRead(VBAT_PIN);
  return (uint16_t)((raw / 4095.0) * 3.3 * VBAT_SCALE * 1000);
}

void sendLoRaPacket() {
  uint16_t batt = readBatteryMv();

  uint8_t packet[12];
  packet[0] = predicted_class;
  packet[1] = confidence;
  memcpy(&packet[2], &feat_MAV, 4);
  memcpy(&packet[6], &feat_RMS, 4);
  packet[10] = (uint8_t)(batt >> 8);
  packet[11] = (uint8_t)(batt & 0xFF);

  Radio.Send(packet, sizeof(packet));

  // Log to serial for debugging
  Serial.print("TX → Class: ");
  Serial.print(predicted_class);
  Serial.print("  Conf: ");
  Serial.print(confidence);
  Serial.print("%  Batt: ");
  Serial.print(batt);
  Serial.println(" mV");
}
