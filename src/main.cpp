#include "Arduino.h"

#define TACHO_PIN 2    // D2 Digital I/O Pin
#define PWM_PIN 9      // D9 PWM / Digital I/O Pin
#define DPLUS_PIN 4    // D4 Digital I/O Pin
#define SPEED_PIN A6   // A6 Analog Input Channel 6

unsigned long last_stats = 0;
unsigned long tacho_last_call = 0;
unsigned int tacho_delay = 0;
byte target_pwm_speed = 64; // 0-255 equals 0-100%

void tacho() {
  unsigned long current_millis = micros();
  tacho_delay = (current_millis - tacho_last_call);
  tacho_last_call = current_millis;
}

void update_target_speed() {
  // If the engine is running, we have a D+ signal on the DPLUS_PIN using a voltage devider ~12 to ~3V
  // When the signal is running, the fan should run on 100% speed to improve toilet drying
  bool dplus = digitalRead(DPLUS_PIN);
  if (dplus) {
    Serial.println("DPLUS active, max power!");
    target_pwm_speed = 255;
  } else {
    unsigned int val = analogRead(SPEED_PIN);
    Serial.print("Poti = ");
    Serial.println(val);
    target_pwm_speed = map(val, 0, 1023, 0, 255);
  }
  analogWrite(PWM_PIN, target_pwm_speed);
}

// PWM-Pins      3, 5, 6, 9, 10, 11
// PWM Freqency  490 Hz (Pins 5 and 6: 980 Hz)
void setup() {
  Serial.begin(115200);

  pinMode(DPLUS_PIN, INPUT);

  pinMode(TACHO_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(TACHO_PIN), tacho, FALLING);

  // Timer 1: Prescaler 0
  TCCR1B = (TCCR1B & B11111000) | B00000001;

  // run PWM on 25% on startup
  analogWrite(PWM_PIN, target_pwm_speed);
}

void loop() {
  unsigned long current_millis = millis();
  
  if(last_stats > current_millis || (last_stats + 1000) < current_millis) {
    update_target_speed();

    Serial.print("Delay: ");
    Serial.print(tacho_delay);
    Serial.println("µs");

    unsigned long freq = 100000000 / tacho_delay;
    Serial.print("Frequency: ");
    Serial.print(freq/100);
    Serial.print('.');
    Serial.print(freq%100);
    Serial.println("Hz");

    freq *= 60;
    freq /= 200;
    Serial.print("RPM: ");
    Serial.println(freq);
    
    Serial.print("Target: ");
    Serial.print(map(target_pwm_speed, 0, 255, 0, 100));
    Serial.println('%');

    Serial.println("-----");
    last_stats = millis();
  }
  delay(100);
}
