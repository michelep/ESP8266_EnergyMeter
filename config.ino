// Written by Michele<o-zone@zerozone.it> Pinassi
// Released under GPLv3 - No any warranty

// ************************************
// Config, save and load functions
//
// save and load configuration from config file in SPIFFS. JSON format (need ArduinoJson library)
// ************************************
bool loadConfigFile() {
  DynamicJsonDocument root(512);
  
  DEBUG_PRINT("[CONFIG] Loading config...");
  
  configFile = SPIFFS.open(CONFIG_FILE, "r");
  if (!configFile) {
    DEBUG_PRINTLN("ERROR: Config file not available");
    return false;
  } else {
    // Get the root object in the document
    DeserializationError err = deserializeJson(root, configFile);
    if (err) {
      DEBUG_PRINTLN("ERROR: "+String(err.c_str()));
      return false;
    } else {
      strlcpy(config.wifi_essid, root["wifi_essid"], sizeof(config.wifi_essid));
      strlcpy(config.wifi_password, root["wifi_password"], sizeof(config.wifi_password));
      strlcpy(config.hostname, root["hostname"] | "energy-meter", sizeof(config.hostname));
      // NTP
      strlcpy(config.ntp_server, root["ntp_server"] | "time.ien.it", sizeof(config.ntp_server));
      config.ntp_timezone = root["ntp_timezone"] | 1;
      // MQTT broker
      strlcpy(config.broker_host, root["broker_host"] | "", sizeof(config.broker_host));
      config.broker_port = root["broker_port"] | 1883;
      strlcpy(config.client_id, root["client_id"] | "energy-meter", sizeof(config.client_id));
      config.broker_tout = root["broker_tout"] | 300; // Default, push MQTT every 300 seconds
      // Other
      config.ota_enable = root["ota_enable"] | true;
      config.power_alarm = root["power_alarm"] | 0;
      // 
      DEBUG_PRINTLN("OK");
    }
  }
  configFile.close();
  return true;
}

bool saveConfigFile() {
  DynamicJsonDocument root(512);
  DEBUG_PRINT("[CONFIG] Saving config...");

  root["wifi_essid"] = config.wifi_essid;
  root["wifi_password"] = config.wifi_password;
  root["hostname"] = config.hostname;
  root["ntp_server"] = config.ntp_server;
  root["ntp_timezone"] = config.ntp_timezone;
  root["broker_host"] = config.broker_host;
  root["broker_port"] = config.broker_port;
  root["client_id"] = config.client_id;
  root["broker_tout"] = config.broker_tout;
  root["ota_enable"] = config.ota_enable;
  root["power_alarm"] = config.power_alarm;
  
  configFile = SPIFFS.open(CONFIG_FILE, "w");
  if(!configFile) {
    DEBUG_PRINTLN("ERROR: Failed to create config file !");
    return false;
  }
  serializeJson(root,configFile);
  configFile.close();
  DEBUG_PRINTLN("OK");
  return true;
}
