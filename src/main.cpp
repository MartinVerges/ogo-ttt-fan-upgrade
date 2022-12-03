/**
 * @file main.cpp
 * @author Martin Verges <martin@verges.cc>
 * @brief OGO Toilet Tuning
 * @version 0.1
 * @date 2022-11-06
 * 
 * @copyright Copyright (c) 2022 by the author alone
 *            https://gitlab.womolin.de/martin.verges/ogo-ttt-fan-upgrade
 * 
 * License: CC BY-NC-SA 4.0
 */

#if !(defined(ESP32))
  #error This code is intended to run on the ESP32 platform! Please check your Tools->Board setting.
#endif

#undef USE_LittleFS
#define USE_LittleFS true

#include "log.h"

#include <Arduino.h>
#include <AsyncTCP.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

// Power Management
#include <driver/rtc_io.h>
#include <esp_sleep.h>
#include <soc/rtc.h>
#include <esp32/clk.h>

#include "global.h"
#include "api-routes.h"

// ESP32 PWM functions
#include "driver/ledc.h"

// DHT22 Temperature and Humidity
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

// Read from memory instead of register (updated by thread)
volatile float currentHumidity = 0;
volatile float currentTemperature = 0;

WebSerialClass WebSerial;
bool stateMixer = false;
bool stateDplus = false;
uint8_t statePoti = 0;

bool stateDehumidification = false;

// extern uint8_t __analogReturnedWidth;
static uint16_t MAX_ADC_VALUE = 4096; // pow(2, __analogReturnedWidth);

void DHT_task(void *pvParameter) {
  DHT_Unified dht(DHT22_PIN, DHT22);
  dht.begin();

  // Print temperature sensor details.
  sensor_t sensor;
  dht.temperature().getSensor(&sensor);
  LOG_INFO_LN(F("[DHT22] ------------------------------------"));
  LOG_INFO_LN(F("[DHT22] Temperature Sensor"));
  LOG_INFO  (F("[DHT22] Sensor Type: ")); LOG_INFO_LN(sensor.name);
  LOG_INFO  (F("[DHT22] Driver Ver:  ")); LOG_INFO_LN(sensor.version);
  LOG_INFO  (F("[DHT22] Unique ID:   ")); LOG_INFO_LN(sensor.sensor_id);
  LOG_INFO  (F("[DHT22] Max Value:   ")); LOG_INFO(sensor.max_value);  LOG_INFO_LN(F("°C"));
  LOG_INFO  (F("[DHT22] Min Value:   ")); LOG_INFO(sensor.min_value);  LOG_INFO_LN(F("°C"));
  LOG_INFO  (F("[DHT22] Resolution:  ")); LOG_INFO(sensor.resolution); LOG_INFO_LN(F("°C"));
  LOG_INFO_LN(F("------------------------------------"));
  // Print humidity sensor details.
  dht.humidity().getSensor(&sensor);
  LOG_INFO_LN(F("[DHT22] Humidity Sensor"));
  LOG_INFO  (F("[DHT22] Sensor Type: ")); LOG_INFO_LN(sensor.name);
  LOG_INFO  (F("[DHT22] Driver Ver:  ")); LOG_INFO_LN(sensor.version);
  LOG_INFO  (F("[DHT22] Unique ID:   ")); LOG_INFO_LN(sensor.sensor_id);
  LOG_INFO  (F("[DHT22] Max Value:   ")); LOG_INFO(sensor.max_value);  LOG_INFO_LN(F("%"));
  LOG_INFO  (F("[DHT22] Min Value:   ")); LOG_INFO(sensor.min_value);  LOG_INFO_LN(F("%"));
  LOG_INFO  (F("[DHT22] Resolution:  ")); LOG_INFO(sensor.resolution); LOG_INFO_LN(F("%"));
  LOG_INFO_LN(F("------------------------------------"));
  // Set delay between sensor readings based on sensor details.

  while(1) {
    sensors_event_t event;
    dht.temperature().getEvent(&event);
    if (isnan(event.temperature)) {
      LOG_INFO_LN(F("[DHT22] Error reading temperature!"));
    } else {
      currentTemperature = event.temperature;
    }

    dht.humidity().getEvent(&event);
    if (isnan(event.relative_humidity)) {
      LOG_INFO_LN(F("[DHT22] Error reading humidity!"));
    } else {
      currentHumidity = event.relative_humidity;
    }

    // -- wait at least 2 sec before reading again ------------
    // The interval of whole process must be beyond 2 seconds !!
    vTaskDelay(3000 / portTICK_RATE_MS);
  }
}

