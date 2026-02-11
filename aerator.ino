#include <SoftwareSerial.h>

// ---------------------------
// GSM Setup
// ---------------------------
SoftwareSerial GSM(2, 3); // RX, TX for SIM800A
String incomingSMS = "";

// ---------------------------
// Sensor Pins
// ---------------------------
const int pHSensorPin = A0;
const int TDSSensorPin = A1;
const int LDRSensorPin = A2;

// ---------------------------
// Timing for Maintenance Reminder
// ---------------------------
unsigned long lastCheckTime = 0;
const unsigned long maintenanceInterval = 20 * 1000; // 20 seconds for testing (~48h in real scenario)
float lastpH = 0, lastTDS = 0;

// ---------------------------
// Thresholds (Aquaculture Values)
// ---------------------------
float pH_normal_min = 6.5;
float pH_normal_max = 8.5;
float pH_warning_min = 6.0;
float pH_warning_max = 9.0;

float TDS_normal_min = 300;
float TDS_normal_max = 500;
float TDS_warning_min = 500;
float TDS_warning_max = 800;

// LDR optional thresholds
int LDR_normal = 200;
int LDR_warning = 100;

// ---------------------------
// Function Declarations
// ---------------------------
float readPH();
float readTDS();
int readLDR();
String checkCondition(float value, float normalMin, float normalMax, float warnMin, float warnMax);
int calculateWHI(float pHValue, float TDSValue);
void sendSMS(String number, String message);
void processSMS(String sms);

// ---------------------------
// Setup
// ---------------------------
void setup() {
  Serial.begin(9600);
  GSM.begin(9600);
  Serial.println("System Initialized");
  delay(1000);

  // Optional: initial SMS to confirm system is online
  // sendSMS("YOUR_PHONE_NUMBER", "Water Monitoring System Started");
}

// ---------------------------
// Main Loop
// ---------------------------
void loop() {
  // --- Read Sensors ---
  float pHValue = readPH();
  float TDSValue = readTDS();
  int LDRValue = readLDR();

  Serial.print("pH: "); Serial.print(pHValue);
  Serial.print(" | TDS: "); Serial.print(TDSValue);
  Serial.print(" | LDR: "); Serial.println(LDRValue);

  // --- Check Conditions ---
  String pHCondition = checkCondition(pHValue, pH_normal_min, pH_normal_max, pH_warning_min, pH_warning_max);
  String TDSCondition = checkCondition(TDSValue, TDS_normal_min, TDS_normal_max, TDS_warning_min, TDS_warning_max);
  String LDRCondition = (LDRValue > LDR_normal) ? "Normal" : ((LDRValue > LDR_warning) ? "Warning" : "Critical");

  // --- Send Critical SMS if any sensor is Critical ---
  if (pHCondition == "Critical" || TDSCondition == "Critical" || LDRCondition == "Critical") {
    String alertMsg = "ALERT! Water Condition Critical\n";
    alertMsg += "pH: " + String(pHValue) + " (" + pHCondition + ")\n";
    alertMsg += "TDS: " + String(TDSValue) + " (" + TDSCondition + ")\n";
    alertMsg += "LDR: " + String(LDRValue) + " (" + LDRCondition + ")";
    sendSMS("8144119988", alertMsg);
    delay(5000); // avoid spamming
  }

  // --- Maintenance Reminder (simulate 48h with shorter time) ---
  if (millis() - lastCheckTime > maintenanceInterval) {
    if (abs(pHValue - lastpH) < 0.05 && abs(TDSValue - lastTDS) < 10) {
      sendSMS("8144119988", "Maintenance Reminder: Sensor readings stable. Please check or clean probes.");
    }
    lastpH = pHValue;
    lastTDS = TDSValue;
    lastCheckTime = millis();
  }

  // --- Check for incoming SMS ---
  if (GSM.available()) {
    char c = GSM.read();
    if (c == '\n' || c == '\r') {
      if (incomingSMS.length() > 0) {
        processSMS(incomingSMS);
        incomingSMS = "";
      }
    } else {
      incomingSMS += c;
    }
  }

  delay(5000); // loop delay
}

// ---------------------------
// Sensor Reading Functions
// ---------------------------
float readPH() {
  int analogValue = analogRead(pHSensorPin);
  // Convert analog reading to pH (assumes 0-5V, 0-1023)
  float voltage = analogValue * (5.0 / 1023.0);
  float pHValue = 3.5 * voltage + 0.0; // Adjust calibration as per your pH module
  return pHValue;
}

float readTDS() {
  int analogValue = analogRead(TDSSensorPin);
  float voltage = analogValue * (5.0 / 1023.0);
  float TDSValue = (voltage * 500.0); // Adjust based on TDS module scaling
  return TDSValue;
}

int readLDR() {
  int value = analogRead(LDRSensorPin);
  return value;
}

// ---------------------------
// Condition Check Function
// ---------------------------
String checkCondition(float value, float normalMin, float normalMax, float warnMin, float warnMax) {
  if (value < normalMin || value > normalMax) return "Critical";
  else if (value < warnMin || value > warnMax) return "Warning";
  else return "Normal";
}

// ---------------------------
// Water Health Index
// ---------------------------
int calculateWHI(float pHValue, float TDSValue) {
  int score = 100;
  // pH penalties
  if (pHValue < pH_normal_min || pHValue > pH_normal_max) score -= 30;
  else if (pHValue < pH_warning_min || pHValue > pH_warning_max) score -= 15;

  // TDS penalties
  if (TDSValue < TDS_normal_min || TDSValue > TDS_normal_max) score -= 30;
  else if (TDSValue < TDS_warning_min || TDSValue > TDS_warning_max) score -= 15;

  if (score < 0) score = 0;
  return score;
}

// ---------------------------
// SMS Functions
// ---------------------------
void sendSMS(String number, String message) {
  GSM.print("AT+CMGF=1\r"); // Text mode
  delay(100);
  GSM.print("AT+CMGS=\"" + number + "\"\r");
  delay(100);
  GSM.print(message);
  delay(100);
  GSM.write(26); // CTRL+Z to send
  delay(5000);
}

void processSMS(String sms) {
  sms.trim();
  sms.toUpperCase();
  if (sms.indexOf("STATUS") >= 0) {
    float pHValue = readPH();
    float TDSValue = readTDS();
    int whi = calculateWHI(pHValue, TDSValue);
    String statusMsg = "STATUS:\n";
    statusMsg += "pH: " + String(pHValue) + "\n";
    statusMsg += "TDS: " + String(TDSValue) + "\n";
    statusMsg += "Water Health Index: " + String(whi) + "/100";
    sendSMS("8144119988", statusMsg);
  } 
  // MOTOR ON / OFF commands ignored in current setup
}