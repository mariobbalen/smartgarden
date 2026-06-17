#include <Servo.h>

const int RED_LED            = 5;
const int YELLOW_LED         = 6;
const int TRIGGER_PIN_ULTRA  = 13;
const int ECHO_PIN_ULTRA     = 12;
const int SOIL_PIN           = A1;
const int LUMI_PIN           = A0;
const int SERVO_PIN          = 8;
const int GREEN_LED          = 7; 

const int MOIST_THRESHOLD    = 30;
const int LUMI_THRESHOLD     = 100;
const int PRESENCE_THRESHOLD = 45;

Servo servo;

void setup() {
  Serial.begin(9600);
  servo.attach(SERVO_PIN);
  
  pinMode(RED_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(TRIGGER_PIN_ULTRA, OUTPUT);
  pinMode(ECHO_PIN_ULTRA, INPUT);
  pinMode(GREEN_LED, OUTPUT);
}

void loop() {
  //TODO: PEGAR A DATA PELA ESP

  Serial.print("Lumi = ");
  Serial.println(analogRead(LUMI_PIN));
  Serial.println("");
  delay(50);
  
  if (analogRead(LUMI_PIN) < LUMI_THRESHOLD) {
    digitalWrite(GREEN_LED, HIGH);
  } else {
    digitalWrite(GREEN_LED, LOW);
  }

  if (getDistance(TRIGGER_PIN_ULTRA, ECHO_PIN_ULTRA) < PRESENCE_THRESHOLD) {
    digitalWrite(RED_LED, HIGH);
  } else {
    digitalWrite(RED_LED, LOW);
  }
  
  if (getMoist() <= MOIST_THRESHOLD) {
    digitalWrite(YELLOW_LED, HIGH);
    for (int pos = 0; pos <= 180; pos++) {
      servo.write(pos);
      delay(15);
    }
    for (int pos = 180; pos >= 0; pos--) {
      servo.write(pos);
      delay(15);
    }
    digitalWrite(YELLOW_LED, LOW);
  }
  else {
    digitalWrite(YELLOW_LED, LOW);
  }

  delay(1000);
}

int getMoist() {
  int raw = analogRead(SOIL_PIN);
  int pct = constrain(map(raw, 1023, 0, 0, 100), 0, 100);
  Serial.print("Moist = ");
  Serial.println(pct);
  Serial.println("");
  delay(50);
  return pct;
}

long getDistance(int triggerPin, int echoPin) {
  digitalWrite(triggerPin, LOW);
  delayMicroseconds(2);
  digitalWrite(triggerPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(triggerPin, LOW);
  long duration = pulseIn(echoPin, HIGH);
  long distance = (duration / 2) / 29.1;
  Serial.print("Distancia em cm: ");
  Serial.println(distance);
  Serial.println("");
  delay(50);
  return distance;
}
