// ************************************
// mqttConnect()
//
//
// ************************************
void mqttConnect() {
  uint8_t timeout=10;
  if(strlen(config.broker_host) > 0) {
    DEBUG_PRINTLN("[MQTT] Attempting connection to "+String(config.broker_host)+":"+String(config.broker_port));
    while((!mqttClient.connected())&&(timeout > 0)) {
      // Attempt to connect
      if (mqttClient.connect(config.client_id)) {
        // Once connected, publish an announcement...
        DEBUG_PRINTLN("[MQTT] Connected as "+String(config.client_id));
      } else {
        timeout--;
        delay(500);
      }
    }
    if(!mqttClient.connected()) {
      DEBUG_PRINTLN("[MQTT] Connection failed");    
    }
  }
}

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
    mqttConnect();
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
    }
  }
}
