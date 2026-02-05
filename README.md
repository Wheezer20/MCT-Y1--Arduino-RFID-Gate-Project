# MCT-Y1--Arduino-RFID-Gate-Project
# Arduino RFID Gate Project

For my first semster project in my Multimedia and Creative Technologies program, I choose to make a small scale implementation of an automated gate prototype built with Arduino that detects a person, reads an RFID card, and opens a servo-driven gate while providing feedback through an LCD, LEDs, and a buzzer. While not all of my componenets are particually difficult to utilize, having everything come together, and getting a better sense of hardware thanks to the hands-on approach I took, this is a good starter project.

This project demonstrates some non-blocking Arduino code, sensor integration, and modular hardware control.

---

## Components Used

- Arduino Uno
- HC-SR04 Ultrasonic Sensor
- MFRC522 RFID Reader
- Servo Motor (SG90 / SM-S2309S)
- I2C 16x2 LCD Display
- LEDs (status indicators)
- Buzzer
- Breadboard + jumper wires

---

## How It Works

1. The ultrasonic sensor detects when someone approaches the gate.
2. The LCD prompts the user to scan their RFID card.
3. If the card is authorized:
   - Green LED turns on
   - Buzzer gives feedback
   - Servo opens the gate
4. After a delay, the gate closes automatically.
**
The system is written using `millis()` timers (no `delay()`), for smooth and responsive behavior, as purely using 'delay()' would cause code breaking early or not running as intented.**
---
