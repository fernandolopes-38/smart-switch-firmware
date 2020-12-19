/******************************************************************************
 * Copyright 2018 Google
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *****************************************************************************/
// This file contains static methods for API requests using Wifi / MQTT
#include <NTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include "FS.h"

// You need to set certificates to All SSL cyphers and you may need to
// increase memory settings in Arduino/cores/esp8266/StackThunk.cpp:
//   https://github.com/esp8266/Arduino/issues/6811
#include "WiFiClientSecureBearSSL.h"
#include <time.h>

#include <MQTT.h>

#include <CloudIoTCore.h>
#include <CloudIoTCoreMqtt.h>
#include "ciotc_config.h" // Wifi configuration here

#define redPin 14  //D5
#define greenPin 13  //D7
#define bluePin 12  //D6

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, ntp_primary, NTP_OFFSET, NTP_INTERVAL);

int redState = 255;
int greenState = 255;
int blueState = 255;
float gain = 0.55;

boolean autoTurnOn = false;
boolean autoTurnOff = false;
static unsigned long lastMillis = 0;
static unsigned long startTimer = 0;
unsigned long timer = 0;

void setColor(char color[]) {
  if (strcmp(color, "red") == 0) {
    analogWrite(redPin, 0);
    analogWrite(greenPin, 255);
    analogWrite(bluePin, 255);
  } else if (strcmp(color, "green") == 0) {
    analogWrite(redPin, 255);
    analogWrite(greenPin, 0);
    analogWrite(bluePin, 255);
  } else if (strcmp(color, "blue") == 0) {
    analogWrite(redPin, 255);
    analogWrite(greenPin, 255);
    analogWrite(bluePin, 0);
  }
}

void messageReceivedAdvanced(MQTTClient *client, char topic[], char bytes[], int length) {
  if (length > 0) {
    Serial.printf("incoming: %s - %s\n", topic, bytes);
    char topicConfig[] = "/devices/";
    strcat(topicConfig, device_id);
    strcat(topicConfig, "/config");
    Serial.println(topicConfig);
    char topicCommand[] = "/devices/";
    strcat(topicCommand, device_id);
    strcat(topicCommand, "/commands");
    Serial.println(topicCommand);
    
    if(strcmp(topic, topicCommand) == 0) {
      Serial.print("bytes: ");
      Serial.println(bytes);
      if (strcmp(bytes, "on") == 0) {
        Serial.println("Turning switch on");
        analogWrite(redPin, 0);
        analogWrite(greenPin, 0);
        analogWrite(bluePin, 0);
      } else if (strcmp(bytes, "off") == 0) {
        Serial.println("Turning switch off");
        analogWrite(redPin, 255);
        analogWrite(greenPin, 255);
        analogWrite(bluePin, 255);
      } else if (strcmp(bytes, "red") == 0) {
        analogWrite(redPin, 0);
        analogWrite(greenPin, 255);
        analogWrite(bluePin, 255);
      } else if (strcmp(bytes, "green") == 0) {
        analogWrite(redPin, 255);
        analogWrite(greenPin, 0);
        analogWrite(bluePin, 255);
      } else if (strcmp(bytes, "blue") == 0) {
        analogWrite(redPin, 255);
        analogWrite(greenPin, 255);
        analogWrite(bluePin, 0);
      } else {
        char *saveptr;
        char *c;
        char *a;
        char *v;
        c = strtok_r(bytes, ":", &saveptr);
        v = strtok_r(NULL, ":", &saveptr);
        a = strtok_r(NULL, ":", &saveptr);
        Serial.println(c);
        Serial.println(v);
        Serial.println(a);
        if (strcmp(c, "timer") == 0) {
          Serial.print("Setting timer to: ");
          autoTurnOff = true;
          timer = strtoul(v, NULL, 10);
          Serial.println(timer);
          startTimer = millis();
        } else if (strcmp(c, "range") == 0) {
          int range = atoi(v);
          Serial.print("range: ");
          Serial.println(range);
            
          if (a == NULL) {
            gain = float(range/255.0);
            Serial.print("gain: ");
            Serial.println(gain);
            redState = int(redState*gain);
            greenState = int(greenState*gain);
            blueState = int(blueState*gain);
            Serial.print("redState: ");
            Serial.println(redState);
            Serial.print("greenState: ");
            Serial.println(greenState);
            Serial.print("blueState: ");
            Serial.println(blueState);
            analogWrite(redPin, redState);
            analogWrite(greenPin, greenState);
            analogWrite(bluePin, blueState);
          } else {
            if (strcmp(a, "r") == 0) {
              redState = abs(255-range);
              Serial.print("redState: ");
              Serial.println(redState);
              analogWrite(redPin, redState);
            } else if (strcmp(a, "g") == 0) {
              greenState = abs(255-range);
              Serial.print("greenState: ");
              Serial.println(greenState);
              analogWrite(greenPin, greenState);
            } else if (strcmp(a, "b") == 0) {
              blueState = abs(255-range);
              Serial.print("blueState: ");
              Serial.println(blueState);
              analogWrite(bluePin, blueState);
            } 
          }
//          Serial.print("Dimming to: ");
//          int range = atoi(v);
//          Serial.println(range);
        }
      }
    } else {
      
    }
  } else {
    Serial.printf("0\n"); // Success but no message
  }
}
///////////////////////////////

