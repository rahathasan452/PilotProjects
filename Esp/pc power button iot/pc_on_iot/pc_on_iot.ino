// 1. Paste your Blynk Template info at the very top
#define BLYNK_TEMPLATE_ID ""
#define BLYNK_TEMPLATE_NAME "PC Power Control"
#define BLYNK_AUTH_TOKEN ""

#define BLYNK_PRINT Serial
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>

// 2. Enter your 2.4GHz Wi-Fi credentials
char ssid[] = "Biye Koro!";
char pass[] = "@";

// D1 corresponds to GPIO 5 on NodeMCU ESP8266
const int OPTO_PIN = 5; 
BlynkTimer timer;

// Triggered when Virtual Pin V0 is pressed in the Blynk App
BLYNK_WRITE(V0) {
  int pinValue = param.asInt();
  
  if (pinValue == 1) {
    Serial.println("Triggering PC Power Button (500ms pulse)...");
    digitalWrite(OPTO_PIN, HIGH); // Turn on optocoupler internal LED
    
    // Automatically release the button after 500ms without blocking Wi-Fi
    timer.setTimeout(500L, []() {
      digitalWrite(OPTO_PIN, LOW); // Turn off optocoupler
      Blynk.virtualWrite(V0, 0);   // Reset button UI in app
      Serial.println("PC Power Button Released.");
    });
  }
}

// Optional: Virtual Pin V1 for Force Shutdown (5.5-second hold) if PC freezes
BLYNK_WRITE(V1) {
  int pinValue = param.asInt();
  
  if (pinValue == 1) {
    Serial.println("Triggering Force Shutdown (5500ms pulse)...");
    digitalWrite(OPTO_PIN, HIGH);
    
    timer.setTimeout(5500L, []() {
      digitalWrite(OPTO_PIN, LOW);
      Blynk.virtualWrite(V1, 0);
      Serial.println("Force Shutdown Released.");
    });
  }
}

void setup() {
  Serial.begin(115200);
  
  // Ensure the optocoupler stays OFF immediately on boot
  digitalWrite(OPTO_PIN, LOW);
  pinMode(OPTO_PIN, OUTPUT);
  
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
}

void loop() {
  Blynk.run();
  timer.run();
}
