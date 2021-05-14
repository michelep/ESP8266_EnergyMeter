/* 
 * ESP8266 Energy Meter
 *  
 * Written by Michele <o-zone@zerozone.it> Pinassi 
 * Released under GPLv3 - No any warranty 
 * 
 * Web server procedures
 * 
 */
String templateProcessor(const String& var)
{
  //
  // System values
  //
  if(var == "hostname") {
    return String(config.hostname);
  }
  if(var == "fw_name") {
    return String(FW_NAME);
  }
  if(var=="fw_version") {
    return String(FW_VERSION);
  }
  if(var=="uptime") {
    return String(millis()/1000);
  }
  if(var=="timedate") {
    return NTP.getTimeDateString();
  }  
  //
  // Config values
  //
  if(var=="wifi_essid") {
    return String(config.wifi_essid);
  }
  if(var=="wifi_password") {
    return String(config.wifi_password);
  }
  if(var=="wifi_rssi") {
    return String(WiFi.RSSI());
  }
  // NTP
  if(var=="ntp_server") {
    return String(config.ntp_server);
  }
  if(var=="ntp_timezone") {
    return String(config.ntp_timezone);
  }
  // OTA
  if(var=="ota_enable") {
    if(config.ota_enable) {
      return String("checked");  
    } else {
      return String("");
    }
  }
  // MQTT
  if(var=="broker_host") {
    return String(config.broker_host);
  }
  if(var=="broker_port") {
    return String(config.broker_port);
  }
  if(var=="client_id") {
    return String(config.client_id);
  }
  if(var=="broker_tout") {
    return String(config.broker_tout);
  }
  // Auth
  if(var=="admin_username") {
    return String(config.admin_username);
  }
  if(var=="admin_password") {
    return String(config.admin_password);
  }   
  // Others
  if(var=="power_alarm") {
    return String(config.power_alarm);
  }   
  if(var=="energy_reset") {
    if(config.energy_reset) {
      return String("checked");  
    } else {
      return String("");
    }
  }
  if(var=="reset_date") {
    // Return last reset date time
    return String(ctime(&config.reset_date));
  }
  //
  return String();
}

// ************************************
// initWebServer
//
// initialize web server
// ************************************
void initWebServer() {
  server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html").setTemplateProcessor(templateProcessor).setAuthentication(config.admin_username, config.admin_password);

  server.on("/restart", HTTP_GET, [](AsyncWebServerRequest *request) {
    is_restart=true;
    request->redirect("/?result=ok");
  });
    
  server.on("/post", HTTP_POST, [](AsyncWebServerRequest *request) {
    // WIFI
    if(request->hasParam("wifi_essid", true)) {
        strcpy(config.wifi_essid,request->getParam("wifi_essid", true)->value().c_str());
    }
    if(request->hasParam("wifi_password", true)) {
        strcpy(config.wifi_password,request->getParam("wifi_password", true)->value().c_str());
    }
    // NTP
    if(request->hasParam("ntp_server", true)) {
        strcpy(config.ntp_server, request->getParam("ntp_server", true)->value().c_str());
    }
    if(request->hasParam("ntp_timezone", true)) {
        config.ntp_timezone = atoi(request->getParam("ntp_timezone", true)->value().c_str());
    }
    // OTA
    if(request->hasParam("ota_enable", true)) {
      config.ota_enable=true;        
    } else {
      config.ota_enable=false;
    } 
    // MQTT
    if(request->hasParam("broker_host", true)) {
        strcpy(config.broker_host,request->getParam("broker_host", true)->value().c_str());
    }
    if(request->hasParam("broker_port", true)) {
        config.broker_port = atoi(request->getParam("broker_port", true)->value().c_str());
    } 
    if(request->hasParam("client_id", true)) {
        strcpy(config.client_id,request->getParam("client_id", true)->value().c_str());
    }
    if(request->hasParam("broker_tout", true)) {
        config.broker_tout = atoi(request->getParam("broker_tout", true)->value().c_str());
    } 
    // ADMIN
    if(request->hasParam("admin_username", true)) {
        strcpy(config.admin_username,request->getParam("admin_username", true)->value().c_str());
    }
    if(request->hasParam("admin_password", true)) {
        strcpy(config.admin_password,request->getParam("admin_password", true)->value().c_str());
    }
    // Others
    if(request->hasParam("power_alarm", true)) {
        config.power_alarm = atoi(request->getParam("power_alarm", true)->value().c_str());
    }     
    if(request->hasParam("energy_reset", true)) {
      config.energy_reset=true;        
    } else {
      config.energy_reset=false;
    } 
    // 
    if(saveConfigFile()) {
      request->redirect("/?result=ok");
    } else {
      request->redirect("/?result=error");      
    }
  });

  server.on("/reset",HTTP_GET, [](AsyncWebServerRequest *request) {
    energyReset();
    request->redirect("/?result=ok");
  });
  
  server.on("/ajax", HTTP_POST, [] (AsyncWebServerRequest *request) {
    String action,response="";
    char outputJson[256];

    if (request->hasParam("action", true)) {
      action = request->getParam("action", true)->value();
      if(action.equals("env")) {
        serializeJson(env,outputJson);
        response = String(outputJson);
      }
    }
    request->send(200, "text/plain", response);
  });

  server.onNotFound([](AsyncWebServerRequest *request) {
    Serial.printf("404 NOT FOUND - http://%s%s\n", request->host().c_str(), request->url().c_str());
    request->send(404);
  });

  server.begin();
}