// Initialize WiFi and MQTT for this board
static MQTTClient *mqttClient;
static BearSSL::WiFiClientSecure netClient;
static BearSSL::X509List certList;
static CloudIoTCoreDevice device(project_id, location, registry_id, device_id);
CloudIoTCoreMqtt *mqtt;

///////////////////////////////
// Helpers specific to this board
///////////////////////////////
String getDefaultSensor() {
  return "Wifi: " + String(WiFi.RSSI()) + "db";
}

String getJwt() {
  // Disable software watchdog as these operations can take a while.
  ESP.wdtDisable();
  time_t iat = time(nullptr);
  Serial.println("Refreshing JWT");
  String jwt = device.createJWT(iat, jwt_exp_secs);
  ESP.wdtEnable(0);
  return jwt;
}

static void readDerCert(const char *filename) {
  File ca = SPIFFS.open(filename, "r");
  if (ca)
  {
    size_t size = ca.size();
    uint8_t cert[size];
    ca.read(cert, size);
    certList.append(cert, size);
    ca.close();

    Serial.print("Success to open ca file ");
  }
  else
  {
    Serial.print("Failed to open ca file ");
  }
  Serial.println(filename);
}

static void setupCertAndPrivateKey() {
  // Set CA cert on wifi client
  // If using a static (pem) cert, uncomment in ciotc_config.h:
  certList.append(primary_ca);
  certList.append(backup_ca);
  netClient.setTrustAnchors(&certList);

  device.setPrivateKey(private_key);
  return;

  // If using the (preferred) method with the cert and private key in /data (SPIFFS)
  // To get the private key run
  // openssl ec -in <private-key.pem> -outform DER -out private-key.der

  if (!SPIFFS.begin()) {
    Serial.println("Failed to mount file system");
    return;
  }

  readDerCert("/gtsltsr.crt"); // primary_ca.pem
  readDerCert("/GSR4.crt"); // backup_ca.pem
  netClient.setTrustAnchors(&certList);


  File f = SPIFFS.open("/private-key.der", "r");
  if (f) {
    size_t size = f.size();
    uint8_t data[size];
    f.read(data, size);
    f.close();

    BearSSL::PrivateKey pk(data, size);
    device.setPrivateKey(pk.getEC()->x);

    Serial.println("Success to open private-key.der");
  } else {
    Serial.println("Failed to open private-key.der");
  }

  SPIFFS.end();
}

static void setupWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(100);
  }

  configTime(0, 0, ntp_primary, ntp_secondary);
  Serial.println("Waiting on time sync...");
  while (time(nullptr) < 1510644967) {
    delay(10);
  }
}

///////////////////////////////
// Orchestrates various methods from preceeding code.
///////////////////////////////
bool publishTelemetry(String data) {
  return mqtt->publishTelemetry(data);
}

bool publishTelemetry(const char *data, int length) {
  return mqtt->publishTelemetry(data, length);
}

bool publishTelemetry(String subfolder, String data) {
  return mqtt->publishTelemetry(subfolder, data);
}

bool publishTelemetry(String subfolder, const char *data, int length) {
  return mqtt->publishTelemetry(subfolder, data, length);
}

// TODO: fix globals
void setupCloudIoT() {
  // ESP8266 WiFi setup
  setupWifi();

  // ESP8266 WiFi secure initialization and device private key
  setupCertAndPrivateKey();

  mqttClient = new MQTTClient(512);
  mqttClient->setOptions(180, true, 1000); // keepAlive, cleanSession, timeout
  mqtt = new CloudIoTCoreMqtt(mqttClient, &netClient, &device);
  mqtt->setUseLts(true);
  mqtt->startMQTTAdvanced(); // Opens connection using advanced callback
}
