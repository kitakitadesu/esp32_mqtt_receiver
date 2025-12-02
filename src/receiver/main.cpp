#include <Arduino.h>

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#else
#include <WiFi.h>
#endif

#include "tasks/espnow_listener.h"

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("=== Communication Robot ===");

  tasks::espnow::init();
}

void loop() {
  tasks::espnow::loop();
  delay(10);
}