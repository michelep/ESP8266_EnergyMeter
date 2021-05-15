/*
 * ESP8266 EnergyMeter
 * 
 * Based on ESP8266 D1 Mini PRO 
 * 
 * to be connected to serial port of PZEM-004T power meter module
 * 
 * 12.05.2021 - v0.0.1
 * - First release
 * 
 * 14.05.2021 - v0.0.2
 * - Added support for v3.0
 * - Added energy reset (manual or at midnight, every day)
 * - Added time alarm
 * - minor changes
 * 
 * 15.05.2021 - v0.0.3
 * - Fix energy reset issue
 * - Added energy reset button in web gui
 * 
 */
 
#define __DEBUG__

// If PZEM004Tv3.0 board, define this.
#undef V3

// Firmware data
const char BUILD[] = __DATE__ " " __TIME__;
#define FW_NAME         "energymeter"
#define FW_VERSION      "0.0.3"

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

// TimeAlarms
// https://github.com/PaulStoffregen/TimeAlarms
#include <TimeAlarms.h>

// NTP
// https://github.com/gmag11/NtpClient
#include <NtpClientLib.h>

NTPSyncEvent_t ntpEvent;
bool syncEventTriggered = false; // True if a time even has been triggered

// PZEM004T module
#define PZEM_TX 4 // Connects to the RX pin on the PZEM
#define PZEM_RX 5 // Connects to the TX pin on the PZEM

// PZEM-004T library v3
#ifdef V3
#warning "Support for PZEM004T V3 board is untested!"
// https://github.com/mandulaj/PZEM-004T-v30
// #TODO
#include <PZEM004Tv30.h>

PZEM004Tv30 pzem(PZEM_RX,PZEM_TX);  // RX,TX pins to be connected to TX,RX of PZEM

#else
#warning "PZEM004T V1 or V2 board"
// PZEM-004T library for v1 and v2
// https://github.com/olehs/PZEM004T/
#include <PZEM004T.h>

PZEM004T pzem(PZEM_RX,PZEM_TX);  // RX,TX pins to be connected to TX,RX of PZEM
IPAddress ip(192,168,1,1);
#endif

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
  unsigned int broker_tout;
  // Alarms config
  unsigned int power_alarm;
  bool energy_reset;
  time_t reset_date;
  unsigned long energy_idx; // This is needed for "soft" power reset on v1 and v2 boards: keeps last energy value before reset. 
};

#define CONFIG_FILE "/config.json"
File configFile;
Config config; // Global config object

// Web server
AsyncWebServer server(80);

// Task Scheduler
// https://github.com/arkhipenko/TaskScheduler
#include <TaskScheduler.h>

// Scheduler and tasks...
Scheduler runner;

#define MQTT_INTERVAL 60000*5 // Every 5 minutes...
void mqttTaskCB();
Task mqttTask(MQTT_INTERVAL, TASK_FOREVER, &mqttTaskCB);

// Define BUZZER Pin
#define BUZZER 0 // D3

bool is_restart=false; // If true, restart ESP

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

void buzzerBeep(uint8_t t) {
  analogWrite(BUZZER, 205);
  Alarm.delay(t);
  analogWrite(BUZZER, 0);
}

/*
 * energyReset - Reset energy counter
 */
void energyReset() {
#ifdef V3
  pzem.resetEnergy();
#else
  // soft reset
  unsigned long e;
  e = env["energy"];
  config.energy_idx += e;
#endif
  config.reset_date = now();
  // then, save to SPIFFS...
  saveConfigFile();
}

void resetAlarm() {
  if(config.energy_reset) {
    energyReset();
  }
}
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

  buzzerBeep(500);
  
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

  // Connect to WiFi
  connectToWifi();
  
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
  mqttTask.setInterval(1000*config.broker_tout);
  runner.addTask(mqttTask);
  mqttTask.enable();

  // Add alarm
  Alarm.alarmRepeat(0,0,0, resetAlarm);  // At midnight, every day, reset energy counter (if enabled)

  env["status"] = "Up and running";

  buzzerBeep(100);
  Alarm.delay(100);
  buzzerBeep(100);
  // GO!
}

void loop() {
  if(config.ota_enable) {
    // Handle OTA
    ArduinoOTA.handle();
  }

  // Scheduler
  runner.execute();

  // MQTT Client
  mqttClient.loop();

  // NTP
  if(syncEventTriggered) {
    processSyncEvent(ntpEvent);
    syncEventTriggered = false;
  }

  if(is_restart) {
    ESP.restart();
  }

  if((millis() - last) > 1100) {
    if(WiFi.status() != WL_CONNECTED) {
      connectToWifi();
    }

    float v = pzem.voltage(ip);
    if(!isnan(v)) {
      Serial.print("Voltage: "); Serial.print(v); Serial.println("V");
      env["voltage"] = v;
    }

    float i = pzem.current(ip);
    if(!isnan(i)) {
      Serial.print("Current: "); Serial.print(i); Serial.println("A");
      env["current"] = i;
    }
  
    float p = pzem.power(ip);
    if(!isnan(p)) {
      Serial.print("Power: "); Serial.print(p); Serial.println("W");
      env["power"] = p;
      // Check for power alarm threshold
      if((config.power_alarm > 0)&&(p > config.power_alarm)) {
        env["is_alarm"] = true;
      } else {
        env["is_alarm"] = false;
      }
    }
  
    float e = pzem.energy(ip);
    if(!isnan(e)) {
      Serial.print("Energy: "); Serial.print(e,3); Serial.println("kWh");
#ifdef V3
      env["energy"] = e;
#else
      env["energy"] = e - config.energy_idx;
#endif
    }

    // If is_alarm is true, then BEEP!
    if(env["is_alarm"]) {
      buzzerBeep(200);  
    }
    
    // Go ahead!
    env["uptime"] = millis() / 1000;
    env["ts"] = now();
    
    last = millis();
  }
  Alarm.delay(10);
}
