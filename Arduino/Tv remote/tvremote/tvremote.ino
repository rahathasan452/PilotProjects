#include <Arduino.h>
#include <IRremote.hpp> // Requires IRremote v4.x or newer

// Transmitter Pin
#define IR_SEND_PIN    3
#define STATUS_LED_PIN 13

// 4x4 Membrane Keypad Pin Assignments
const uint8_t ROW_PINS[4] = {4, 5, 6, 7};   // R1, R2, R3, R4
const uint8_t COL_PINS[4] = {8, 9, 10, 11}; // C1, C2, C3, C4

const char KEY_MAP[4][4] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

// Current Active Mode: 1 = Navigation, 2 = Number Pad
uint8_t currentMode = 1;
bool isPaused = false;

// Function Declarations
char scanKeypad();
void blinkLED(uint8_t times);
void handleMode1_Nav(char key);
void handleMode2_Num(char key);
void sendSony12(uint8_t cmd, const __FlashStringHelper* label);
void sendSony15(uint8_t addr, uint8_t cmd, const __FlashStringHelper* label);

void setup() {
  Serial.begin(115200);
  pinMode(STATUS_LED_PIN, OUTPUT);

  // Configure Keypad Row pins as OUTPUT (HIGH) and Column pins as INPUT_PULLUP
  for (uint8_t r = 0; r < 4; r++) {
    pinMode(ROW_PINS[r], OUTPUT);
    digitalWrite(ROW_PINS[r], HIGH);
  }
  for (uint8_t c = 0; c < 4; c++) {
    pinMode(COL_PINS[c], INPUT_PULLUP);
  }

  // Initialize IR Transmitter on Pin 3
  IrSender.begin(IR_SEND_PIN);

  Serial.println(F("===================================================="));
  Serial.println(F("   SONY BRAVIA GOOGLE TV DEDICATED REMOTE          "));
  Serial.println(F("===================================================="));
  Serial.println(F("Default Mode: [MODE 1] Smart Navigation"));
  Serial.println(F("Press '#' to toggle between Mode 1 and Mode 2."));
  Serial.println(F("----------------------------------------------------"));
}

void loop() {
  char key = scanKeypad();

  if (key != 0) {
    Serial.print(F("Key: '"));
    Serial.print(key);
    Serial.print(F("' | Mode: "));
    Serial.println(currentMode);

    // Toggle Mode with '#'
    if (key == '#') {
      currentMode = (currentMode == 1) ? 2 : 1;
      
      if (currentMode == 1) {
        Serial.println(F(">>> Mode 1: Sony Bravia Google TV Smart Navigation"));
        blinkLED(1);
      } else {
        Serial.println(F(">>> Mode 2: Sony Bravia Number Pad (0-9) & Channels"));
        blinkLED(2);
      }
      return;
    }

    // Process key based on active mode
    if (currentMode == 1) {
      handleMode1_Nav(key);
    } else {
      handleMode2_Num(key);
    }
  }
}

// Custom Keypad Scanner with Debounce
char scanKeypad() {
  static unsigned long lastPressTime = 0;
  if (millis() - lastPressTime < 180) {
    return 0; // Debounce lockout
  }

  for (uint8_t r = 0; r < 4; r++) {
    digitalWrite(ROW_PINS[r], LOW);
    delayMicroseconds(10);

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

// Visual LED Feedback for Mode Switching
void blinkLED(uint8_t times) {
  for (uint8_t i = 0; i < times; i++) {
    digitalWrite(STATUS_LED_PIN, HIGH);
    delay(120);
    digitalWrite(STATUS_LED_PIN, LOW);
    delay(120);
  }
}

// Sony SIRC 12-bit Protocol Sender (TV Address 0x01, requires 2 repeats)
void sendSony12(uint8_t cmd, const __FlashStringHelper* label) {
  Serial.print(F("Sony 12-bit -> "));
  Serial.println(label);
  IrSender.sendSony(0x01, cmd, 2, 12);
}

// Sony SIRC 15-bit Extended Protocol Sender (requires 2 repeats)
void sendSony15(uint8_t addr, uint8_t cmd, const __FlashStringHelper* label) {
  Serial.print(F("Sony 15-bit -> "));
  Serial.println(label);
  IrSender.sendSony(addr, cmd, 2, 15);
}

// ---------------- MODE 1: SMART NAVIGATION ----------------
void handleMode1_Nav(char key) {
  switch (key) {
    case '1': sendSony12(0x15, F("POWER TOGGLE")); break;
    case '2': sendSony12(0x74, F("D-PAD UP")); break;
    case '3': sendSony12(0x60, F("GOOGLE TV HOME")); break;
    case 'A': sendSony12(0x12, F("VOLUME UP")); break;

    case '4': sendSony12(0x34, F("D-PAD LEFT")); break;
    case '5': sendSony12(0x65, F("OK / ENTER")); break;
    case '6': sendSony12(0x33, F("D-PAD RIGHT")); break;
    case 'B': sendSony12(0x13, F("VOLUME DOWN")); break;

    case '7': sendSony15(0x97, 0x23, F("BACK / RETURN")); break;
    case '8': sendSony12(0x75, F("D-PAD DOWN")); break;
    case '9': sendSony15(0x97, 0x36, F("OPTIONS MENU")); break;
    case 'C': sendSony12(0x14, F("MUTE")); break;

    case '*': sendSony12(0x25, F("INPUT SOURCE")); break;
    case '0':
      if (!isPaused) {
        sendSony15(0x97, 0x1A, F("PLAY"));
      } else {
        sendSony15(0x97, 0x19, F("PAUSE"));
      }
      isPaused = !isPaused;
      break;
    case 'D': sendSony12(0x10, F("CHANNEL UP")); break;
  }
}

// ---------------- MODE 2: NUMBER PAD (0-9) ----------------
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
    case 'A': sendSony12(0x12, F("VOLUME UP")); break;
    case 'B': sendSony12(0x13, F("VOLUME DOWN")); break;
    case 'C': sendSony12(0x10, F("CHANNEL UP")); break;
    case 'D': sendSony12(0x11, F("CHANNEL DOWN")); break;
  }
}