#include <Servo.h>

Servo servo1;
Servo servo2;
Servo servo3;
Servo servo4;




int x1Pin = A0;  //vrY pin
int y1Pin = A1;  //vrX pin

int x2Pin = A2;   //vrY pin
int y2Pin = A3;   //vrX pin



int x1;
int y1;
int x2;
int y2;

int servoVal1;
int servoVal2;
int servoVal3;
int servoVal4;

void setup() 
{
  // put your setup code here, to run once:
  servo1.attach(9);  // base
  servo2.attach(3);  //shoulder
  servo3.attach(6);  //elbow
  servo4.attach(5);  // claw
  

  Serial.begin(9600);

  //pinMode(buttonPin, INPUT_PULLUP);
}

void loop() {
  // put your main code here, to run repeatedly:

  int x1 = analogRead(x1Pin);  // 0–1023
  int y1 = analogRead(y1Pin);  // 0–1023
  int x2 = analogRead(x2Pin);  // 0–1023
  int y2 = analogRead(y2Pin);  // 0–1023
  //int buttonState = digitalRead(buttonPin);  // LOW = pressed

 
  servoVal1 = map(x1, 0, 1023, 0, 180);   // base
  servo1.write(servoVal1);

 
  servoVal2 = map(y1,0, 1023, 0, 180);   //shoulder
  servo2.write(servoVal2);

 
  servoVal3 = map(x2, 0, 1023, 0 , 180); // elbow
  servo3.write(servoVal3);
 
 
 
 
  servoVal4 = map(y2, 0, 1023, 0, 100);  // claw
  servo4.write(servoVal4);

  Serial.print("X1: ");
  Serial.print(x1);
 
  
  Serial.print(" | Y1: ");
  Serial.print(y1);
   

  Serial.print("    |  X2: ");
  Serial.print(x2);
 
  
  Serial.print(" | Y2: ");
  Serial.print(y2);

    Serial.print(" |||    X1: ");
  Serial.print(servoVal1);
 
  
  Serial.print(" | Y1: ");
  Serial.print(servoVal2);
   

  Serial.print("    |  X2: ");
  Serial.print(servoVal3);
 
  
  Serial.print(" | Y2: ");
  Serial.println(servoVal4);

  delay(15);

}
