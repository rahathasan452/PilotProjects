#include <Arduino.h>
#include <IRremote.hpp> // Requires IRremote library version 4.x or newer

// ==================== PIN DEFINITIONS ====================
#define IR_SEND_PIN    3   // KY-005 'S' pin (Left pin facing module front)
#define STATUS_LED_PIN 13  // Built-in Arduino Uno 'L' LED for blink feedback

// 4x4 Membrane Keypad Ribbon Cable Pins (Digital Pins 4 to 11)
const uint8_t ROW_PINS[4] = {4, 5, 6, 7};   // Pins 1 to 4 on ribbon cable (Rows R1, R2, R3, R4)
const uint8_t COL_PINS[4] = {8, 9, 10, 11}; // Pins 5 to 8 on ribbon cable (Columns C1, C2, C3, C4)

const char KEY_MAP[4][4] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

// ==================== MODE & D-PAD STATE ====================
// Mode 1: Sony Bravia Google TV Smart Navigation (D-Pad, Home, Back, OK, Volume)
// Mode 2: Sony Bravia Number Pad (Digits 0-9 & Channel controls)
uint8_t currentMode = 1;
bool isPaused = false;

// Function Prototypes
char scanKeypad();
void blinkLED(uint8_t times);
void sendSony12(uint8_t cmd, const __FlashStringHelper* label);
void sendSony15(uint8_t addr, uint8_t cmd, const __FlashStringHelper* label);
void sendSonyDPad(char direction);
void handleMode1_Nav(char key);
void handleMode2_Num(char key);

void setup() {
  Serial.begin(115200);
  pinMode(STATUS_LED_PIN, OUTPUT);

  // Configure Keypad Row Pins as OUTPUT (idle HIGH)
  for (uint8_t r = 0; r < 4; r++) {
    pinMode(ROW_PINS[r], OUTPUT);
    digitalWrite(ROW_PINS[r], HIGH);
  }

  // Configure Keypad Column Pins as INPUT with internal pull-up resistors
  for (uint8_t c = 0; c < 4; c++) {
    pinMode(COL_PINS[c], INPUT_PULLUP);
  }

  // Initialize the KY-005 IR Transmitter on Pin 3
  IrSender.begin(IR_SEND_PIN);

  Serial.println(F("=============================================================="));
  Serial.println(F("  SONY BRAVIA GOOGLE TV REMOTE - FIXED D-PAD NAVIGATION CODE  "));
  Serial.println(F("=============================================================="));
  Serial.println(F("Status: Ready!"));
  Serial.println(F("Active Mode: [MODE 1] Smart Navigation (D-Pad, Home, Back, OK)"));
  Serial.println(F("Press '#' on your 4x4 keypad at any time to switch to Mode 2 (0-9)."));
  Serial.println(F("--------------------------------------------------------------"));
}

void loop() {
  char key = scanKeypad();

  if (key != 0) {
    Serial.print(F("[Key Pressed: '"));
    Serial.print(key);
    Serial.print(F("'] | Active Mode: "));
    Serial.print(currentMode);
    Serial.print(F(" -> "));

    // '#' Button toggles between Mode 1 and Mode 2
    if (key == '#') {
      currentMode = (currentMode == 1) ? 2 : 1;

      if (currentMode == 1) {
        Serial.println(F("SWITCHED TO MODE 1: Google TV Smart Navigation"));
        blinkLED(1);
      } else {
        Serial.println(F("SWITCHED TO MODE 2: Number Pad (0-9) & Channels"));
        blinkLED(2);
      }
      return;
    }

    // Route key event based on active mode
    if (currentMode == 1) {
      handleMode1_Nav(key);
    } else {
      handleMode2_Num(key);
    }
  }
}

// Keypad matrix scanner with hardware debounce protection
char scanKeypad() {
  static unsigned long lastPressTime = 0;
  if (millis() - lastPressTime < 220) {
    return 0; // Prevent accidental double-firing
  }

  for (uint8_t r = 0; r < 4; r++) {
    digitalWrite(ROW_PINS[r], LOW);
    delayMicroseconds(20);

    for (uint8_t c = 0; c < 4; c++) {
      if (digitalRead(COL_PINS[c]) == LOW) {
        digitalWrite(ROW_PINS[r], HIGH);
        lastPressTime = millis();
        return KEY_MAP[r][c];
      }
    }
    digitalWrite(ROW_PINS[r], HIGH);
  }
  return 0;
}

// Blinks built-in Pin 13 LED to indicate mode changes
void blinkLED(uint8_t times) {
  for (uint8_t i = 0; i < times; i++) {
    digitalWrite(STATUS_LED_PIN, HIGH);
    delay(120);
    digitalWrite(STATUS_LED_PIN, LOW);
    delay(120);
  }
}

// Standard Sony 12-bit SIRC: Device Address 0x01 (TV), repeat count = 3 (4 frames total)
void sendSony12(uint8_t cmd, const __FlashStringHelper* label) {
  Serial.print(F("Sony 12-bit (Addr 0x01, Cmd 0x"));
  Serial.print(cmd, HEX);
  Serial.print(F("): "));
  Serial.println(label);

  digitalWrite(STATUS_LED_PIN, HIGH);
  IrSender.sendSony(0x01, cmd, 3, 12);
  digitalWrite(STATUS_LED_PIN, LOW);
}

// Extended Sony 15-bit SIRC: repeat count = 3 (4 frames total)
void sendSony15(uint8_t addr, uint8_t cmd, const __FlashStringHelper* label) {
  Serial.print(F("Sony 15-bit (Addr 0x"));
  Serial.print(addr, HEX);
  Serial.print(F(", Cmd 0x"));
  Serial.print(cmd, HEX);
  Serial.print(F("): "));
  Serial.println(label);

  digitalWrite(STATUS_LED_PIN, HIGH);
  IrSender.sendSony(addr, cmd, 3, 15);
  digitalWrite(STATUS_LED_PIN, LOW);
}

