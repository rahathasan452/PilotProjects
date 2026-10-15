#include <Arduino.h>
#include <EEPROM.h>

// ==================== PIN DEFINITIONS ====================
#define IR_SEND_PIN    3   // KY-005 IR Transmitter 'S' pin (Timer 2 PWM on Uno)
#define IR_RECEIVE_PIN 2   // IR Receiver (VS1838B / KY-022 / TSOP) Signal 'OUT' pin (INT0)
#define STATUS_LED_PIN 13  // Built-in Arduino Uno 'L' LED for blink feedback

#include <IRremote.hpp> // Requires IRremote library version 4.x or newer

// 4x4 Membrane Keypad Ribbon Cable Pins (Digital Pins 4 to 11)
const uint8_t ROW_PINS[4] = {4, 5, 6, 7};   // Pins 1 to 4 on ribbon cable (Rows R1, R2, R3, R4)
const uint8_t COL_PINS[4] = {8, 9, 10, 11}; // Pins 5 to 8 on ribbon cable (Columns C1, C2, C3, C4)

const char KEY_MAP[4][4] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

// ==================== MODE & STATE DEFINITIONS ====================
// Mode 1: Sony Bravia Google TV Smart Navigation (D-Pad, Home, Back, OK, Volume)
// Mode 2: Sony Bravia Number Pad (Digits 0-9 & Channel controls)
// Mode 3: Cloned TV Remote Playback (Transmits signals learned from your original remote)
// Mode 4: IR Clone / Learning Mode (Captures signals from original remote via IR Receiver Pin 2)
uint8_t currentMode = 1;
bool isPaused = false;

// ==================== CLONED REMOTE DATA STRUCTURES ====================
#define EEPROM_MAGIC     0x434C // Magic header 'CL' (Cloned Remote)
#define NUM_CLONED_KEYS  15     // 15 cloneable keys (Key '#' is reserved for mode switching)

struct ClonedCode {
  uint8_t protocol;       // decode_type_t (NEC, SONY, SAMSUNG, LG, PANASONIC, RC5, RC6, etc.)
  uint16_t address;      // IR device address
  uint16_t command;      // IR command code
  uint8_t numberOfBits;  // Bit length
  uint8_t flags;         // Protocol flags
  uint32_t rawData;      // Decoded raw data
  bool isValid;          // True if a valid signal has been cloned and saved
};

ClonedCode clonedKeys[NUM_CLONED_KEYS];

const int EEPROM_HEADER_ADDR = 0;
const int EEPROM_DATA_START  = 2;

// Mode 4 state tracking
char keyToLearn = 0;
unsigned long learnStartTime = 0;

// Function Prototypes
char scanKeypad();
void blinkLED(uint8_t times);
int8_t getKeyIndex(char key);
char getKeyCharFromIndex(uint8_t index);
void loadClonedKeysFromEEPROM();
void saveClonedKeyToEEPROM(uint8_t index);
void clearAllClonedKeys();
void printClonedKeysSummary();
void sendClonedCode(const ClonedCode &code);

void sendSony12(uint8_t cmd, const __FlashStringHelper* label);
void sendSony15(uint8_t addr, uint8_t cmd, const __FlashStringHelper* label);
void sendSonyDPad(char direction);

void handleMode1_Nav(char key);
void handleMode2_Num(char key);
void handleMode3_Cloned(char key);
void handleMode4_Learn(char key);

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

  // Load saved cloned keys from EEPROM
  loadClonedKeysFromEEPROM();

  // Initialize IR Receiver on Pin 2 with LED feedback
  IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK);

  // Initialize KY-005 IR Transmitter on Pin 3
  IrSender.begin(IR_SEND_PIN);

  Serial.println(F("=============================================================="));
  Serial.println(F("    UNIVERSAL & SONY BRAVIA SMART REMOTE CONTROLLER           "));
  Serial.println(F("=============================================================="));
  Serial.println(F("Status: Ready!"));
  Serial.println(F("Available Modes:"));
  Serial.println(F("  [MODE 1] Sony Bravia Smart Navigation (D-Pad, Home, Back, OK)"));
  Serial.println(F("  [MODE 2] Sony Bravia Number Pad (0-9) & Channels"));
  Serial.println(F("  [MODE 3] Cloned TV Remote Playback (Transmits learned signals)"));
  Serial.println(F("  [MODE 4] IR Clone / Learning Mode (Learns from original remote)"));
  Serial.println(F("--------------------------------------------------------------"));
  Serial.println(F("Press '#' on your 4x4 keypad at any time to cycle modes."));
  Serial.println(F("Serial Commands: 'l' = List Cloned Keys | 'c' = Clear EEPROM"));
  Serial.println(F("--------------------------------------------------------------"));
  printClonedKeysSummary();
}

