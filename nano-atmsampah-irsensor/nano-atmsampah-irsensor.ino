#include <Servo.h>

const int BUTTON_PIN1   = 2;
const int BUTTON_PIN2   = 3;
const int IR_SENSOR_PIN = 4;
const int SERVO_PIN1    = 5; // servo continues
const int SERVO_PIN2    = 6; // servo 180

const int SERVO_STOP = 90;   // netral (berhenti) - servo 1 (continuous rotation)
const int SERVO_CW   = 180;  // putar CW - servo 1
const int SERVO_CCW  = 0;    // putar CCW - servo 1

const unsigned long MOVE_DURATION = 500;  // durasi gerak servo 1
const unsigned long HOLD_DELAY    = 2000; // jeda servo 1 di posisi tertentu

// ==== Konfigurasi servo 2 (positional 180 derajat, bukan continuous) ====
const int SERVO2_HOME_ANGLE   = 0;   // posisi awal
const int SERVO2_TARGET_ANGLE = 90;  // posisi target
const unsigned long SERVO2_TRAVEL_TIME = 300;  // asumsi waktu tempuh fisik servo, sesuaikan jika perlu
const unsigned long SERVO2_HOLD_DELAY  = 1000; // jeda 1 detik di posisi 90 derajat

enum State {
  STATE_IDLE,
  STATE_HOMING,
  STATE_MOVING_FORWARD,
  STATE_HOLDING,
  STATE_RETURNING
};

enum State2 {
  STATE2_IDLE,
  STATE2_MOVING_TO_TARGET,
  STATE2_HOLDING,
  STATE2_RETURNING_HOME
};

State  currentState  = STATE_IDLE;
State2 currentState2 = STATE2_IDLE;

unsigned long stateStartTime  = 0;
unsigned long stateStartTime2 = 0;

// Debounce & edge-detect Button 1
bool lastButtonReading1 = HIGH;
bool buttonStableState1 = HIGH;
unsigned long lastDebounceTime1 = 0;

// Debounce & edge-detect Button 2
bool lastButtonReading2 = HIGH;
bool buttonStableState2 = HIGH;
unsigned long lastDebounceTime2 = 0;

const unsigned long debounceDelay = 50;

Servo myServo;   // servo 1 (continuous rotation)
Servo myServo2;  // servo 2 (positional 180 derajat)

void setup() {
  pinMode(BUTTON_PIN1, INPUT_PULLUP);
  pinMode(BUTTON_PIN2, INPUT_PULLUP);
  pinMode(IR_SENSOR_PIN, INPUT); // asumsi: output IR sensor push-pull

  myServo.attach(SERVO_PIN1);
  myServo.write(SERVO_STOP);

  myServo2.attach(SERVO_PIN2);
  myServo2.write(SERVO2_HOME_ANGLE); // posisi awal 0 derajat
}

void loop() {
  bool button1PressedEvent = readButtonPressed(BUTTON_PIN1, lastButtonReading1, buttonStableState1, lastDebounceTime1);
  bool button2PressedEvent = readButtonPressed(BUTTON_PIN2, lastButtonReading2, buttonStableState2, lastDebounceTime2);
  bool irHigh = (digitalRead(IR_SENSOR_PIN) == LOW);

  // ================= STATE MACHINE SERVO 1 =================
  switch (currentState) {
    case STATE_IDLE:
      if (!irHigh) {
        myServo.write(SERVO_CW);
        currentState = STATE_HOMING;
        break;
      }
      myServo.write(SERVO_STOP);
      // servo 1 hanya boleh mulai siklus baru kalau servo 2 sedang idle
      if (button1PressedEvent && currentState2 == STATE2_IDLE) {
        myServo.write(SERVO_CCW);
        stateStartTime = millis();
        currentState = STATE_MOVING_FORWARD;
      }
      break;

    case STATE_HOMING:
      if (irHigh) {
        myServo.write(SERVO_STOP);
        currentState = STATE_IDLE;
      }
      break;

    case STATE_MOVING_FORWARD:
      if (millis() - stateStartTime >= MOVE_DURATION) {
        myServo.write(SERVO_STOP);
        stateStartTime = millis();
        currentState = STATE_HOLDING;
      }
      break;

    case STATE_HOLDING:
      myServo.write(SERVO_STOP);
      if (millis() - stateStartTime >= HOLD_DELAY) {
        myServo.write(SERVO_CW);
        stateStartTime = millis();
        currentState = STATE_RETURNING;
      }
      break;

    case STATE_RETURNING:
      if (millis() - stateStartTime >= MOVE_DURATION) {
        myServo.write(SERVO_STOP);
        currentState = STATE_IDLE;
      }
      break;
  }

  // ================= STATE MACHINE SERVO 2 =================
  switch (currentState2) {
    case STATE2_IDLE:
      // servo 2 hanya boleh mulai siklus baru kalau servo 1 sedang IDLE
      if (button2PressedEvent && currentState == STATE_IDLE) {
        myServo2.write(SERVO2_TARGET_ANGLE);
        stateStartTime2 = millis();
        currentState2 = STATE2_MOVING_TO_TARGET;
      }
      break;

    case STATE2_MOVING_TO_TARGET:
      if (millis() - stateStartTime2 >= SERVO2_TRAVEL_TIME) {
        stateStartTime2 = millis();
        currentState2 = STATE2_HOLDING;
      }
      break;

    case STATE2_HOLDING:
      if (millis() - stateStartTime2 >= SERVO2_HOLD_DELAY) {
        myServo2.write(SERVO2_HOME_ANGLE);
        stateStartTime2 = millis();
        currentState2 = STATE2_RETURNING_HOME;
      }
      break;

    case STATE2_RETURNING_HOME:
      if (millis() - stateStartTime2 >= SERVO2_TRAVEL_TIME) {
        currentState2 = STATE2_IDLE;
      }
      break;
  }
}

// Mengembalikan true SEKALI setiap ada tap baru (falling edge, dengan debounce)
bool readButtonPressed(int pin, bool &lastReading, bool &stableState, unsigned long &lastDebounceTime) {
  bool reading = digitalRead(pin);
  bool pressedEvent = false;

  if (reading != lastReading) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != stableState) {
      stableState = reading;
      if (stableState == LOW) {
        pressedEvent = true;
      }
    }
  }

  lastReading = reading;
  return pressedEvent;
}