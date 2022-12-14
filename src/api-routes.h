/**
 * @file api-routes.h
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
#include <AsyncJson.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <FS.h>
#include <LittleFS.h>
#include <Update.h>
#include <esp_ota_ops.h>

extern bool otaRunning;
extern bool enableWifi;
extern bool enableMqtt;

extern const int MIXER_START_PIN;

#ifdef __cplusplus
extern "C" {
#endif
uint8_t temprature_sens_read();
#ifdef __cplusplus
}
#endif
uint8_t temprature_sens_read();

void APIRegisterRoutes() {
  webServer.on("/api/firmware/info", HTTP_GET, [&](AsyncWebServerRequest *request) {
    auto data = esp_ota_get_running_partition();
    String output;
    DynamicJsonDocument doc(256);
    doc["partition_type"] = data->type;
    doc["partition_subtype"] = data->subtype;
    doc["address"] = data->address;
    doc["size"] = data->size;
    doc["label"] = data->label;
    doc["encrypted"] = data->encrypted;
    doc["firmware_version"] = AUTO_FW_VERSION;
    doc["firmware_date"] = AUTO_FW_DATE;
    serializeJson(doc, output);
    request->send(200, "application/json", output);
  });

  webServer.on("/api/update/upload", HTTP_POST,
    [&](AsyncWebServerRequest *request) { },
    [&](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {

    String otaPassword = "";
    if (preferences.begin(NVS_NAMESPACE, true)) {
      otaPassword = preferences.getString("otaPassword");
      preferences.end();

      if (otaPassword.length()) {
        if(!request->authenticate("ota", otaPassword.c_str())) {
          return request->send(401, "application/json", "{\"message\":\"Invalid OTA password provided!\"}");
        }
      } else LOG_INFO_LN(F("[OTA] No password configured, no authentication requested!"));
    } else LOG_INFO_LN(F("[OTA] Unable to load password from NVS."));

    if (!index) {
      otaRunning = true;
      LOG_INFO(F("[OTA] Begin firmware update with filename: "));
      LOG_INFO_LN(filename);
      // if filename includes spiffs|littlefs, update the spiffs|littlefs partition
      int cmd = (filename.indexOf("spiffs") > -1 || filename.indexOf("littlefs") > -1) ? U_SPIFFS : U_FLASH;
      if (!Update.begin(UPDATE_SIZE_UNKNOWN, cmd)) {
        LOG_INFO(F("[OTA] Error: "));
        Update.printError(Serial);
        request->send(500, "application/json", "{\"message\":\"Unable to begin firmware update!\"}");
        otaRunning = false;
      }
    }

    if (Update.write(data, len) != len) {
      LOG_INFO(F("[OTA] Error: "));
      Update.printError(Serial);
      request->send(500, "application/json", "{\"message\":\"Unable to write firmware update data!\"}");
      otaRunning = false;
    }

    if (final) {
      if (!Update.end(true)) {
        String output;
        DynamicJsonDocument doc(32);
        doc["message"] = "Update error";
        doc["error"] = Update.errorString();
        serializeJson(doc, output);
        request->send(500, "application/json", output);

        LOG_INFO_LN("[OTA] Error when calling calling Update.end().");
        Update.printError(Serial);
        otaRunning = false;
      } else {
        LOG_INFO_LN("[OTA] Firmware update successful.");
        request->send(200, "application/json", "{\"message\":\"Please wait while the device reboots!\"}");
        yield();
        delay(250);

        LOG_INFO_LN("[OTA] Update complete, rebooting now!");
        Serial.flush();
        ESP.restart();
      }
    }
  });

  events.onConnect([&](AsyncEventSourceClient *client){
    if(client->lastId()){
      LOG_INFO_F("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
    }
    client->send("connected", NULL, millis(), 1000);
  });
  webServer.addHandler(&events);

  webServer.on("/api/mixer/start", HTTP_POST, [&](AsyncWebServerRequest *request) {
    AsyncResponseStream *response = request->beginResponseStream("application/json");

    activateMixer();
    
    request->send(200, "application/json", "{\"message\":\"Running the mixer!\"}");
    request->send(response);
  });

  webServer.on("/api/reset", HTTP_POST, [&](AsyncWebServerRequest *request) {
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    request->send(200, "application/json", "{\"message\":\"Resetting the sensor!\"}");
    request->send(response);
    yield();
    delay(250);
    ESP.restart();
  });

  webServer.on("/api/config", HTTP_POST, [&](AsyncWebServerRequest * request){}, NULL,
    [&](AsyncWebServerRequest * request, uint8_t *data, size_t len, size_t index, size_t total) {

    DynamicJsonDocument jsonBuffer(1024);
    deserializeJson(jsonBuffer, (const char*)data);

    if (preferences.begin(NVS_NAMESPACE)) {
      String hostname = jsonBuffer["hostname"].as<String>();
      if (!hostname || hostname.length() < 3 || hostname.length() > 32) {
        // TODO: Add better checks according to RFC hostnames
        request->send(422, "application/json", "{\"message\":\"Invalid hostname!\"}");
        return;
      } else {
        preferences.putString("hostName", hostname);
      }

      if (preferences.putBool("enableWifi", jsonBuffer["enablewifi"].as<boolean>())) {
        enableWifi = jsonBuffer["enablewifi"].as<boolean>();
      }
      if (preferences.putBool("enableSoftAp", jsonBuffer["enablesoftap"].as<boolean>())) {
        WifiManager.fallbackToSoftAp(jsonBuffer["enablesoftap"].as<boolean>());
      }

      preferences.putString("otaPassword", jsonBuffer["otapassword"].as<String>());
      ArduinoOTA.setPassword(jsonBuffer["otapassword"].as<String>().c_str());
      
      preferences.putULong("runMixerAfter", jsonBuffer["runMixerAfterMinutes"].as<unsigned long>()*60*1000);
      runMixerAfter = jsonBuffer["runMixerAfterMinutes"].as<unsigned long>()*60*1000;

      preferences.putInt("noMixerBelow", jsonBuffer["noMixerBelowTempC"].as<int8_t>());
      noMixerBelowTempC = jsonBuffer["noMixerBelowTempC"].as<int8_t>();

      preferences.putUInt("overrideSpeed", jsonBuffer["overrideSpeed"].as<uint8_t>());
      overrideSpeed = jsonBuffer["overrideSpeed"].as<uint8_t>();

      if (preferences.putBool("overridePoti", jsonBuffer["overrideSpeedPoti"].as<boolean>())) {
        overrideSpeedPoti = jsonBuffer["overrideSpeedPoti"].as<boolean>();
      }

      preferences.putUInt("humidityThr", jsonBuffer["humidityThr"].as<uint8_t>());
      humidityThr = jsonBuffer["humidityThr"].as<uint8_t>();

      preferences.putUInt("humiditySpeed", jsonBuffer["humiditySpeed"].as<uint8_t>());
      humiditySpeed = jsonBuffer["humiditySpeed"].as<uint8_t>();

      // MQTT Settings
      preferences.putUInt("mqttPort", jsonBuffer["mqttport"].as<uint16_t>());
      preferences.putString("mqttHost", jsonBuffer["mqtthost"].as<String>());
      preferences.putString("mqttTopic", jsonBuffer["mqtttopic"].as<String>());
      preferences.putString("mqttUser", jsonBuffer["mqttuser"].as<String>());
      preferences.putString("mqttPass", jsonBuffer["mqttpass"].as<String>());
      if (preferences.putBool("enableMqtt", jsonBuffer["enablemqtt"].as<boolean>())) {
        if (enableMqtt) Mqtt.disconnect();
        enableMqtt = jsonBuffer["enablemqtt"].as<boolean>();
        if (enableMqtt) {
          Mqtt.prepare(
            jsonBuffer["mqtthost"].as<String>(),
            jsonBuffer["mqttport"].as<uint16_t>(),
            jsonBuffer["mqtttopic"].as<String>(),
            jsonBuffer["mqttuser"].as<String>(),
            jsonBuffer["mqttpass"].as<String>()
          );
          Mqtt.connect();
        }
      }
    }
    preferences.end();
    
    request->send(200, "application/json", "{\"message\":\"New hostname stored in NVS, reboot required!\"}");
  });

  webServer.on("/api/config", HTTP_GET, [&](AsyncWebServerRequest *request) {
    if (request->contentType() == "application/json") {
      String output;
      DynamicJsonDocument doc(1024);

      if (preferences.begin(NVS_NAMESPACE, true)) {
        doc["hostname"] = hostName;
        doc["enablewifi"] = enableWifi;
        doc["enablesoftap"] = WifiManager.getFallbackState();

        doc["otapassword"] = preferences.getString("otaPassword");

        doc["runMixerAfterMinutes"] = preferences.getULong("runMixerAfter", runMixerAfter) / 60 / 1000 ;
        doc["noMixerBelowTempC"] = preferences.getInt("noMixerBelow", noMixerBelowTempC);

        doc["overrideSpeedPoti"] = preferences.getBool("overrideSpeedPoti", overrideSpeedPoti);
        doc["overrideSpeed"] = preferences.getUInt("overrideSpeed", overrideSpeed);
        
        doc["humidityThr"] = preferences.getUInt("humidityThr", humidityThr);
        doc["humiditySpeed"] = preferences.getUInt("humiditySpeed", humiditySpeed);

        // MQTT
        doc["enablemqtt"] = enableMqtt;
        doc["mqttport"] = preferences.getUInt("mqttPort", 1883);
        doc["mqtthost"] = preferences.getString("mqttHost", "");
        doc["mqtttopic"] = preferences.getString("mqttTopic", "");
        doc["mqttuser"] = preferences.getString("mqttUser", "");
        doc["mqttpass"] = preferences.getString("mqttPass", "");
      }
      preferences.end();

      serializeJson(doc, output);
      request->send(200, "application/json", output);
    } else request->send(415, "text/plain", "Unsupported Media Type");
  });

  webServer.on("/api/partition/switch", HTTP_POST, [&](AsyncWebServerRequest * request){}, NULL,
    [&](AsyncWebServerRequest * request, uint8_t *data, size_t len, size_t index, size_t total) {
    auto next = esp_ota_get_next_update_partition(NULL);
    auto error = esp_ota_set_boot_partition(next);
    if (error == ESP_OK) {
      request->send(200, "application/json", "{\"message\":\"New partition ready for boot\"}");
    } else {
      request->send(500, "application/json", "{\"message\":\"Error switching boot partition\"}");
    }
  });

  webServer.on("/api/esp", HTTP_GET, [&](AsyncWebServerRequest * request) {
    String output;
    DynamicJsonDocument json(2048);

    JsonObject booting = json.createNestedObject("booting");
    booting["rebootReason"] = esp_reset_reason();
    booting["partitionCount"] = esp_ota_get_app_partition_count();

    auto partition = esp_ota_get_boot_partition();
    JsonObject bootPartition = json.createNestedObject("bootPartition");
    bootPartition["address"] = partition->address;
    bootPartition["size"] = partition->size;
    bootPartition["label"] = partition->label;
    bootPartition["encrypted"] = partition->encrypted;
    switch (partition->type) {
      case ESP_PARTITION_TYPE_APP:  bootPartition["type"] = "app"; break;
      case ESP_PARTITION_TYPE_DATA: bootPartition["type"] = "data"; break;
      default: bootPartition["type"] = "any";
    }
    bootPartition["subtype"] = partition->subtype;

    partition = esp_ota_get_running_partition();
    JsonObject runningPartition = json.createNestedObject("runningPartition");
    runningPartition["address"] = partition->address;
    runningPartition["size"] = partition->size;
    runningPartition["label"] = partition->label;
    runningPartition["encrypted"] = partition->encrypted;
    switch (partition->type) {
      case ESP_PARTITION_TYPE_APP:  runningPartition["type"] = "app"; break;
      case ESP_PARTITION_TYPE_DATA: runningPartition["type"] = "data"; break;
      default: runningPartition["type"] = "any";
    }
    runningPartition["subtype"] = partition->subtype;

    JsonObject build = json.createNestedObject("build");
    build["date"] = __DATE__;
    build["time"] = __TIME__;

    JsonObject ram = json.createNestedObject("ram");
    ram["heapSize"] = ESP.getHeapSize();
    ram["freeHeap"] = ESP.getFreeHeap();
    ram["usagePercent"] = (float)ESP.getFreeHeap() / (float)ESP.getHeapSize() * 100.f;
    ram["minFreeHeap"] = ESP.getMinFreeHeap();
    ram["maxAllocHeap"] = ESP.getMaxAllocHeap();

    JsonObject spi = json.createNestedObject("spi");
    spi["psramSize"] = ESP.getPsramSize();
    spi["freePsram"] = ESP.getFreePsram();
    spi["minFreePsram"] = ESP.getMinFreePsram();
    spi["maxAllocPsram"] = ESP.getMaxAllocPsram();

    JsonObject chip = json.createNestedObject("chip");
    chip["revision"] = ESP.getChipRevision();
    chip["model"] = ESP.getChipModel();
    chip["cores"] = ESP.getChipCores();
    chip["cpuFreqMHz"] = ESP.getCpuFreqMHz();
    chip["cycleCount"] = ESP.getCycleCount();
    chip["sdkVersion"] = ESP.getSdkVersion();
    chip["efuseMac"] = ESP.getEfuseMac();
    chip["temperature"] = (temprature_sens_read() - 32) / 1.8;

    JsonObject flash = json.createNestedObject("flash");
    flash["flashChipSize"] = ESP.getFlashChipSize();
    flash["flashChipRealSize"] = spi_flash_get_chip_size();
    flash["flashChipSpeedMHz"] = ESP.getFlashChipSpeed() / 1000000;
    flash["flashChipMode"] = ESP.getFlashChipMode();
    flash["sdkVersion"] = ESP.getFlashChipSize();

    JsonObject sketch = json.createNestedObject("sketch");
    sketch["size"] = ESP.getSketchSize();
    sketch["maxSize"] = ESP.getFreeSketchSpace();
    sketch["usagePercent"] = (float)ESP.getSketchSize() / (float)ESP.getFreeSketchSpace() * 100.f;
    sketch["md5"] = ESP.getSketchMD5();

    JsonObject fs = json.createNestedObject("filesystem");
    fs["type"] = F("LittleFS");
    fs["totalBytes"] = LittleFS.totalBytes();
    fs["usedBytes"] = LittleFS.usedBytes();
    fs["usagePercent"] = (float)LittleFS.usedBytes() / (float)LittleFS.totalBytes() * 100.f;
    
    serializeJson(json, output);
    request->send(200, "application/json", output);
  });

  File tmp = LittleFS.open("/index.html");
  time_t cr = tmp.getLastWrite();
  tmp.close();
  struct tm * timeinfo = gmtime(&cr);

  webServer.serveStatic("/", LittleFS, "/")
    .setCacheControl("max-age=86400")
    .setLastModified(timeinfo)
    .setDefaultFile("index.html");


  webServer.onNotFound([&](AsyncWebServerRequest *request) {
    if (request->method() == HTTP_OPTIONS) {
      request->send(200);
    } else {
      AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/index.html");
      response->setCode(200);
      request->send(response);
/*    if (request->contentType() == "application/json") {
        request->send(404, "application/json", "{\"message\":\"Not found\"}");
      } else request->send(404, "text/plain", "Not found");*/
    }
  });
}