void loop() {
  // Check Serial Monitor for quick commands
  if (Serial.available()) {
    char cmd = Serial.read();
    if (cmd == 'l' || cmd == 'L') {
      printClonedKeysSummary();
    } else if (cmd == 'c' || cmd == 'C') {
      clearAllClonedKeys();
    }
  }

  char key = scanKeypad();

  if (key != 0) {
    Serial.print(F("[Key Pressed: '"));
    Serial.print(key);
    Serial.print(F("'] | Active Mode: "));
    Serial.print(currentMode);
    Serial.print(F(" -> "));

    // '#' Button cycles through Mode 1 -> Mode 2 -> Mode 3 -> Mode 4
    if (key == '#') {
      currentMode++;
      if (currentMode > 4) {
        currentMode = 1;
      }

      if (currentMode == 1) {
        Serial.println(F("SWITCHED TO MODE 1: Sony Bravia Google TV Smart Navigation"));
        blinkLED(1);
      } else if (currentMode == 2) {
        Serial.println(F("SWITCHED TO MODE 2: Sony Bravia Number Pad (0-9) & Channels"));
        blinkLED(2);
      } else if (currentMode == 3) {
        Serial.println(F("SWITCHED TO MODE 3: Cloned TV Remote Playback"));
        printClonedKeysSummary();
        blinkLED(3);
      } else if (currentMode == 4) {
        Serial.println(F("SWITCHED TO MODE 4: IR Clone / Learning Mode"));
        Serial.println(F("  Step 1: Press the Keypad button you want to program (0-9, *, A-D)."));
        Serial.println(F("  Step 2: Point original remote at IR Receiver (Pin 2) and press button."));
        keyToLearn = 0;
        digitalWrite(STATUS_LED_PIN, LOW);
        blinkLED(4);
      }
      return;
    }

    // Route key event based on active mode
    if (currentMode == 1) {
      handleMode1_Nav(key);
    } else if (currentMode == 2) {
      handleMode2_Num(key);
    } else if (currentMode == 3) {
      handleMode3_Cloned(key);
    } else if (currentMode == 4) {
      handleMode4_Learn(key);
    }
  } else {
    // When no keypad key is pressed, Mode 4 continues listening for incoming IR signals
    if (currentMode == 4) {
      handleMode4_Learn(0);
    } else {
      // Flush any residual incoming IR frames in non-learning modes
      if (IrReceiver.decode()) {
        IrReceiver.resume();
      }
    }
  }
}

