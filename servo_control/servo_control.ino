#include <Servo.h>

const int BUTTON_PIN1   = 9; // pin raspi 5 = trigger servo continues
const int BUTTON_PIN2   = 10; // pin raspi 6
const int BUTTON_PIN3   = 11;  // pin raspi 16
const int IR_SENSOR_PIN = 12; // ir reset posisition
const int SERVO_PIN1    = 4;  // servo continuous rotation
const int SERVO_PIN2    = 5;  // servo positional 180 derajat

const int SERVO_STOP = 90;    // netral (berhenti) - servo 1
const int SERVO_CW   = 180;   // putar CW - servo 1
const int SERVO_CCW  = 0;     // putar CCW - servo 1

// Catatan asumsi sensor IR (sesuai spesifikasi terbaru):
//   - IR_SENSOR_PIN == HIGH  -> BELUM di posisi awal -> servo1 CCW (mencari home)
//   - IR_SENSOR_PIN == LOW   -> SUDAH di posisi awal  -> servo1 berhenti (home tercapai)
// Jika ternyata modul sensor kamu punya polaritas terbalik, tinggal balik kondisi
// pembacaan "irSensorHigh" di bawah.

// ==== Durasi servo 1 ====
const unsigned long MOVE_OUT_DURATION = 2000; // durasi gerak CW menjauh dari home (dipicu tombol)
const unsigned long HOLD_DELAY_BTN1   = 2000; // jeda setelah CW, untuk BUTTON_PIN1
const unsigned long HOLD_DELAY_BTN3   = 8000; // jeda setelah CW, untuk BUTTON_PIN3

// ==== Konfigurasi servo 2 (positional 180 derajat) ====
const int SERVO2_HOME_ANGLE   = 0;    // posisi awal
const int SERVO2_TARGET_ANGLE = 45;   // posisi target
const unsigned long SERVO2_TRAVEL_TIME = 300;  // asumsi waktu tempuh fisik servo, sesuaikan jika perlu
const unsigned long SERVO2_HOLD_DELAY  = 2000; // jeda 2 detik di posisi target

// ================= STATE MACHINE SERVO 1 =================
enum Servo1State {
  S1_HOMING,         // mencari posisi awal: CCW selama IR HIGH, berhenti saat IR LOW
  S1_IDLE,           // diam di posisi awal (IR LOW), menunggu tombol
  S1_MOVING_OUT,     // CW menjauh dari home (2 detik), dipicu tombol 1 / tombol 3
  S1_HOLDING         // menahan posisi, durasi tergantung tombol pemicu
};

// ================= STATE MACHINE SERVO 2 =================
enum Servo2State {
  S2_IDLE,
  S2_MOVING_TO_TARGET,
  S2_HOLDING,
  S2_RETURNING_HOME
};

Servo1State currentState1 = S1_HOMING; // mulai dengan homing supaya servo1 mencari posisi awal saat boot
Servo2State currentState2 = S2_IDLE;

unsigned long stateStartTime1 = 0;
unsigned long stateStartTime2 = 0;
unsigned long holdDuration1   = 0; // diisi HOLD_DELAY_BTN1 atau HOLD_DELAY_BTN3 tergantung pemicu

// Debounce & edge-detect Button 1
bool lastButtonReading1 = HIGH;
bool buttonStableState1 = HIGH;
unsigned long lastDebounceTime1 = 0;

// Debounce & edge-detect Button 2
bool lastButtonReading2 = HIGH;
bool buttonStableState2 = HIGH;
unsigned long lastDebounceTime2 = 0;

// Debounce & edge-detect Button 3
bool lastButtonReading3 = HIGH;
bool buttonStableState3 = HIGH;
unsigned long lastDebounceTime3 = 0;

const unsigned long debounceDelay = 50;

Servo myServo;   // servo 1 (continuous rotation)
Servo myServo2;  // servo 2 (positional 180 derajat)

