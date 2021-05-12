/*
 * ESP8266 EnergyMeter
 * 
 * Based on ESP8266 D1 Mini PRO 
 * 
 * to be connected to serial port of PZEM-004T power meter module
 * 
 */
#define __DEBUG__

// Firmware data
const char BUILD[] = __DATE__ " " __TIME__;
#define FW_NAME         "energymeter"
#define FW_VERSION      "0.0.1"

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <ESP8266httpUpdate.h>
#include <EEPROM.h>

#include <Hash.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>

// File System
#include <FS.h>   

// Format SPIFFS if mount failed
#define FORMAT_SPIFFS_IF_FAILED 1
// ArduinoJson
// https://arduinojson.org/
#include <ArduinoJson.h>

// NTP ClientLib
// https://github.com/gmag11/NtpClient
#include <NtpClientLib.h>

// NTP
NTPSyncEvent_t ntpEvent;
bool syncEventTriggered = false; // True if a time even has been triggered

// PZEM-004T library
// https://github.com/olehs/PZEM004T/
#include <PZEM004T.h>

#define PZEM_TX 4 // Connects to the RX pin on the PZEM
#define PZEM_RX 5 // Connects to the TX pin on the PZEM

PZEM004T pzem(PZEM_RX,PZEM_TX);  // RX,TX pins to be connected to TX,RX of PZEM
IPAddress ip(192,168,1,1);

// MQTT PubSub client
// https://github.com/knolleary/pubsubclient
#include <PubSubClient.h>

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

// Config
struct Config {
  // WiFi config
  char wifi_essid[24];
  char wifi_password[16];
  // NTP Config
  char ntp_server[16];
  int8_t ntp_timezone;
  // Host config
  char hostname[16];
  bool ota_enable;
  char admin_username[16];
  char admin_password[16];
  // MQTT config
  char broker_host[24];
  unsigned int broker_port;
  char client_id[24];
  // Alarms config
  unsigned int power_alarm;
};

#define CONFIG_FILE "/config.json"
File configFile;
Config config; // Global config object

// Web server
AsyncWebServer server(80);

// Task Scheduler
#include <TaskScheduler.h>

// Scheduler and tasks...
Scheduler runner;

#define MQTT_INTERVAL 60000*5 // Every 5 minutes...
void mqttTaskCB();
Task mqttTask(MQTT_INTERVAL, TASK_FOREVER, &mqttTaskCB);

// Define BUZZER Pin
#define BUZZER 0 // D3

// ************************************
// DEBUG_PRINT() and DEBUG_PRINTLN()
//
// send message via RSyslog (if enabled) or fallback on Serial 
// ************************************
void DEBUG_PRINT(String message) {
#ifdef __DEBUG__
  Serial.print(message);
#endif
}

void DEBUG_PRINTLN(String message) {
#ifdef __DEBUG__
  Serial.println(message);
#endif
}

unsigned int last=0;
DynamicJsonDocument env(128);

/*
 * SETUP()
 */
void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();
  Serial.println();
  Serial.println();
  Serial.print("ESP8266 SDK: ");
  Serial.println(ESP.getSdkVersion());
  Serial.println();
  Serial.print(FW_NAME);
  Serial.print(" ");
  Serial.print(FW_VERSION);
  Serial.print(" ");
  Serial.println(BUILD);
  delay(1000);

  pinMode(BUZZER, OUTPUT);
  analogWrite(BUZZER, 205);
  delay(500);
  analogWrite(BUZZER, 0);

  // Initialize SPIFFS
  DEBUG_PRINT("[INIT] Initializing SPIFFS...");
  if(!SPIFFS.begin()){
    DEBUG_PRINTLN("[ERROR] SPIFFS mount failed. Try formatting...");
    if(SPIFFS.format()) {
      DEBUG_PRINTLN("[INIT] SPIFFS initialized successfully");
    } else {
      DEBUG_PRINTLN("[FATAL] SPIFFS fatal error");
      ESP.restart();
    }
  } else {
    DEBUG_PRINTLN("OK");
  }

  // Load configuration
  loadConfigFile();

  // Setup OTA
  ArduinoOTA.onStart([]() {
    Serial.println("[OTA] Update Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("[OTA] Update End");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    char p[32];
    sprintf(p, "[OTA] Progress: %u%%\n", (progress/(total/100)));
    DEBUG_PRINTLN(p);
  });
  ArduinoOTA.onError([](ota_error_t error) {
    if(error == OTA_AUTH_ERROR) DEBUG_PRINTLN("[OTA] Auth Failed");
    else if(error == OTA_BEGIN_ERROR) DEBUG_PRINTLN("[OTA] Begin Failed");
    else if(error == OTA_CONNECT_ERROR) DEBUG_PRINTLN("[OTA] Connect Failed");
    else if(error == OTA_RECEIVE_ERROR) DEBUG_PRINTLN("[OTA] Recieve Failed");
    else if(error == OTA_END_ERROR) DEBUG_PRINTLN("[OTA] End Failed");
  });
  ArduinoOTA.setHostname(config.hostname);
  ArduinoOTA.begin();

  // Initialize PZEM 
  pzem.setAddress(ip);

  // Initialize web server on port 80
  initWebServer();

  // Add environmental sensor data fetch task
  runner.addTask(mqttTask);
  mqttTask.enable();

  env["status"] = "Up and running";
  // GO!
}

void loop() {
  if(config.ota_enable) {
    // Handle OTA
    ArduinoOTA.handle();
  }

  // Scheduler
  runner.execute();
  
  // NTP ?
  if(syncEventTriggered) {
    processSyncEvent(ntpEvent);
    syncEventTriggered = false;
  }

  if((millis() - last) > 1100) {
    if(WiFi.status() != WL_CONNECTED) {
      connectToWifi();
    }

    float v = pzem.voltage(ip);
    if(!isnan(v)) {
      Serial.print("Voltage: "); Serial.print(v); Serial.println("V");
      env["v"] = v;
    }

    float i = pzem.current(ip);
    if(!isnan(i)) {
      Serial.print("Current: "); Serial.print(i); Serial.println("A");
      env["i"] = i;
    }
  
    float p = pzem.power(ip);
    if(!isnan(p)) {
      Serial.print("Power: "); Serial.print(p); Serial.println("W");
      env["p"] = p;
    }
  
    float e = pzem.energy(ip);
    if(!isnan(e)) {
      Serial.print("Energy: "); Serial.print(e,3); Serial.println("kWh");
      env["e"] = e;
    }
    
    env["uptime"] = millis() / 1000;
    last = millis();
  }
}