// ==================== KEYPAD SCANNER ====================
char scanKeypad() {
  static unsigned long lastPressTime = 0;
  if (millis() - lastPressTime < 220) {
    return 0; // Hardware debounce guard
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

// Maps keypad character to cloned array index 0..14
int8_t getKeyIndex(char key) {
  switch (key) {
    case '1': return 0;
    case '2': return 1;
    case '3': return 2;
    case 'A': return 3;
    case '4': return 4;
    case '5': return 5;
    case '6': return 6;
    case 'B': return 7;
    case '7': return 8;
    case '8': return 9;
    case '9': return 10;
    case 'C': return 11;
    case '*': return 12;
    case '0': return 13;
    case 'D': return 14;
    default:  return -1; // '#' is reserved for mode toggle
  }
}

// Reverse lookup for display
char getKeyCharFromIndex(uint8_t index) {
  const char keyChars[NUM_CLONED_KEYS] = {
    '1', '2', '3', 'A',
    '4', '5', '6', 'B',
    '7', '8', '9', 'C',
    '*', '0', 'D'
  };
  if (index < NUM_CLONED_KEYS) return keyChars[index];
  return '?';
}

// Blinks built-in Pin 13 LED to indicate mode changes or status
void blinkLED(uint8_t times) {
  for (uint8_t i = 0; i < times; i++) {
    digitalWrite(STATUS_LED_PIN, HIGH);
    delay(120);
    digitalWrite(STATUS_LED_PIN, LOW);
    delay(120);
  }
}

// ==================== EEPROM STORAGE MANAGEMENT ====================
void loadClonedKeysFromEEPROM() {
  uint16_t magic = 0;
  EEPROM.get(EEPROM_HEADER_ADDR, magic);

  if (magic == EEPROM_MAGIC) {
    for (uint8_t i = 0; i < NUM_CLONED_KEYS; i++) {
      int addr = EEPROM_DATA_START + (i * sizeof(ClonedCode));
      EEPROM.get(addr, clonedKeys[i]);
    }
    Serial.println(F("[EEPROM] Cloned remote profiles loaded successfully."));
  } else {
    // First boot or uninitialized EEPROM
    for (uint8_t i = 0; i < NUM_CLONED_KEYS; i++) {
      clonedKeys[i].isValid = false;
      clonedKeys[i].protocol = UNKNOWN;
      clonedKeys[i].address = 0;
      clonedKeys[i].command = 0;
      clonedKeys[i].numberOfBits = 0;
      clonedKeys[i].flags = 0;
      clonedKeys[i].rawData = 0;
    }
    EEPROM.put(EEPROM_HEADER_ADDR, (uint16_t)EEPROM_MAGIC);
    for (uint8_t i = 0; i < NUM_CLONED_KEYS; i++) {
      int addr = EEPROM_DATA_START + (i * sizeof(ClonedCode));
      EEPROM.put(addr, clonedKeys[i]);
    }
    Serial.println(F("[EEPROM] Initialized fresh EEPROM storage for cloned profiles."));
  }
}

void saveClonedKeyToEEPROM(uint8_t index) {
  if (index < NUM_CLONED_KEYS) {
    int addr = EEPROM_DATA_START + (index * sizeof(ClonedCode));
    EEPROM.put(addr, clonedKeys[index]);
  }
}

void clearAllClonedKeys() {
  for (uint8_t i = 0; i < NUM_CLONED_KEYS; i++) {
    clonedKeys[i].isValid = false;
    clonedKeys[i].protocol = UNKNOWN;
    clonedKeys[i].address = 0;
    clonedKeys[i].command = 0;
    clonedKeys[i].numberOfBits = 0;
    clonedKeys[i].flags = 0;
    clonedKeys[i].rawData = 0;
    saveClonedKeyToEEPROM(i);
  }
  Serial.println(F("[EEPROM] All cloned keys cleared back to factory defaults!"));
  blinkLED(5);
}

void printClonedKeysSummary() {
  Serial.println(F("\n--- Current Cloned Remote Key Assignments ---"));
  bool anyCloned = false;
  for (uint8_t i = 0; i < NUM_CLONED_KEYS; i++) {
    if (clonedKeys[i].isValid) {
      anyCloned = true;
      Serial.print(F("  Key '"));
      Serial.print(getKeyCharFromIndex(i));
      Serial.print(F("': Protocol "));
      Serial.print(getProtocolString((decode_type_t)clonedKeys[i].protocol));
      Serial.print(F(" | Addr: 0x"));
      Serial.print(clonedKeys[i].address, HEX);
      Serial.print(F(" | Cmd: 0x"));
      Serial.print(clonedKeys[i].command, HEX);
      Serial.print(F(" | Bits: "));
      Serial.println(clonedKeys[i].numberOfBits);
    }
  }
  if (!anyCloned) {
    Serial.println(F("  (No buttons cloned yet. Switch to Mode 4 with '#' to clone buttons!)"));
  }
  Serial.println(F("---------------------------------------------"));
}

// ==================== CLONED IR TRANSMISSION ====================
void sendClonedCode(const ClonedCode &code) {
  IRData sendData;
  memset(&sendData, 0, sizeof(sendData));
  sendData.protocol = (decode_type_t)code.protocol;
  sendData.address = code.address;
  sendData.command = code.command;
  sendData.numberOfBits = code.numberOfBits;
  sendData.flags = 0;
  sendData.decodedRawData = code.rawData;

  // Temporarily pause receiver during transmission to avoid receiving self-echo
  IrReceiver.stop();
  digitalWrite(STATUS_LED_PIN, HIGH);

  // Sony protocols require >= 3 repeats; other protocols typically require 1 repeat
  int_fast8_t repeats = (code.protocol == SONY) ? 3 : 1;
  IrSender.write(&sendData, repeats);

  digitalWrite(STATUS_LED_PIN, LOW);
  IrReceiver.start();
}

// ==================== MODE 3: CLONED TV REMOTE PLAYBACK ====================
void handleMode3_Cloned(char key) {
  int8_t idx = getKeyIndex(key);
  if (idx < 0) return;

  if (!clonedKeys[idx].isValid) {
    Serial.print(F("Key '"));
    Serial.print(key);
    Serial.println(F("' has not been cloned yet! Switch to Mode 4 (press '#') to learn it."));
    blinkLED(2);
    return;
  }

  Serial.print(F("Transmitting Cloned Key '"));
  Serial.print(key);
  Serial.print(F("' -> Protocol: "));
  Serial.print(getProtocolString((decode_type_t)clonedKeys[idx].protocol));
  Serial.print(F(" | Addr: 0x"));
  Serial.print(clonedKeys[idx].address, HEX);
  Serial.print(F(" | Cmd: 0x"));
  Serial.println(clonedKeys[idx].command, HEX);

  sendClonedCode(clonedKeys[idx]);
}

// ==================== MODE 4: IR CLONE / LEARNING HANDLER ====================
void handleMode4_Learn(char key) {
  // If user presses a keypad key, select it for programming
  if (key != 0) {
    int8_t idx = getKeyIndex(key);
    if (idx >= 0) {
      keyToLearn = key;
      learnStartTime = millis();

      Serial.print(F("Target Key '"));
      Serial.print(keyToLearn);
      Serial.println(F("' selected for programming!"));
      Serial.println(F(">>> Point your original TV remote at IR Receiver (Pin 2) and press the button NOW..."));

      // Clear any prior pending frame
      IrReceiver.resume();

      // Turn LED solid ON to indicate listening state
      digitalWrite(STATUS_LED_PIN, HIGH);
    }
    return;
  }

  // When waiting for incoming IR signal from original TV remote
  if (keyToLearn != 0) {
    // 15-second timeout safeguard
    if (millis() - learnStartTime > 15000) {
      Serial.print(F("[TIMEOUT] No IR signal received for Key '"));
      Serial.print(keyToLearn);
      Serial.println(F("'. Learning cancelled."));
      digitalWrite(STATUS_LED_PIN, LOW);
      keyToLearn = 0;
      return;
    }

    if (IrReceiver.decode()) {
      digitalWrite(STATUS_LED_PIN, LOW);

      // Filter out repeat frames
      if (IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT) {
        IrReceiver.resume();
        return;
      }

      // Filter out parity errors
      if (IrReceiver.decodedIRData.flags & IRDATA_FLAGS_PARITY_FAILED) {
        Serial.println(F("[ERROR] Parity check failed. Please press the remote button again."));
        IrReceiver.resume();
        return;
      }

      // Check protocol validity
      if (IrReceiver.decodedIRData.protocol == UNKNOWN) {
        Serial.println(F("[WARNING] Unknown protocol or noisy signal. Hold remote 5-15cm away and try again."));
        IrReceiver.resume();
        return;
      }

      int8_t idx = getKeyIndex(keyToLearn);
      if (idx >= 0) {
        clonedKeys[idx].protocol = (uint8_t)IrReceiver.decodedIRData.protocol;
        clonedKeys[idx].address = IrReceiver.decodedIRData.address;
        clonedKeys[idx].command = IrReceiver.decodedIRData.command;
        clonedKeys[idx].numberOfBits = (uint8_t)IrReceiver.decodedIRData.numberOfBits;
        clonedKeys[idx].flags = 0;
        clonedKeys[idx].rawData = IrReceiver.decodedIRData.decodedRawData;
        clonedKeys[idx].isValid = true;

        // Persist to EEPROM
        saveClonedKeyToEEPROM(idx);

        Serial.println(F("\n=============================================================="));
        Serial.print(F(">>> [SUCCESS] KEY '"));
        Serial.print(keyToLearn);
        Serial.println(F("' HAS BEEN CLONED AND SAVED! <<<"));
        Serial.print(F("Protocol : "));
        Serial.println(getProtocolString(IrReceiver.decodedIRData.protocol));
        Serial.print(F("Address  : 0x"));
        Serial.println(clonedKeys[idx].address, HEX);
        Serial.print(F("Command  : 0x"));
        Serial.println(clonedKeys[idx].command, HEX);
        Serial.print(F("Bits     : "));
        Serial.println(clonedKeys[idx].numberOfBits);
        Serial.println(F("Permanently saved in EEPROM! You can now use it in Mode 3."));
        Serial.println(F("Press another keypad key to program, or press '#' to switch modes."));
        Serial.println(F("=============================================================="));

        blinkLED(3); // 3 joyful blinks on success
      }

      keyToLearn = 0;
      IrReceiver.resume();
    }
  }
}

// ==================== SONY IR SENDER FUNCTIONS ====================
// Standard Sony 12-bit SIRC: Device Address 0x01 (TV), repeat count = 3 (4 frames total)
void sendSony12(uint8_t cmd, const __FlashStringHelper* label) {
  Serial.print(F("Sony 12-bit (Addr 0x01, Cmd 0x"));
  Serial.print(cmd, HEX);
  Serial.print(F("): "));
  Serial.println(label);

  IrReceiver.stop();
  digitalWrite(STATUS_LED_PIN, HIGH);
  IrSender.sendSony(0x01, cmd, 3, 12);
  digitalWrite(STATUS_LED_PIN, LOW);
  IrReceiver.start();
}

// Extended Sony 15-bit SIRC: repeat count = 3 (4 frames total)
void sendSony15(uint8_t addr, uint8_t cmd, const __FlashStringHelper* label) {
  Serial.print(F("Sony 15-bit (Addr 0x"));
  Serial.print(addr, HEX);
  Serial.print(F(", Cmd 0x"));
  Serial.print(cmd, HEX);
  Serial.print(F("): "));
  Serial.println(label);

  IrReceiver.stop();
  digitalWrite(STATUS_LED_PIN, HIGH);
  IrSender.sendSony(addr, cmd, 3, 15);
  digitalWrite(STATUS_LED_PIN, LOW);
  IrReceiver.start();
}

// ==================== HYBRID D-PAD NAVIGATION HANDLER ====================
// Transmits the standard Sony 12-bit cursor command followed immediately by
// the Sony 15-bit Android/Google TV smart cursor command so all Bravia models respond
void sendSonyDPad(char direction) {
  IrReceiver.stop();
  digitalWrite(STATUS_LED_PIN, HIGH);

  switch (direction) {
    case 'U': // D-PAD UP
      Serial.println(F("D-PAD UP -> Firing 12-bit (0x74) + 15-bit (0x4F)"));
      IrSender.sendSony(0x01, 0x74, 2, 12);
      delay(25);
      IrSender.sendSony(0x97, 0x4F, 2, 15);
      break;

    case 'D': // D-PAD DOWN
      Serial.println(F("D-PAD DOWN -> Firing 12-bit (0x75) + 15-bit (0x50)"));
      IrSender.sendSony(0x01, 0x75, 2, 12);
      delay(25);
      IrSender.sendSony(0x97, 0x50, 2, 15);
      break;

    case 'L': // D-PAD LEFT
      Serial.println(F("D-PAD LEFT -> Firing 12-bit (0x34) + 15-bit (0x4D)"));
      IrSender.sendSony(0x01, 0x34, 2, 12);
      delay(25);
      IrSender.sendSony(0x97, 0x4D, 2, 15);
      break;

    case 'R': // D-PAD RIGHT
      Serial.println(F("D-PAD RIGHT -> Firing 12-bit (0x33) + 15-bit (0x4E)"));
      IrSender.sendSony(0x01, 0x33, 2, 12);
      delay(25);
      IrSender.sendSony(0x97, 0x4E, 2, 15);
      break;

    case 'O': // OK / SELECT / ENTER
      Serial.println(F("OK / ENTER -> Firing 12-bit (0x65) + 15-bit (0x4C) + Alt (0x0B)"));
      IrSender.sendSony(0x01, 0x65, 2, 12);
      delay(25);
      IrSender.sendSony(0x97, 0x4C, 2, 15);
      delay(25);
      IrSender.sendSony(0x01, 0x0B, 2, 12);
      break;
  }

  digitalWrite(STATUS_LED_PIN, LOW);
  IrReceiver.start();
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