void setup() {
  pinMode(BUTTON_PIN1, INPUT_PULLUP);
  pinMode(BUTTON_PIN2, INPUT_PULLUP);
  pinMode(BUTTON_PIN3, INPUT_PULLUP);
  pinMode(IR_SENSOR_PIN, INPUT); // asumsi: output IR sensor push-pull

  myServo.attach(SERVO_PIN1);
  myServo.write(SERVO_STOP);

  myServo2.attach(SERVO_PIN2);
  myServo2.write(SERVO2_HOME_ANGLE); // posisi awal 0 derajat
}

void loop() {
  bool button1PressedEvent = readButtonPressed(BUTTON_PIN1, lastButtonReading1, buttonStableState1, lastDebounceTime1);
  bool button2PressedEvent = readButtonPressed(BUTTON_PIN2, lastButtonReading2, buttonStableState2, lastDebounceTime2);
  bool button3PressedEvent = readButtonPressed(BUTTON_PIN3, lastButtonReading3, buttonStableState3, lastDebounceTime3);

  // Nama variabel langsung merepresentasikan kondisi fisik pin (tidak dibalik),
  // supaya tidak menyesatkan seperti sebelumnya.
  bool irSensorHigh = (digitalRead(IR_SENSOR_PIN) == HIGH);

  // ================= STATE MACHINE SERVO 1 =================
  switch (currentState1) {

    case S1_HOMING:
      // Selama IR masih HIGH (belum di posisi awal), terus putar CCW mencari home.
      if (irSensorHigh) {
        myServo.write(SERVO_CCW);
      } else {
        // IR sudah LOW -> posisi awal tercapai -> berhenti
        myServo.write(SERVO_STOP);
        currentState1 = S1_IDLE;
      }
      break;

    case S1_IDLE:
      myServo.write(SERVO_STOP);

      // Safety: kalau tiba-tiba IR terbaca HIGH lagi saat idle, cari home ulang.
      if (irSensorHigh) {
        currentState1 = S1_HOMING;
        break;
      }

      // Servo 1 hanya boleh mulai siklus baru kalau servo 2 sedang idle (interlock).
      if ((button1PressedEvent || button3PressedEvent) && currentState2 == S2_IDLE) {
        holdDuration1 = button1PressedEvent ? HOLD_DELAY_BTN1 : HOLD_DELAY_BTN3;
        myServo.write(SERVO_CW);
        stateStartTime1 = millis();
        currentState1 = S1_MOVING_OUT;
      }
      break;

    case S1_MOVING_OUT:
      if (millis() - stateStartTime1 >= MOVE_OUT_DURATION) {
        myServo.write(SERVO_STOP);
        stateStartTime1 = millis();
        currentState1 = S1_HOLDING;
      }
      break;

    case S1_HOLDING:
      if (millis() - stateStartTime1 >= holdDuration1) {
        // Kembali ke posisi awal berbasis feedback sensor IR (bukan waktu tetap).
        // IR seharusnya sudah HIGH lagi (karena tadi bergerak menjauh dari home),
        // sehingga langsung masuk proses homing (CCW sampai IR LOW).
        currentState1 = S1_HOMING;
      }
      break;
  }

  // ================= STATE MACHINE SERVO 2 =================
  switch (currentState2) {
    case S2_IDLE:
      // Servo 2 hanya boleh mulai siklus baru kalau servo 1 sedang IDLE (interlock).
      if (button2PressedEvent && currentState1 == S1_IDLE) {
        myServo2.write(SERVO2_TARGET_ANGLE);
        stateStartTime2 = millis();
        currentState2 = S2_MOVING_TO_TARGET;
      }
      break;

    case S2_MOVING_TO_TARGET:
      if (millis() - stateStartTime2 >= SERVO2_TRAVEL_TIME) {
        stateStartTime2 = millis();
        currentState2 = S2_HOLDING;
      }
      break;

    case S2_HOLDING:
      if (millis() - stateStartTime2 >= SERVO2_HOLD_DELAY) {
        myServo2.write(SERVO2_HOME_ANGLE);
        stateStartTime2 = millis();
        currentState2 = S2_RETURNING_HOME;
      }
      break;

    case S2_RETURNING_HOME:
      if (millis() - stateStartTime2 >= SERVO2_TRAVEL_TIME) {
        currentState2 = S2_IDLE;
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
