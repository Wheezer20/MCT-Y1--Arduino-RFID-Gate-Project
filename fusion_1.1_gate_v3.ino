// -  Kristopher Montgomery 2025/2026

#include <SPI.h>
#include <MFRC522.h>
#include <Servo.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

//Pins

// Ultrasonic (HC-SR04)
const int trigPin = 5;
const int echoPin = 6;

// Servo (gate)
const int servoPin = 7;

// LEDs
const int redLedPin   = 4;
const int greenLedPin = 3;

// Buzzer
const int buzzerPin = 8;

// RFID (RC522)
const byte RST_PIN = 9;
const byte SS_PIN  = 10;

//Hardware (Servo and LCD Monitor)
MFRC522 mfrc522(SS_PIN, RST_PIN);
Servo gateServo;

// LCD (I2C)
const uint8_t LCD_ADDR = 0x27;   // if blank lcd
LiquidCrystal_I2C lcd(LCD_ADDR, 16, 2);

// Settings 

const int DETECT_DISTANCE = 25;              // cm
const unsigned long RFID_TIMEOUT = 5000;     // ms
const unsigned long GATE_OPEN_TIME = 5000;   // ms
const unsigned long RFID_RETRY_DELAY = 5000; // ms
const unsigned long COME_CLOSER_INTERVAL = 7000; // ms


//State

struct GateState {
  bool isOpen;
  unsigned long openedAt;
};

struct RFIDState {
  bool awaiting;
  unsigned long startTime;
  unsigned long cooldownUntil;
};

//predictable starting state
GateState gate = {false, 0};
RFIDState rfid = {false, 0, 0};

int entryCount = 0;

// store latest distance so LCD can show it from one place
long lastDistanceCm = -1;

// LCD Helpers

// Print two lines safely to 16x2
void lcdShow(const String& line1, const String& line2 = "") {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(line1.substring(0, 16));
  lcd.setCursor(0, 1);
  lcd.print(line2.substring(0, 16));
}

// (prevents flicker during text change; recycled code from game timer)
void lcdShowIfChanged(const String& line1, const String& line2 = "") {
  static String last1 = "", last2 = "";
  if (line1 == last1 && line2 == last2) return; //if lines are identical, do nothing
  last1 = line1; last2 = line2;
  lcdShow(line1, line2);
}

void updateLCD() {
  if (rfid.awaiting) {
    lcdShowIfChanged("Awaiting RFID", "Scan tag...");
    return;
  }

  if (gate.isOpen) {
    lcdShowIfChanged("GATE: OPEN", "Welcome!");
    return;
  }

  // Gate closed idle screen
  if (lastDistanceCm > 0) {
    lcdShowIfChanged("GATE: CLOSED", "Dist: " + String(lastDistanceCm) + "cm");
  } else {
    lcdShowIfChanged("GATE: CLOSED", "Dist: ---");
  }
}

// Ultrasonic 

long readDistanceCm() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);

  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH, 50000); // 50ms timeout
  if (duration == 0) return -1;
  return duration * 0.034 / 2;  //time to distance formula
}



// Sounds

void playSuccessRiff() {
  tone(buzzerPin, 600, 150);
  delay(100);
  tone(buzzerPin, 800, 150);
  delay(100);
  tone(buzzerPin, 700, 150);
  delay(150);
  noTone(buzzerPin);
}

void playErrorBeep() {
  tone(buzzerPin, 300, 250);
  delay(150);
  noTone(buzzerPin);
}

// Gate control 
void openGate() {
  gateServo.write(90);
  gate.isOpen = true;
  gate.openedAt = millis();

  digitalWrite(redLedPin, LOW);
  digitalWrite(greenLedPin, HIGH);

  Serial.println("Gate OPEN.");
}

void closeGate() {
  gateServo.write(0);
  gate.isOpen = false;

  digitalWrite(greenLedPin, LOW);
  digitalWrite(redLedPin, HIGH);

  Serial.println("Gate CLOSED.");
}

void updateGateAutoClose() {
  if (gate.isOpen && (millis() - gate.openedAt >= GATE_OPEN_TIME)) {
    Serial.println("Auto-closing gate.");
    closeGate();
  }
}

// RFID authorization

bool isAuthorizedCard(byte *uid, byte uidSize) {
  // Replace with your real UID(s)
  byte allowedUID[] = {0x53, 0xDA, 0x50, 0x06};

  if (uidSize != sizeof(allowedUID)) return false;
  for (byte i = 0; i < uidSize; i++) {
    if (uid[i] != allowedUID[i]) return false;
  }
  return true;
}

// Person detection 

void updatePersonDetection() {
  long distance = readDistanceCm();

  lastDistanceCm = distance; // for LCD/UI    

  static unsigned long lastCloserMsg = 0;

// If something is seen, but it's too far away
if (!gate.isOpen &&
    !rfid.awaiting &&
    distance > DETECT_DISTANCE &&
    distance > 0) {

  if (millis() - lastCloserMsg >= COME_CLOSER_INTERVAL) {
    Serial.println("Gate detected. Please come closer.");
    lastCloserMsg = millis();
  }
}

  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 7000) {  //distance prints once every 7 seconds
    Serial.print("Distance: ");
    Serial.print(distance);
    Serial.println(" cm");
    lastPrint = millis();
  }

  // Only trigger when gate is closed, not already waiting RFID, valid distance,
  // and not in cooldown after invalid tag
  if (!gate.isOpen &&
      !rfid.awaiting &&
      distance > 0 && distance < DETECT_DISTANCE &&
      millis() >= rfid.cooldownUntil) {

    rfid.awaiting = true;
    rfid.startTime = millis();

    Serial.println("Person detected. Please scan RFID.");
  }
}

// RFID handling 

void updateRFID() {
  if (!rfid.awaiting) return;

  unsigned long now = millis();

  // Timeout if no tag scanned
  if (now - rfid.startTime > RFID_TIMEOUT) {
    Serial.println("Awaiting tag scan...");
    rfid.awaiting = false;
    return;
  }

  // Wait for a card
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
    return;
  }

  // Validate
  if (isAuthorizedCard(mfrc522.uid.uidByte, mfrc522.uid.size)) {
    Serial.println("VALID RFID tag.");
    rfid.awaiting = false;

    entryCount++;
    Serial.print("Access granted. Entry count: ");
    Serial.println(entryCount);

    openGate();
    playSuccessRiff();
  } else {
    Serial.println("INVALID RFID tag.");
    rfid.awaiting = false;

    playErrorBeep();
    rfid.cooldownUntil = millis() + RFID_RETRY_DELAY; //ignores new attempts
  }

  mfrc522.PICC_HaltA(); 
  mfrc522.PCD_StopCrypto1();
}

// Setup & Loop 

void setup() {
  Serial.begin(9600);

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  pinMode(redLedPin, OUTPUT);
  pinMode(greenLedPin, OUTPUT);

  pinMode(buzzerPin, OUTPUT);

  gateServo.attach(servoPin);

  // LCD (I2C: SDA=A4, SCL=A5 on UNO)
  Wire.begin();
  lcd.init();
  lcd.backlight();
  lcdShow("Booting...", "Gate system");
  delay(3000);

  // RFID
  SPI.begin();
  mfrc522.PCD_Init();
  Serial.println("RC522 ready.");

  closeGate();
  lcdShow("Gate ready", "Waiting...");
}

void loop() {
  updateGateAutoClose();
  updatePersonDetection();
  updateRFID();

  updateLCD();
  delay(150);
}
