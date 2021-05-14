/* 
 * ESP8266 Energy Meter
 *  
 * Written by Michele <o-zone@zerozone.it> Pinassi 
 * Released under GPLv3 - No any warranty 
 * 
 * MQTT procedures
 * 
 */

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  DEBUG_PRINTLN("Message arrived: "+String(topic));
}

// ************************************
// mqttTaskCB()
//
// 
// ************************************
void mqttTaskCB() {
  char topic[32];
    
  // MQTT not connected? Connect!
  if (!mqttClient.connected()) {
    if(!mqttConnect()) {
      // If connection problem, return
      env["status"] = "MQTT connection error"+String(mqttClient.state()); 
      return;
    }
  }

  // Now prepare MQTT message...
  JsonObject root = env.as<JsonObject>();
 
  for (JsonPair keyValue : root) {
    sprintf(topic,"%s/%s",config.client_id,keyValue.key().c_str());
    String value = keyValue.value().as<String>();
    if(mqttClient.publish(topic,value.c_str())) {
      DEBUG_PRINTLN("[MQTT] Publish "+String(topic)+":"+value);
    } else {
      DEBUG_PRINTLN("[MQTT] Publish failed!");
      env["status"] = "MQTT publish failed: "+String(mqttClient.state());
    }
  }
}

bool mqttConnect() {
  if(strlen(config.broker_host) > 0) {
    DEBUG_PRINT("[MQTT] Attempting connection to "+String(config.broker_host)+":"+String(config.broker_port));
    mqttClient.setServer(config.broker_host,config.broker_port);
    mqttClient.setCallback(mqttCallback);
    if (mqttClient.connect(config.client_id)) {
      DEBUG_PRINTLN("[MQTT] Connected as "+String(config.client_id));
      env["status"] = "MQTT connected as "+String(config.client_id);
      return true;
    } else {
      DEBUG_PRINTLN("[MQTT] Connection failed");   
      return false;
    }
  }
  return false;
}