// ==================== HYBRID D-PAD NAVIGATION HANDLER ====================
// Transmits the standard Sony 12-bit cursor command followed immediately by
// the Sony 15-bit Android/Google TV smart cursor command so all Bravia models respond
void sendSonyDPad(char direction) {
  digitalWrite(STATUS_LED_PIN, HIGH);

  switch (direction) {
    case 'U': // D-PAD UP
      Serial.println(F("D-PAD UP -> Firing 12-bit (0x74) + 15-bit (0x4F)"));
      IrSender.sendSony(0x01, 0x74, 2, 12); // Standard 12-bit cursor Up
      delay(25);
      IrSender.sendSony(0x97, 0x4F, 2, 15); // Android/Google TV 15-bit cursor Up
      break;

    case 'D': // D-PAD DOWN
      Serial.println(F("D-PAD DOWN -> Firing 12-bit (0x75) + 15-bit (0x50)"));
      IrSender.sendSony(0x01, 0x75, 2, 12); // Standard 12-bit cursor Down
      delay(25);
      IrSender.sendSony(0x97, 0x50, 2, 15); // Android/Google TV 15-bit cursor Down
      break;

    case 'L': // D-PAD LEFT
      Serial.println(F("D-PAD LEFT -> Firing 12-bit (0x34) + 15-bit (0x4D)"));
      IrSender.sendSony(0x01, 0x34, 2, 12); // Standard 12-bit cursor Left
      delay(25);
      IrSender.sendSony(0x97, 0x4D, 2, 15); // Android/Google TV 15-bit cursor Left
      break;

    case 'R': // D-PAD RIGHT
      Serial.println(F("D-PAD RIGHT -> Firing 12-bit (0x33) + 15-bit (0x4E)"));
      IrSender.sendSony(0x01, 0x33, 2, 12); // Standard 12-bit cursor Right
      delay(25);
      IrSender.sendSony(0x97, 0x4E, 2, 15); // Android/Google TV 15-bit cursor Right
      break;

    case 'O': // OK / SELECT / ENTER
      Serial.println(F("OK / ENTER -> Firing 12-bit (0x65) + 15-bit (0x4C) + Alt (0x0B)"));
      IrSender.sendSony(0x01, 0x65, 2, 12); // Standard 12-bit OK
      delay(25);
      IrSender.sendSony(0x97, 0x4C, 2, 15); // Android/Google TV 15-bit Select
      delay(25);
      IrSender.sendSony(0x01, 0x0B, 2, 12); // Alternate Bravia Enter command
      break;
  }

  digitalWrite(STATUS_LED_PIN, LOW);
}

// ==================== MODE 1: SONY BRAVIA SMART NAVIGATION ====================
void handleMode1_Nav(char key) {
  switch (key) {
    // Row 1
    case '1': sendSony12(0x15, F("POWER TOGGLE")); break;
    case '2': sendSonyDPad('U'); break;                 // D-PAD UP
    case '3': sendSony12(0x60, F("GOOGLE TV HOME")); break;
    case 'A': sendSony12(0x12, F("VOLUME UP (+)")); break;

    // Row 2
    case '4': sendSonyDPad('L'); break;                 // D-PAD LEFT
    case '5': sendSonyDPad('O'); break;                 // OK / SELECT (ENTER)
    case '6': sendSonyDPad('R'); break;                 // D-PAD RIGHT
    case 'B': sendSony12(0x13, F("VOLUME DOWN (-)")); break;

    // Row 3
    case '7': sendSony15(0x97, 0x23, F("BACK / RETURN")); break;
    case '8': sendSonyDPad('D'); break;                 // D-PAD DOWN
    case '9': sendSony15(0x97, 0x36, F("OPTIONS / QUICK SETTINGS")); break;
    case 'C': sendSony12(0x14, F("MUTE AUDIO")); break;

    // Row 4
    case '*': sendSony12(0x25, F("INPUT / SOURCE SELECT")); break;
    case '0':
      if (!isPaused) {
        sendSony15(0x97, 0x1A, F("PLAY"));
      } else {
        sendSony15(0x97, 0x19, F("PAUSE"));
      }
      isPaused = !isPaused;
      break;
    case 'D': sendSony12(0x10, F("CHANNEL UP (+)")); break;
  }
}

// ==================== MODE 2: DIRECT NUMBER PAD (0-9) ====================
void handleMode2_Num(char key) {
  switch (key) {
    case '1': sendSony12(0x00, F("Digit 1")); break;
    case '2': sendSony12(0x01, F("Digit 2")); break;
    case '3': sendSony12(0x02, F("Digit 3")); break;
    case '4': sendSony12(0x03, F("Digit 4")); break;
    case '5': sendSony12(0x04, F("Digit 5")); break;
    case '6': sendSony12(0x05, F("Digit 6")); break;
    case '7': sendSony12(0x06, F("Digit 7")); break;
    case '8': sendSony12(0x07, F("Digit 8")); break;
    case '9': sendSony12(0x08, F("Digit 9")); break;
    case '0': sendSony12(0x09, F("Digit 0")); break;

    case '*': sendSony12(0x65, F("OK / CONFIRM")); break;
    case 'A': sendSony12(0x12, F("VOLUME UP (+)")); break;
    case 'B': sendSony12(0x13, F("VOLUME DOWN (-)")); break;
    case 'C': sendSony12(0x10, F("CHANNEL UP (+)")); break;
    case 'D': sendSony12(0x11, F("CHANNEL DOWN (-)")); break;
  }
}