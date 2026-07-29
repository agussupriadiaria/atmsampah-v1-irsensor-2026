#include <Servo.h>

const int BUTTON_PIN     = 2;
const int IR_SENSOR_PIN  = 3;
const int SERVO_PIN      = 9;

const int SERVO_STOP = 90;   // netral (berhenti)
const int SERVO_CW   = 180;  // putar searah jarum jam (menuju posisi tertentu)
const int SERVO_CCW  = 0;    // putar berlawanan arah (balik ke posisi awal / homing)

const unsigned long MOVE_DURATION = 500; // 2 detik gerak menuju posisi tertentu
const unsigned long HOLD_DELAY    = 2000; // jeda 3 detik di posisi tersebut

enum State {
  STATE_IDLE,
  STATE_HOMING,          // IR masih LOW -> servo CCW sampai IR HIGH
  STATE_MOVING_FORWARD,
  STATE_HOLDING,
  STATE_RETURNING
};

State currentState = STATE_IDLE;
unsigned long stateStartTime = 0;

// Debounce & edge-detect tombol (tap, dihitung 1x saja)
bool lastButtonReading = HIGH;
bool buttonStableState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

Servo myServo;

void setup() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(IR_SENSOR_PIN, INPUT); // asumsi: output IR sensor push-pull, ganti ke INPUT_PULLUP jika perlu
  myServo.attach(SERVO_PIN);
  myServo.write(SERVO_STOP);
}

void loop() {
  bool buttonPressedEvent = readButtonPressed();
  bool irHigh = (digitalRead(IR_SENSOR_PIN) == LOW);

  switch (currentState) {
    case STATE_IDLE:
      if (!irHigh) {
        // IR masih LOW -> mulai homing (servo CW)
        myServo.write(SERVO_CW);
        currentState = STATE_HOMING;
        break;
      }
      // IR sudah HIGH -> logic tombol normal berlaku
      myServo.write(SERVO_STOP);
      if (buttonPressedEvent) {
        myServo.write(SERVO_CCW);
        stateStartTime = millis();
        currentState = STATE_MOVING_FORWARD;
      }
      break;

    case STATE_HOMING:
      // Tombol diabaikan di sini, terus CCW sampai IR HIGH
      if (irHigh) {
        myServo.write(SERVO_STOP);
        currentState = STATE_IDLE; // balik ke idle, siap cek tombol
      }
      break;

    case STATE_MOVING_FORWARD:
      // Tombol diabaikan di sini, siklus tetap lanjut sampai selesai
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
        currentState = STATE_IDLE;  // balik ke idle, IR akan dicek lagi di iterasi berikutnya
      }
      break;
  }
}

// Mengembalikan true SEKALI setiap ada tap baru (falling edge, dengan debounce)
bool readButtonPressed() {
  bool reading = digitalRead(BUTTON_PIN);
  bool pressedEvent = false;

  if (reading != lastButtonReading) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != buttonStableState) {
      buttonStableState = reading;
      if (buttonStableState == LOW) {
        pressedEvent = true;
      }
    }
  }

  lastButtonReading = reading;
  return pressedEvent;
}