/*
 * Robotic Arm Sequence Controller
 * Target Hardware: Arduino Uno/Nano
 * Integration: Matches user's variable names (servo1-4) and Pinout
 * * PIN MAPPING (From your snippet):
 * servo1 (Base)     -> Pin 9
 * servo2 (Shoulder) -> Pin 3
 * servo3 (Elbow)    -> Pin 6
 * servo4 (Claw)     -> Pin 5
 */

#include <Servo.h>

// --- Configuration ---
const int SPEED_DELAY  = 15; // Delay between moves (Lower = Faster)
const int NUM_STEPS    = 6;
const int NUM_SERVOS   = 4;

// --- Servo Objects (Matched to your names) ---
Servo servo1; // Base
Servo servo2; // Shoulder
Servo servo3; // Elbow
Servo servo4; // Claw

// --- Variables for Servo Values ---
// We use these to track where the servo IS currently
int servoVal1; // Base position
int servoVal2; // Shoulder position
int servoVal3; // Elbow position
int servoVal4; // Claw position

// --- The Look-Up Table (LUT) ---
// Sequence Order: {Shoulder, Elbow, Base, Claw}
// Note: We must map these correctly to servo1, servo2, servo3, servo4 in the code below.
// servo2=Shoulder, servo3=Elbow, servo1=Base, servo4=Claw
const int waypoints[NUM_STEPS][NUM_SERVOS] = {
  // S(s2)  E(s3)  B(s1)  C(s4)
  {35,      140,   60,    100}, // Step 0: Home
  {88,      120,   60,    100}, // Step 1: Reach
  {88,      120,   60,    0},   // Step 2: Grab
  {35,      140,   60,    0},   // Step 3: Retract
  {35,      140,   150,   0},   // Step 4: Rotate
  {45,      150,   150,   100}  // Step 5: Drop
};

void setup() {
  Serial.begin(9600);
  Serial.println("Initializing Robot Arm (User Config)...");

  // --- Attach Servos (Matched to your pins) ---
  servo1.attach(9);  // Base
  servo2.attach(3);  // Shoulder
  servo3.attach(6);  // Elbow
  servo4.attach(5);  // Claw

  // Initialize standard starting positions (Step 0)
  // We load Step 0 values into our tracking variables
  servoVal2 = waypoints[0][0]; // Shoulder
  servoVal3 = waypoints[0][1]; // Elbow
  servoVal1 = waypoints[0][2]; // Base
  servoVal4 = waypoints[0][3]; // Claw

  // Write initial positions
  servo1.write(servoVal1);
  servo2.write(servoVal2);
  servo3.write(servoVal3);
  servo4.write(servoVal4);

  delay(1000); // Wait for servos to reach home
}

void loop() {
  // Iterate through the sequence steps
  for (int i = 0; i < NUM_STEPS; i++) {
    Serial.print("Executing Step: ");
    Serial.println(i);
    
    // Extract target values from the array for this step
    int targetShoulder = waypoints[i][0];
    int targetElbow    = waypoints[i][1];
    int targetBase     = waypoints[i][2];
    int targetClaw     = waypoints[i][3];

    // Move to this step smoothly
    moveToTarget(targetBase, targetShoulder, targetElbow, targetClaw);
    
    delay(1000); // Pause at each position
  }
  
  Serial.println("Sequence Complete. Returning to Start...");
  delay(2000);
  
  // Move back to Step 0 smoothly
  moveToTarget(waypoints[0][2], waypoints[0][0], waypoints[0][1], waypoints[0][3]);
  delay(1000);
}

/**
 * Smooth Movement Function
 * Updates servoVal1...servoVal4 incrementally until they match targets.
 * * Inputs:
 * t_base  (Target for servo1)
 * t_shldr (Target for servo2)
 * t_elbow (Target for servo3)
 * t_claw  (Target for servo4)
 */
void moveToTarget(int t_base, int t_shldr, int t_elbow, int t_claw) {
  
  bool moveFinished = false;

  // Keep looping until all servos correspond to the target arguments
  while (!moveFinished) {
    moveFinished = true; // Assume done, prove otherwise

    // --- Base (servo1) ---
    if (servoVal1 != t_base) {
      if (servoVal1 < t_base) servoVal1++;
      else servoVal1--;
      servo1.write(servoVal1);
      moveFinished = false;
    }

    // --- Shoulder (servo2) ---
    if (servoVal2 != t_shldr) {
      if (servoVal2 < t_shldr) servoVal2++;
      else servoVal2--;
      servo2.write(servoVal2);
      moveFinished = false;
    }

    // --- Elbow (servo3) ---
    if (servoVal3 != t_elbow) {
      if (servoVal3 < t_elbow) servoVal3++;
      else servoVal3--;
      servo3.write(servoVal3);
      moveFinished = false;
    }

    // --- Claw (servo4) ---
    if (servoVal4 != t_claw) {
      if (servoVal4 < t_claw) servoVal4++;
      else servoVal4--;
      servo4.write(servoVal4);
      moveFinished = false;
    }

    // Print debug (Optional, slows down loop)
    // Serial.print(servoVal1); Serial.print(","); Serial.println(servoVal2);

    delay(SPEED_DELAY); // Controls the speed
  }
}