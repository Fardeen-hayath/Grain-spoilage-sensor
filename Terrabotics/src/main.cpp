#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ESP32Servo.h>

// --- Pin Definitions ---
#define ONE_WIRE_BUS      4   // DS18B20 Temp Data
#define TRIG_PIN          5   // HC-SR04 Trigger
#define ECHO_PIN          18  // HC-SR04 Echo
#define MQ135_PIN         34  // Gas Potentiometer (ADC1)
#define MOISTURE_PIN      32  // Moisture Potentiometer (ADC1)
#define SERVO_PIN         13  // Exhaust Damper Servo
#define RELAY_PIN         27  // Blower Fan Relay
#define ALERT_LED_PIN     26  // Critical Warning LED
#define BUZZER_PIN        25  // Alarm Buzzer

// --- Display Configuration ---
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT  64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// --- Sensor & Actuator Objects ---
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature tempSensor(&oneWire);
Servo damperServo;

// --- Safety Thresholds ---
const int MOISTURE_CRITICAL = 3072; // ~75% ADC value
const int GAS_WARNING       = 2500; // ~60% Gas Concentration ADC
const float TEMP_WARNING    = 35.0; // 35°C Hotspot threshold

// --- Non-blocking Timing Variables ---
unsigned long lastSensorRead = 0;
unsigned long lastBlink      = 0;
const long SENSOR_INTERVAL   = 1000; // Read sensors every 1 second
const long BLINK_INTERVAL    = 200;  // Alarm LED blink rate (200ms)

bool ledState = false;

// --- Helper: Read Ultrasonic Distance in cm ---
float getSiloStockLevel() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000); // 30ms timeout
  if (duration == 0) return 0.0;
  return (duration * 0.0343) / 2.0;
}

void setup() {
  Serial.begin(115200);

  // Pin Modes
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(ALERT_LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  // Initialize Relay and LED to OFF
  digitalWrite(RELAY_PIN, LOW);
  digitalWrite(ALERT_LED_PIN, LOW);

  // Attach Servo
  damperServo.attach(SERVO_PIN);
  damperServo.write(0); // Vents closed initially

  // Initialize Temperature Sensor
  tempSensor.begin();

  // Initialize OLED Display
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(10, 20);
  display.println("Smart Silo Booting...");
  display.display();
  delay(1500);
}

void loop() {
  unsigned long currentMillis = millis();

  // 1. Periodically read sensors and control fan/servo (every 1 sec)
  if (currentMillis - lastSensorRead >= SENSOR_INTERVAL) {
    lastSensorRead = currentMillis;

    // Read Sensors
    tempSensor.requestTemperatures();
    float coreTemp = tempSensor.getTempCByIndex(0);
    float headspaceDist = getSiloStockLevel();
    int gasLevel = analogRead(MQ135_PIN);
    int moistureLevel = analogRead(MOISTURE_PIN);

    // Calculate Percentages for OLED Output
    int moisturePct = map(moistureLevel, 0, 4095, 0, 100);
    int gasPct      = map(gasLevel, 0, 4095, 0, 100);

    // Aeration Logic (Fan + Servo Damper)
    bool hazardDetected = (gasLevel >= GAS_WARNING) || (coreTemp >= TEMP_WARNING);
    
    if (hazardDetected) {
      digitalWrite(RELAY_PIN, HIGH); // Turn ON Aeration Fan
      damperServo.write(90);          // Open Exhaust Vents
    } else {
      digitalWrite(RELAY_PIN, LOW);  // Turn OFF Fan
      damperServo.write(0);           // Close Vents
    }

    // Print Telemetry to Serial Monitor
    Serial.printf("Temp: %.1fC | Gas ADC: %d (%d%%) | Moisture ADC: %d (%d%%) | Headspace: %.1f cm | Fan: %s\n",
                  coreTemp, gasLevel, gasPct, moistureLevel, moisturePct, headspaceDist, hazardDetected ? "ON" : "OFF");

    // Update OLED Display
    display.clearDisplay();
    display.setCursor(0, 0);
    display.print("--- SILO STATUS ---");
    
    display.setCursor(0, 16);
    display.printf("Core Temp : %.1f C", coreTemp);
    
    display.setCursor(0, 28);
    display.printf("Gas Level : %d%%", gasPct);
    
    display.setCursor(0, 40);
    display.printf("Moisture  : %d%%", moisturePct);

    display.setCursor(0, 52);
    display.printf("Stock Dist: %.0f cm", headspaceDist);

    display.display();
  }

  // 2. Critical Moisture Alert Logic (Fast Non-Blocking Flashing)
  int currentMoisture = analogRead(MOISTURE_PIN);
  if (currentMoisture >= MOISTURE_CRITICAL) {
    if (currentMillis - lastBlink >= BLINK_INTERVAL) {
      lastBlink = currentMillis;
      ledState = !ledState;
      digitalWrite(ALERT_LED_PIN, ledState ? HIGH : LOW);
      
      if (ledState) {
        tone(BUZZER_PIN, 1500); // 1.5 kHz tone
      } else {
        noTone(BUZZER_PIN);
      }
    }
  } else {
    digitalWrite(ALERT_LED_PIN, LOW);
    noTone(BUZZER_PIN);
  }
}