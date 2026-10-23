# IoT Remote PC Power Switch

Controls PC power remotely via ESP8266 and PC817 optocoupler, maintaining 100% electrical isolation.

---

## 1. Hardware Setup & Reference Photos

| Bench Test Setup | Chassis Installation |
| :---: | :---: |
| ![Bench Test Setup](assets/bench_test_setup.jpg) | ![Chassis Installation](assets/pc_case_wiring.jpg) |
| *NodeMCU, resistor, PC817 optocoupler & bench jumper leads* | *Installed inside PC case spliced with POWER SW header* |

---

## 2. Circuit Schematic

```mermaid
flowchart LR
    subgraph ESP8266["ESP8266 NodeMCU"]
        D1["Pin D1 (GPIO 5)"]
        GND["Pin G (GND)"]
    end

    subgraph Circuit["Breadboard Circuit"]
        R["330Ω Resistor"]
        D1 --> R
        
        subgraph PC817["PC817 Optocoupler"]
            P1["Pin 1 (Anode / Dot)"]
            P2["Pin 2 (Cathode)"]
            P4["Pin 4 (Collector)"]
            P3["Pin 3 (Emitter)"]
            
            P1 -.->|IR Light Barrier| P4
            P2 -.-> P3
        end
        
        R --> P1
        P2 --> GND
    end

    subgraph Motherboard["PC Motherboard Front Panel"]
        MB_PWR["PWR_SW (+) Header"]
        MB_GND["PWR_SW (-) Header"]
        
        P4 --> MB_PWR
        P3 --> MB_GND
    end

    style PC817 fill:#fdf2e9,stroke:#e67e22,stroke-width:2px
    style ESP8266 fill:#ebf5fb,stroke:#2980b9,stroke-width:2px
    style Motherboard fill:#eaeff2,stroke:#34495e,stroke-width:2px
```

---

## 3. PC817 (817BN) Pinout

```mermaid
flowchart TD
    subgraph Chip["PC817 DIP-4 (Top View)"]
        direction TB
        subgraph TopRow["Input Side (ESP)"]
            P1["1: Anode (+) [Dot]"]
            P2["2: Cathode (-)"]
        end
        subgraph BottomRow["Output Side (Motherboard)"]
            P4["4: Collector (+)"]
            P3["3: Emitter (-)"]
        end
    end
    P1 --- P4
    P2 --- P3
```

* **Pin 1 (Dot):** Connects to **D1 (GPIO 5)** via **330Ω resistor**.
* **Pin 2:** Connects to ESP8266 **GND**.
* **Pin 3:** Connects to Motherboard **PWR_SW -** (Ground).
* **Pin 4:** Connects to Motherboard **PWR_SW +** (Signal).

---

## 4. Wiring Quick Reference

| From | To | Component / Wire | Role |
| :--- | :--- | :--- | :--- |
| **ESP8266 D1 (GPIO 5)** | **PC817 Pin 1** | 220Ω–470Ω Resistor | Safe trigger signal |
| **ESP8266 GND** | **PC817 Pin 2** | Jumper wire | Return ground |
| **PC817 Pin 4** | **Motherboard PWR_SW (+)** | DuPont M-F | Collector (3.3V sense) |
| **PC817 Pin 3** | **Motherboard PWR_SW (-)** | DuPont M-F | Emitter (Motherboard GND) |

> **Why D1 (GPIO 5)?** D1 and D2 are boot-safe. Pins D0, D3, D4, and D8 glitch during boot or ESP reset, which would accidentally trigger your PC.

---

## 5. Firmware (`pc_on_iot.ino`)

```cpp
#define BLYNK_TEMPLATE_ID "TMPL6Hzn5xBxV"
#define BLYNK_TEMPLATE_NAME "PC Power Control"
#define BLYNK_AUTH_TOKEN "Your_Blynk_Auth_Token"

#define BLYNK_PRINT Serial
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>

char ssid[] = "Your_WiFi_SSID";
char pass[] = "Your_WiFi_Password";

const int OPTO_PIN = 5; // D1 = GPIO 5
BlynkTimer timer;

// V0: 500ms Normal Press (Turn ON or Graceful Shutdown)
BLYNK_WRITE(V0) {
  if (param.asInt() == 1) {
    digitalWrite(OPTO_PIN, HIGH);
    timer.setTimeout(500L, []() {
      digitalWrite(OPTO_PIN, LOW);
      Blynk.virtualWrite(V0, 0);
    });
  }
}

// V1: 5.5s Emergency Hard Reset
BLYNK_WRITE(V1) {
  if (param.asInt() == 1) {
    digitalWrite(OPTO_PIN, HIGH);
    timer.setTimeout(5500L, []() {
      digitalWrite(OPTO_PIN, LOW);
      Blynk.virtualWrite(V1, 0);
    });
  }
}

void setup() {
  Serial.begin(115200);
  digitalWrite(OPTO_PIN, LOW); // Stay OFF during boot
  pinMode(OPTO_PIN, OUTPUT);
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
}

void loop() {
  Blynk.run();
  timer.run();
}
```

---

## 6. Configuration & Quick Checklist

1. **Blynk App:** Add Button widget $\to$ Datastream `V0` $\to$ Mode: **PUSH**.
2. **Motherboard BIOS:** Disable **ErP**, Enable **USB Power in Soft Off (S5)** (keeps ESP powered when PC is off).
3. **Windows Setting:** `Control Panel` $\to$ `Power Options` $\to$ `When I press the power button: Shut down`.
4. **Physical Button:** Connect in parallel with Pin 3 & Pin 4 using a DuPont Y-splitter if you want both case button & app control.
5. **Polarity Check:** If PC does not trigger, swap Pin 3 and Pin 4 jumpers on the motherboard header.
