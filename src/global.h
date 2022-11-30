/**
 * @file global.h
 * @author Martin Verges <martin@verges.cc>
 * @version 0.1
 * @date 2022-07-09
 * 
 * @copyright Copyright (c) 2022 by the author alone
 *            https://gitlab.womolin.de/martin.verges/ogo-ttt-fan-upgrade
 * 
 * License: CC BY-NC-SA 4.0
 */

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <FS.h>
#include <LittleFS.h>
#include <Preferences.h>
#include "MQTTclient.h"
#include "wifimanager.h"

#define webserverPort 80                    // Start the Webserver on this port
#define NVS_NAMESPACE "ogotoilet"           // Preferences.h namespace to store settings

#include <SPI.h>
#include <Wire.h>

#define uS_TO_S_FACTOR   1000000           // Conversion factor for micro seconds to seconds
#define TIME_TO_SLEEP    10                // WakeUp interval


const int TACHO_PIN = GPIO_NUM_25;                // Digital Input Pin
const int DPLUS_PIN = GPIO_NUM_35;                // Digital Input Pin
const int SPEED_PIN = GPIO_NUM_34;                // Potentiometer ADC Pin

const int DHT22_PIN = GPIO_NUM_26;                // DHT22 Data Pin

const int PWM_PIN = GPIO_NUM_0;                   // PWM Pin
const int PWM_CHANNEL = 0;                        // PWM Channel to assign
const int PWM_MAX_DUTY_CYCLE = (int)(pow(2, 10)-1); // 10 Bit == 1024

const int MIXER_STATUS_PIN = GPIO_NUM_33;         // Digital Input Pin
const int MIXER_START_PIN = GPIO_NUM_27;          // Digital Output Pin to activate transistor

unsigned long runMixerAfter = 12*60*60*1000;      // Automatically run the MIXER after some time (12h)
unsigned long lastMixerRun = 0;                   // Last time the MIXER was active
int8_t noMixerBelowTempC = 10;                    // Temperature under which the mixer won't run to prevent damage
unsigned long lastTachoInterrupt = 0;             // Microseconds of the last TACHO interrupt (pull down)
unsigned int tachoDelay = 0;
unsigned int targetPwmSpeed = PWM_MAX_DUTY_CYCLE * 0.25; // 0-1023 equals 0-100%, default to 25% speed

bool otaRunning = false;

RTC_DATA_ATTR struct timing_t {
  // Check Services like MQTT, ...
  uint64_t lastServiceCheck = 0;               // last millis() from ServiceCheck
  const unsigned int serviceInterval = 30000;  // Interval in ms to execute code

  // Status report on serial console
  uint64_t lastStatusUpdate = 0;                  // last millis() from Status report
  const unsigned int statusUpdateInterval = 5000; // Interval in ms to execute code

  // Check potentiometer and update fan speed
  uint64_t lastSpeedUpdate = 0;                  // last millis() from Speed update
  const unsigned int speedUpdateInterval = 250;  // Interval in ms to execute code

  // Check mixer usage
  uint64_t lastMixerUpdate = 0;                  // last millis() from Status report
  const unsigned int mixerUpdateInterval = 250;  // Interval in ms to execute code
} Timing;

RTC_DATA_ATTR uint64_t sleepTime = 0;       // Time that the esp32 slept

WIFIMANAGER WifiManager;
bool enableWifi = true;                     // Enable Wifi, disable to reduce power consumtion, stored in NVS

struct Button {
  const gpio_num_t PIN;
  bool pressed;
};
Button button1 = {GPIO_NUM_14, false};       // Run the setup (use a RTC GPIO)
void IRAM_ATTR ISR_button1() {
  button1.pressed = true;
}

String hostName;
AsyncWebServer webServer(webserverPort);
AsyncEventSource events("/api/events");
Preferences preferences;

MQTTclient Mqtt;

// Current system runtime in MS
uint64_t runtime() {
  return rtc_time_slowclk_to_us(rtc_time_get(), esp_clk_slowclk_cal_get()) / 1000;
}

void deepsleepForSeconds(int seconds) {
    esp_sleep_enable_timer_wakeup(seconds * uS_TO_S_FACTOR);
    esp_deep_sleep_start();
}

void print_wakeup_reason() {
  esp_sleep_wakeup_cause_t wakeup_reason;
  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch(wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT0 : 
      LOG_INFO_LN(F("[POWER] Wakeup caused by external signal using RTC_IO"));
      button1.pressed = true;
    break;
    case ESP_SLEEP_WAKEUP_EXT1 : LOG_INFO_LN(F("[POWER] Wakeup caused by external signal using RTC_CNTL")); break;
    case ESP_SLEEP_WAKEUP_TIMER : 
      LOG_INFO_LN(F("[POWER] Wakeup caused by timer"));
      uint64_t timeNow, timeDiff;
      timeNow = rtc_time_slowclk_to_us(rtc_time_get(), esp_clk_slowclk_cal_get());
      timeDiff = timeNow - sleepTime;
      printf("Now: %" PRIu64 "ms, Duration: %" PRIu64 "ms\n", timeNow / 1000, timeDiff / 1000);
      delay(2000);
    break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : LOG_INFO_LN(F("[POWER] Wakeup caused by touchpad")); break;
    case ESP_SLEEP_WAKEUP_ULP : LOG_INFO_LN(F("[POWER] Wakeup caused by ULP program")); break;
    default : LOG_INFO_F("[POWER] Wakeup was not caused by deep sleep: %d\n",wakeup_reason); break;
  }
}

// Check if a feature is enabled, that prevents the
// deep sleep mode of our ESP32 chip.
void sleepOrDelay() {
  if (enableWifi || enableMqtt) {
    yield();
    delay(50);
  } else {
    // We can save a lot of power by going into deepsleep
    // Thid disables WIFI and everything.
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
    sleepTime = rtc_time_slowclk_to_us(rtc_time_get(), esp_clk_slowclk_cal_get());
    rtc_gpio_pullup_en(button1.PIN);
    rtc_gpio_pulldown_dis(button1.PIN);
    esp_sleep_enable_ext0_wakeup(button1.PIN, 0);

    preferences.end();
    LOG_INFO_LN(F("[POWER] Sleeping..."));
    esp_deep_sleep_start();
  }
}

const uint8_t mixerTimerID = 0;
hw_timer_t *MixerTimer = NULL;
void IRAM_ATTR _endMixerOutputPin() {
  LOG_INFO_LN("_endMixerOutputPin()");
  digitalWrite(MIXER_START_PIN, LOW);
  if (MixerTimer) {
    timerEnd(MixerTimer);
    MixerTimer = NULL;
  }
}
void activateMixer() {
  LOG_INFO_LN("activateMixer()");
  digitalWrite(MIXER_START_PIN, HIGH);

  if (MixerTimer == NULL) {
    MixerTimer = timerBegin(mixerTimerID, 80, true);
    timerAttachInterrupt(MixerTimer, &_endMixerOutputPin, true);
    timerAlarmWrite(MixerTimer, 500000, true); // 1.000.000 == 1s
    timerAlarmEnable(MixerTimer);
  }
}