void initWifiAndServices() {
  // Load well known Wifi AP credentials from NVS
  WifiManager.startBackgroundTask();
  WifiManager.attachWebServer(&webServer);
  WifiManager.fallbackToSoftAp(preferences.getBool("enableSoftAp", true));

  WebSerial.begin(&webServer);
  
  APIRegisterRoutes();
  webServer.begin();
  LOG_INFO_LN(F("[WEB] HTTP server started"));

  if (enableWifi) {
    LOG_INFO_LN(F("[MDNS] Starting mDNS Service!"));
    MDNS.begin(hostName.c_str());
    MDNS.addService("http", "tcp", 80);
    MDNS.addService("ota", "udp", 3232);
    LOG_INFO_F("[MDNS] You should be able now to open http://%s.local/ in your browser.\n", hostName);
  }

  if (enableMqtt) {
    Mqtt.prepare(
      preferences.getString("mqttHost", "localhost"),
      preferences.getUInt("mqttPort", 1883),
      preferences.getString("mqttTopic", "verges/toilet"),
      preferences.getString("mqttUser", ""),
      preferences.getString("mqttPass", "")
    );
  }
  else LOG_INFO_LN(F("[MQTT] Publish to MQTT is disabled."));
}

// Tacho interrupt handler is only executed if the FAN is spinning 
// and the TACHO signal pin is connected
void IRAM_ATTR tacho_interrupt_handler() {
  unsigned long current_micros = micros();
  tachoDelay = (current_micros - lastTachoInterrupt);
  lastTachoInterrupt = current_micros;
}

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);

  LOG_INFO_LN(F("\n\n==== starting ESP32 setup() ===="));
  LOG_INFO_F("Firmware build date: %s %s\n", __DATE__, __TIME__);

  LOG_INFO_F("[GPIO] Configuration of GPIO %d as INPUT_PULLUP ... ", button1.PIN);
  pinMode(button1.PIN, INPUT_PULLUP);
  attachInterrupt(button1.PIN, ISR_button1, FALLING);
  LOG_INFO_LN(F("done"));

  // Initialize with the current runtime to avoid running the mixer on start!
  lastMixerRun = runtime();

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(DPLUS_PIN, INPUT_PULLDOWN);

  pinMode(MIXER_START_PIN, OUTPUT);
  pinMode(MIXER_STATUS_PIN, INPUT_PULLDOWN);

  pinMode(TACHO_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(TACHO_PIN), tacho_interrupt_handler, FALLING);

  // run PWM on 25% on startup
  analogWrite(PWM_PIN, targetPwmSpeed);
  ledcSetup(PWM_CHANNEL, 25000, 10);
  ledcAttachPin(PWM_PIN, PWM_CHANNEL);
  ledcWrite(PWM_CHANNEL, targetPwmSpeed);

  if (!LittleFS.begin(true)) {
    LOG_INFO_LN(F("[FS] An Error has occurred while mounting LittleFS"));
    // Reduce power consumption while having issues with NVS
    // This won't fix the problem, a check of the sensor log is required
    deepsleepForSeconds(5);
  }
  if (!preferences.begin(NVS_NAMESPACE)) preferences.clear();
  LOG_INFO_LN(F("[LITTLEFS] initialized"));

  // Load Settings from NVS
  hostName = preferences.getString("hostName");
  if (hostName.isEmpty()) {
    hostName = "ogotoilet";
    preferences.putString("hostName", hostName);
  }
  enableWifi = preferences.getBool("enableWifi", enableWifi);
  enableMqtt = preferences.getBool("enableMqtt", enableMqtt);

  runMixerAfter = preferences.getULong("runMixerAfter", runMixerAfter);
  noMixerBelowTempC = preferences.getInt("noMixerBelow", noMixerBelowTempC);

  overrideSpeedPoti = preferences.getBool("overridePoti", overrideSpeedPoti);
  overrideSpeed = preferences.getUInt("overrideSpeed", overrideSpeed);

  humidityThr = preferences.getUInt("humidityThr", humidityThr);
  humiditySpeed = preferences.getUInt("humiditySpeed", humiditySpeed);

  if (enableWifi) initWifiAndServices();
  else LOG_INFO_LN(F("[WIFI] Not starting WiFi!"));

  String otaPassword = preferences.getString("otaPassword");
  if (otaPassword.isEmpty()) {
    otaPassword = String((uint32_t)ESP.getEfuseMac());
    preferences.putString("otaPassword", otaPassword);
  }
  LOG_INFO_F("[OTA] Password set to '%s'\n", otaPassword);
  ArduinoOTA
    .setHostname(hostName.c_str())
    .setPassword(otaPassword.c_str())
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH) type = "sketch";
      else {
        type = "filesystem";
        LittleFS.end();
      }
      LOG_INFO_LN("Start updating " + type);
    })
    .onEnd([]() {
      LOG_INFO_LN("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      LOG_INFO_F("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      LOG_INFO_F("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) LOG_INFO_LN("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) LOG_INFO_LN("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) LOG_INFO_LN("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) LOG_INFO_LN("Receive Failed");
      else if (error == OTA_END_ERROR) LOG_INFO_LN("End Failed");
    });

  ArduinoOTA.begin();

  preferences.end();

  // Update the DHT Temperature and Humidity in a background task
  xTaskCreate(&DHT_task, "DHT_task", 2048, NULL, 5, NULL);
}

