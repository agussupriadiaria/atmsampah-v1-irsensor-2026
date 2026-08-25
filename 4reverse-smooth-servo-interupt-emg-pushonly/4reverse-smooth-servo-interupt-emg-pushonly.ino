// versi: 2 - INTERRUPT ENABLED
// Reverse smooth servo - servos dapat bergerak simultan
// refresh 4

#include <Servo.h>

const int BUTTON_PIN1   = 9;  // pin raspi 5 = trigger servo continuous
const int BUTTON_PIN2   = 10; // pin raspi 6
const int BUTTON_PIN3   = 11; // pin raspi 16
const int IR_SENSOR_PIN = 12; // IR reset position
const int SERVO_PIN1    = 4;  // servo continuous rotation
const int SERVO_PIN2    = 5;  // servo positional 180 derajat

const int SERVO_STOP = 90;    // netral (berhenti) - servo 1
const int SERVO_CW   = 180;   // putar CW - servo 1
const int SERVO_CCW  = 0;     // putar CCW - servo 1

// Catatan asumsi sensor IR:
// IR_SENSOR_PIN == HIGH -> BELUM di posisi awal -> servo1 CCW
// IR_SENSOR_PIN == LOW  -> SUDAH di posisi awal -> servo1 berhenti
//
// Jika modul sensor IR mempunyai polaritas terbalik,
// kondisi pembacaan irSensorHigh perlu dibalik.

// ==== Durasi servo 1 ====
const unsigned long MOVE_OUT_DURATION       = 1250; // durasi gerak CW menjauh dari home (normal)
const unsigned long MOVE_OUT_SHORT_DURATION = 250;  // durasi gerak CW dari interrupt homing
const unsigned long MOVE_OUT_DELAY          = 500;  // delay antara gerak CW
const unsigned long HOLD_DELAY_BTN1         = 5000; // jeda setelah CW, BUTTON_PIN1
const unsigned long HOLD_DELAY_BTN3         = 8000; // jeda setelah CW, BUTTON_PIN3

// ==== Konfigurasi Servo 2 (MG996R positional 180 derajat) ====
const int SERVO2_HOME_ANGLE   = 180;  // posisi awal
const int SERVO2_TARGET_ANGLE = 90;   // posisi target

// Kecepatan gerakan Servo 2
// 10 ms = setiap perubahan 1 derajat diberi jeda 10 ms
// Semakin besar nilainya = semakin lambat
const unsigned long SERVO2_STEP_DELAY = 10;

const unsigned long SERVO2_HOLD_DELAY = 2000; // tahan 2 detik di posisi target


// ================= STATE MACHINE SERVO 1 =================

enum Servo1State {
  S1_HOMING,         // mencari posisi awal
  S1_IDLE,           // diam di posisi awal
  S1_MOVING_OUT,     // bergerak CW menjauh dari home
  S1_HOLDING         // menahan posisi
};


// ================= STATE MACHINE SERVO 2 =================

enum Servo2State {
  S2_IDLE,
  S2_MOVING_TO_TARGET,
  S2_HOLDING,
  S2_RETURNING_HOME
};


Servo1State currentState1 = S1_HOMING;
Servo2State currentState2 = S2_IDLE;

unsigned long stateStartTime1 = 0;
unsigned long stateStartTime2 = 0;

unsigned long holdDuration1 = 0;

// ================= SERVO 1 FLAGS =================

bool isShortMoveInterrupt = false; // flag untuk short move dari interrupt homing
bool isInDelayPhase = false;       // flag untuk tahu sedang dalam delay 500ms
int moveCounter = 0;               // counter untuk accumulate gerak CW


// ================= BUTTON 1 =================

bool lastButtonReading1 = HIGH;
bool buttonStableState1 = HIGH;
unsigned long lastDebounceTime1 = 0;


// ================= BUTTON 2 =================

bool lastButtonReading2 = HIGH;
bool buttonStableState2 = HIGH;
unsigned long lastDebounceTime2 = 0;


// ================= BUTTON 3 =================

bool lastButtonReading3 = HIGH;
bool buttonStableState3 = HIGH;
unsigned long lastDebounceTime3 = 0;

const unsigned long debounceDelay = 50;


// ================= SERVO =================

Servo myServo;   // Servo 1 - continuous rotation
Servo myServo2;  // Servo 2 - MG996R positional


// ================= SERVO 2 SMOOTH MOVEMENT =================

// Posisi aktual Servo 2
int servo2CurrentAngle = SERVO2_HOME_ANGLE;

// Target posisi Servo 2
int servo2TargetAngle = SERVO2_HOME_ANGLE;

// Waktu terakhir Servo 2 bergerak 1 derajat
unsigned long servo2LastStepTime = 0;


