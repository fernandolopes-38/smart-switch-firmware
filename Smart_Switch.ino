//#include <CloudIoTCore.h>
#include "esp8266_mqtt.h"

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  analogWriteRange(255);
  pinMode(redPin, OUTPUT);
  pinMode(greenPin, OUTPUT);
  pinMode(bluePin, OUTPUT);
  analogWrite(redPin, 255);
  analogWrite(greenPin, 255);
  analogWrite(bluePin, 255);
//  pinMode(switchPin, OUTPUT);
//  analogWrite(switchPin, 255);
//  digitalWrite(switchPin, LOW);
  
  setupCloudIoT(); // Creates globals for MQTT
  pinMode(LED_BUILTIN, OUTPUT);
  timeClient.begin();
}

void loop() {
  if (!mqtt->loop()) {
    mqtt->mqttConnect();
  }
  delay(10); // <- fixes some issues with WiFi stability
  
//  timeClient.update();
//  Serial.println(timeClient.getFormattedTime());
//  Serial.println(timeClient.getHours());
//  Serial.println(timeClient.getMinutes());

  if (autoTurnOff && (millis() - startTimer > timer)) {
    Serial.println("Timer up, turning switch LOW");
//    digitalWrite(switchPin, LOW);
    autoTurnOff = false;
  }

//  if (millis() - lastMillis > 60000) {
//    lastMillis = millis();
//    publishTelemetry(getDefaultSensor());
//  }
}