// Soft reset the ESP to start with setup() again, but without loosing RTC_DATA as it would be with ESP.reset()
void softReset() {
  if (enableWifi) {
    webServer.end();
    MDNS.end();
    Mqtt.disconnect();
    WifiManager.stopWifi();
  }
  esp_sleep_enable_timer_wakeup(1);
  esp_deep_sleep_start();
}

void loop() {
  ArduinoOTA.handle();

  if (button1.pressed) {
    LOG_INFO_LN(F("[EVENT] Button pressed!"));
    button1.pressed = false;
    if (enableWifi) {
      // bringt up a SoftAP instead of beeing a client
      WifiManager.runSoftAP();
    } else {
      initWifiAndServices();
    }
    // softReset();
  }

  if (runtime() - Timing.lastServiceCheck > Timing.serviceInterval) {
    Timing.lastServiceCheck = runtime();
    // Check if all the services work
    if (enableWifi && WiFi.status() == WL_CONNECTED && WiFi.getMode() & WIFI_MODE_STA) {
      if (enableMqtt && !Mqtt.isConnected()) Mqtt.connect();
    }
  }

  // Do not continue regular operation as long as a OTA is running
  // Reason: Background workload can cause upgrade issues that we want to avoid!
  if (otaRunning) return sleepOrDelay();

  if (runtime() - Timing.lastMixerUpdate > Timing.mixerUpdateInterval) {
    Timing.lastMixerUpdate = runtime();
    
    // When the Mixer of the toilet is active, we have a 12V Signal on the MIXER_STATUS_PIN using
    // a voltage devider ~12 to ~3V. We use that signal to reset the mixer timer so that
    // we can run it after X hours of the last run.
    stateMixer = digitalRead(MIXER_STATUS_PIN);
    if (stateMixer) {
      lastMixerRun = runtime();
      LOG_INFO_LN("MIXER - runs now!");
    } else if (runtime() - lastMixerRun > runMixerAfter) {
      // Some time has passed, we run the mixer using a transistor on MIXER_START_PIN to improve the rotting
      if (currentTemperature > noMixerBelowTempC) {
        activateMixer();
      } else {
        LOG_INFO(F("[INFO] Temerature below configured limit, not running the mixer. Next retry after configured timeout."));
      }
      lastMixerRun = runtime();
    }
  }

  if (runtime() - Timing.lastSpeedUpdate > Timing.speedUpdateInterval) {
    Timing.lastSpeedUpdate = runtime();
    
    // If the engine is running, we have a D+ signal on the DPLUS_PIN using a voltage devider ~12 to ~3V
    // When the signal is running, the fan should run on 100% speed to improve toilet drying
    // While the mixer is working, we again want to get full speed fan.
    stateDplus = digitalRead(DPLUS_PIN);
    if (stateDplus || stateMixer) {
      // LOG_INFO_LN("DPLUS active, max power!");
      digitalWrite(LED_BUILTIN, HIGH);
      targetPwmSpeed = PWM_MAX_DUTY_CYCLE;
    } else {
      digitalWrite(LED_BUILTIN, LOW);
      uint16_t potiRead = analogRead(SPEED_PIN);
      statePoti = map(potiRead, 0, MAX_ADC_VALUE, 0, 100);
      if (currentHumidity >= humidityThr && humiditySpeed > 0) {
        stateDehumidification = true;
        // Dehumidification required, overruling all other options
        if (humiditySpeed >= 100) targetPwmSpeed = PWM_MAX_DUTY_CYCLE;
        else targetPwmSpeed = map(humiditySpeed, 0, 100, 0, PWM_MAX_DUTY_CYCLE);
      } else {
        stateDehumidification = false;
        if (overrideSpeedPoti) {
          // Ignore potentiometer, use config value
          if (overrideSpeed >= 100) targetPwmSpeed = PWM_MAX_DUTY_CYCLE;
          else if(overrideSpeed <= 0) targetPwmSpeed = 0; // it's uint8, this should never happen ;)
          else targetPwmSpeed = map(overrideSpeed, 0, 100, 0, PWM_MAX_DUTY_CYCLE);
        } else {
          targetPwmSpeed = map(potiRead, 0, MAX_ADC_VALUE, 0, PWM_MAX_DUTY_CYCLE);
        }
      }
    }
    ledcWrite(PWM_CHANNEL, targetPwmSpeed);
  }

  if (runtime() - Timing.lastStatusUpdate > Timing.statusUpdateInterval) {
    Timing.lastStatusUpdate = runtime();

    String jsonOutput;
    StaticJsonDocument<1024> jsonDoc;

    uint8_t fanSpeed = map(targetPwmSpeed, 0, PWM_MAX_DUTY_CYCLE, 0, 100);
    LOG_INFO_F("FAN Target Speed: %d %%\n", fanSpeed);

    // Tacho Delay is not working, if the FAN doesn't provide the TACHO signal
    if (tachoDelay != 0) {
      unsigned long freq = 100000000 / tachoDelay;
      LOG_INFO_F("FAN Tacho delay:  %d µs\n", tachoDelay);
      LOG_INFO_F("FAN Frequency:    %d.%d Hz\n", freq/100, freq%100);

      freq *= 60;
      freq /= 200;
      LOG_INFO_F("FAN Current RPM:  %d \n", freq);

      jsonDoc["stateFanRpm"] = freq;
      if (enableMqtt && Mqtt.isReady()) Mqtt.client.publish((Mqtt.mqttTopic + "/fan-rpm").c_str(), String(freq).c_str(), true);
    } else {
      jsonDoc["stateFanRpm"] = 0;
    }

    jsonDoc["lastMixer"] = runtime() - lastMixerRun;
    jsonDoc["stateMixer"] = stateMixer;
    jsonDoc["stateDplus"] = stateDplus;
    jsonDoc["statePoti"] = statePoti;
    jsonDoc["statePwmSpeed"] = fanSpeed;
    jsonDoc["stateTemperature"] = currentTemperature;
    jsonDoc["stateHumidity"] = currentHumidity;
    jsonDoc["stateDehumidification"] = stateDehumidification;

    serializeJsonPretty(jsonDoc, jsonOutput);
    events.send(jsonOutput.c_str(), "status", millis());

    if (enableMqtt && Mqtt.isReady()) {
      Mqtt.client.publish((Mqtt.mqttTopic + "/json").c_str(), jsonOutput.c_str(), true);
      Mqtt.client.publish((Mqtt.mqttTopic + "/mixer").c_str(), String(stateMixer).c_str(), true);
      Mqtt.client.publish((Mqtt.mqttTopic + "/dplus").c_str(), String(stateDplus).c_str(), true);
      Mqtt.client.publish((Mqtt.mqttTopic + "/potentiometer").c_str(), String(statePoti).c_str(), true);
      Mqtt.client.publish((Mqtt.mqttTopic + "/pwm-speed").c_str(), String(fanSpeed).c_str(), true);
      Mqtt.client.publish((Mqtt.mqttTopic + "/temperature").c_str(), String(currentTemperature).c_str(), true);
      Mqtt.client.publish((Mqtt.mqttTopic + "/humidity").c_str(), String(currentHumidity).c_str(), true);
    }

    LOG_INFO("currentTemperature = "); LOG_INFO_LN(currentTemperature);
    LOG_INFO("currentHumidity = ");    LOG_INFO_LN(currentHumidity);
  }
  sleepOrDelay();
}