// ================= SETUP =================

void setup() {

  pinMode(BUTTON_PIN1, INPUT_PULLUP);
  pinMode(BUTTON_PIN2, INPUT_PULLUP);
  pinMode(BUTTON_PIN3, INPUT_PULLUP);

  pinMode(IR_SENSOR_PIN, INPUT);

  // Servo 1
  myServo.attach(SERVO_PIN1);
  myServo.write(SERVO_STOP);

  // Servo 2
  myServo2.attach(SERVO_PIN2);

  // Posisi awal Servo 2 = 180 derajat
  myServo2.write(SERVO2_HOME_ANGLE);

  servo2CurrentAngle = SERVO2_HOME_ANGLE;
  servo2TargetAngle = SERVO2_HOME_ANGLE;
}


// ================= LOOP =================

void loop() {

  bool button1PressedEvent =
    readButtonPressed(
      BUTTON_PIN1,
      lastButtonReading1,
      buttonStableState1,
      lastDebounceTime1
    );

  bool button2PressedEvent =
    readButtonPressed(
      BUTTON_PIN2,
      lastButtonReading2,
      buttonStableState2,
      lastDebounceTime2
    );

  bool button3PressedEvent =
    readButtonPressed(
      BUTTON_PIN3,
      lastButtonReading3,
      buttonStableState3,
      lastDebounceTime3
    );


  // ================= IR SENSOR =================

  bool irSensorHigh =
    (digitalRead(IR_SENSOR_PIN) == HIGH);


  // =========================================================
  // STATE MACHINE SERVO 1
  // =========================================================

  switch (currentState1) {

    case S1_HOMING:

      // ===== BUTTON 1 DITEKAN DI S1_HOMING =====
      if (button1PressedEvent) {

        // Jika IR LOW = kondisi awal, gerak CW 1250ms normal
        if (!irSensorHigh) {

          isShortMoveInterrupt = false;  // ← Normal move
          holdDuration1 = HOLD_DELAY_BTN1;

          myServo.write(SERVO_CW);

          stateStartTime1 = millis();

          currentState1 = S1_MOVING_OUT;

        } 
        // Jika IR HIGH = sedang mendekat home, gerak CW 250ms accumulation
        else {

          isShortMoveInterrupt = true;   // ← Short move dengan accumulation
          isInDelayPhase = false;
          moveCounter = 1;

          myServo.write(SERVO_CW);

          stateStartTime1 = millis();

          currentState1 = S1_MOVING_OUT;
        }

        break;
      }


      // ===== NORMAL HOMING LOGIC =====
      if (irSensorHigh) {

        myServo.write(SERVO_CCW);

      } else {

        // IR LOW = posisi home tercapai
        myServo.write(SERVO_STOP);

        currentState1 = S1_IDLE;
      }

      break;


    case S1_IDLE:

      myServo.write(SERVO_STOP);

      // Jika IR tiba-tiba HIGH, lakukan homing ulang
      if (irSensorHigh) {

        currentState1 = S1_HOMING;

        break;
      }


      // ===== HANDLE BUTTON PRESS =====
      // Jika servo sedang idle (diam), Button 1 → trigger homing
      if (button1PressedEvent) {

        // Button 1 dari S1_IDLE → kembali ke S1_HOMING
        currentState1 = S1_HOMING;

        break;
      }

      // Button 3 → normal gerak CW 1250ms
      if (button3PressedEvent) {

        isShortMoveInterrupt = false;  // Normal move, bukan short

        holdDuration1 = HOLD_DELAY_BTN3;

        myServo.write(SERVO_CW);

        stateStartTime1 = millis();

        currentState1 = S1_MOVING_OUT;
      }

      break;


    case S1_MOVING_OUT:
    {
      // ===== CHECK IR SENSOR =====
      // Jika IR LOW (sudah di home) → langsung stop & S1_IDLE
      if (!irSensorHigh) {

        myServo.write(SERVO_STOP);

        isShortMoveInterrupt = false;
        isInDelayPhase = false;
        moveCounter = 0;

        currentState1 = S1_IDLE;

        break;
      }


      // =========================================================
      // CASE 1: SHORT MOVE (250ms dengan ACCUMULATION dari homing)
      // =========================================================
      if (isShortMoveInterrupt) {

        // ===== PHASE: GERAK CW 250ms =====
        if (!isInDelayPhase) {

          if (millis() - stateStartTime1 >= MOVE_OUT_SHORT_DURATION) {

            // Selesai gerak CW 250ms
            myServo.write(SERVO_STOP);

            isInDelayPhase = true;  // Enter delay phase

            stateStartTime1 = millis();  // Reset timer untuk delay

            moveCounter--;  // Kurangi counter
          }
        }

        // ===== PHASE: DELAY 500ms (menunggu button press atau timeout) =====
        else if (isInDelayPhase) {

          // Button 1 ditekan lagi saat delay → accumulate gerak CW
          if (button1PressedEvent) {

            moveCounter++;  // Tambah counter

            isInDelayPhase = false;  // Kembali ke move phase

            myServo.write(SERVO_CW);  // Gerak CW lagi

            stateStartTime1 = millis();  // Reset timer untuk gerak berikutnya

          }

          // Delay 500ms selesai tanpa press button
          else if (millis() - stateStartTime1 >= MOVE_OUT_DELAY) {

            // Cek apakah masih ada counter (perlu gerak lagi)
            if (moveCounter > 0) {

              isInDelayPhase = false;  // Kembali ke move phase

              myServo.write(SERVO_CW);  // Gerak CW lagi

              stateStartTime1 = millis();

            } else {

              // Counter = 0, selesai accumulation, balik S1_HOMING
              isShortMoveInterrupt = false;
              isInDelayPhase = false;

              currentState1 = S1_HOMING;
            }
          }
        }
      }


      // =========================================================
      // CASE 2: NORMAL MOVE (1250ms TANPA accumulation dari idle)
      // =========================================================
      else {

        if (millis() - stateStartTime1 >= MOVE_OUT_DURATION) {

          // Selesai gerak CW 1250ms
          myServo.write(SERVO_STOP);

          // Langsung ke S1_IDLE, diam di posisi (tunggu button press lagi)
          currentState1 = S1_IDLE;
        }
      }

      break;
    }


    // case S1_HOLDING tidak lagi digunakan
    // Setelah gerak normal 1250ms, langsung S1_IDLE

  }


  // =========================================================
  // STATE MACHINE SERVO 2
  // =========================================================

  switch (currentState2) {


    // =======================================================
    // SERVO 2 IDLE
    // =======================================================

    case S2_IDLE:

      // BUTTON 2 ditekan - langsung execute tanpa queue
      if (button2PressedEvent) {

        servo2TargetAngle = SERVO2_TARGET_ANGLE;

        servo2LastStepTime = millis();

        currentState2 = S2_MOVING_TO_TARGET;
      }

      break;


    // =======================================================
    // SERVO 2 BERGERAK SMOOTH KE TARGET
    // =======================================================

    case S2_MOVING_TO_TARGET:

      updateServo2Smooth();

      // Jika sudah sampai target
      if (servo2CurrentAngle == servo2TargetAngle) {

        stateStartTime2 = millis();

        currentState2 = S2_HOLDING;
      }

      break;


    // =======================================================
    // SERVO 2 MENAHAN POSISI 90 DERAJAT
    // =======================================================

    case S2_HOLDING:

      if (millis() - stateStartTime2 >= SERVO2_HOLD_DELAY) {

        // Setelah 2 detik,
        // Servo 2 kembali ke 180 derajat

        servo2TargetAngle = SERVO2_HOME_ANGLE;

        servo2LastStepTime = millis();

        currentState2 = S2_RETURNING_HOME;
      }

      break;


    // =======================================================
    // SERVO 2 KEMBALI SMOOTH KE HOME
    // =======================================================

    case S2_RETURNING_HOME:

      updateServo2Smooth();

      // Jika sudah sampai 180 derajat
      if (servo2CurrentAngle == servo2TargetAngle) {

        currentState2 = S2_IDLE;
      }

      break;
  }
}


// =============================================================
// FUNGSI GERAK SMOOTH SERVO 2
// =============================================================

void updateServo2Smooth() {

  unsigned long currentTime = millis();


  // Belum waktunya pindah 1 derajat
  if (currentTime - servo2LastStepTime < SERVO2_STEP_DELAY) {

    return;
  }


  servo2LastStepTime = currentTime;


  // Bergerak menuju sudut yang lebih besar
  if (servo2CurrentAngle < servo2TargetAngle) {

    servo2CurrentAngle++;

    myServo2.write(servo2CurrentAngle);
  }


  // Bergerak menuju sudut yang lebih kecil
  else if (servo2CurrentAngle > servo2TargetAngle) {

    servo2CurrentAngle--;

    myServo2.write(servo2CurrentAngle);
  }
}


// =============================================================
// BUTTON DEBOUNCE
// =============================================================

// Mengembalikan TRUE satu kali setiap ada tap/tombol baru
// Menggunakan falling edge dengan debounce

bool readButtonPressed(
  int pin,
  bool &lastReading,
  bool &stableState,
  unsigned long &lastDebounceTime
) {

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
