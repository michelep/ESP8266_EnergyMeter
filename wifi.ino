/* 
 * ESP8266 Energy Meter
 *  
 * Written by Michele <o-zone@zerozone.it> Pinassi 
 * Released under GPLv3 - No any warranty 
 * 
 * WiFi procedures
 * 
 */

// ************************************
// scanWifi()
//
// scan available WiFi networks
// ************************************
void scanWifi() {
  WiFi.mode(WIFI_STA);
  Serial.println("--------------");
  int n = WiFi.scanNetworks();
  Serial.println("scan done");
  if (n == 0) {
    Serial.println("no networks found");
  } else {
    Serial.print(n);
    Serial.println(" networks found");
    for (int i = 0; i < n; ++i) {
      // Print SSID and RSSI for each network found
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(i));
      Serial.print(" (");
      Serial.print(WiFi.RSSI(i));
      Serial.print(")");
      Serial.println((WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*");
      delay(10);
    }
  }
  Serial.println("--------------");
}

// ************************************
// connectToWifi()
//
// connect to configured WiFi network
// ************************************

const PROGMEM char* ntpServer = "pool.ntp.org";

bool connectToWifi() {
  uint8_t timeout=0;
  if(strlen(config.wifi_essid) > 0) {
    DEBUG_PRINT("[INIT] Connecting to "+String(config.wifi_essid));
    
    WiFi.mode(WIFI_STA);
    WiFi.hostname(config.hostname);
    WiFi.begin(config.wifi_essid, config.wifi_password);

    while((WiFi.status() != WL_CONNECTED) && (timeout < 10)) {
      delay(250);
      DEBUG_PRINT(".");
      delay(250);
      timeout++;
    }
    
    if(WiFi.status() == WL_CONNECTED) {
      DEBUG_PRINT("OK. IP:"+WiFi.localIP().toString());
      DEBUG_PRINTLN("Signal strength (RSSI):"+String(WiFi.RSSI())+" dBm");

      if (MDNS.begin(config.hostname)) {
        DEBUG_PRINTLN("[INIT] MDNS responder started");
        // Add service to MDNS-SD
        MDNS.addService("http", "tcp", 80);
      }

      NTP.onNTPSyncEvent([](NTPSyncEvent_t event) {
        ntpEvent = event;
        syncEventTriggered = true;
      });

      // NTP
      DEBUG_PRINTLN("[INIT] NTP sync time on "+String(config.ntp_server));
      NTP.begin(config.ntp_server, config.ntp_timezone, true, 0);
      NTP.setInterval(600);
    
      return true;  
    } else {
      DEBUG_PRINTLN("[ERROR] Failed to connect to WiFi");
      WiFi.printDiag(Serial);
      return false;
    }
  } else {
      DEBUG_PRINTLN("[ERROR] WiFi configuration missing!");
      return false;
  }
}
