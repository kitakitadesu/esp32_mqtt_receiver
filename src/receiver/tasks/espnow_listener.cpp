#include "tasks/espnow_listener.h"
#include "robot_command.h"

#include <Arduino.h>

#if defined(ESP8266)
#include <Servo.h>
#else
#include <ESP32Servo.h>
#endif

#ifndef ROBOT_NAME
#define ROBOT_NAME "Unknown"
#endif

#if defined(ESP8266)
extern "C" {
#include <espnow.h>
}
#include <ESP8266WiFi.h>
#else
#include <esp_now.h>
#include <WiFi.h>
#endif

namespace tasks::espnow {
namespace {
bool g_initialized = false;
const uint8_t BROADCAST_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Motor Left
#if defined(ESP8266)
const int in1 = D7; // ESP8285 GPIO1
const int in2 = D1; // ESP8285 GPIO2
const int pwm1 = D2; // ESP8285 GPIO3
#else
const int in1 = 16; //esp32
const int in2 = 5; //esp32
const int pwm1 = 4; //esp32
#endif

// Motor Right
#if defined(ESP8266)
const int in3 = D3; // ESP8285 GPIO4
const int in4 = D4;  // ESP8285 GPIO5
const int pwm2 = D8; // ESP8285 GPIO6
#else
const int in3 = 33; //esp32
const int in4 = 27; //esp32
const int pwm2 = 32; //esp32
#endif

// Servo pins
#if defined(ESP8266)
const int servo1Pin = D5; // ESP8285 GPIO7
const int servo2Pin = D6; // ESP8285 GPIO8
#else
const int servo1Pin = 14; //esp32
const int servo2Pin = 12; //esp32
#endif

Servo servo1;
Servo servo2;

int currentServo1Pos = 90;
int currentServo2Pos = 90;

void smoothServoWrite(Servo &servo, int &currentPos, int targetPos, int step, int delayTime) {
  if (targetPos > currentPos) {
    for (int pos = currentPos; pos <= targetPos; pos += step) {
      servo.write(pos);
      delay(delayTime);
    }
  } else {
    for (int pos = currentPos; pos >= targetPos; pos -= step) {
      servo.write(pos);
      delay(delayTime);
    }
  }
  currentPos = targetPos;
}

void moveForward(int speed) {
  digitalWrite(in1, HIGH);
  digitalWrite(in2, LOW);
  digitalWrite(in3, HIGH);
  digitalWrite(in4, LOW);
  analogWrite(pwm1, speed);
  analogWrite(pwm2, speed);
}

void moveBackward(int speed) {
  digitalWrite(in1, LOW);
  digitalWrite(in2, HIGH);
  digitalWrite(in3, LOW);
  digitalWrite(in4, HIGH);
  analogWrite(pwm1, speed);
  analogWrite(pwm2, speed);
}

void turnLeft(int speed) {  
  // ล้อซ้ายถอยหลัง, ล้อขวาเดินหน้า → เลี้ยวซ้าย
  digitalWrite(in2, LOW);
  digitalWrite(in1, HIGH);
  digitalWrite(in4, HIGH);
  digitalWrite(in3, LOW);
  analogWrite(pwm1, speed);
  analogWrite(pwm2, speed);
}

void turnRight(int speed) { 
  // ล้อซ้ายเดินหน้า, ล้อขวาถอยหลัง → เลี้ยวขวา
  digitalWrite(in2, HIGH);
  digitalWrite(in1, LOW);
  digitalWrite(in4, LOW);
  digitalWrite(in3, HIGH);
  analogWrite(pwm1, speed);
  analogWrite(pwm2, speed);
}

void stopMotors() {
  digitalWrite(in1, LOW);
  digitalWrite(in2, LOW);
  digitalWrite(in3, LOW);
  digitalWrite(in4, LOW);
  analogWrite(pwm1, 0);
  analogWrite(pwm2, 0);
}

void sendEspNowPacket(const uint8_t *mac, const uint8_t *data, size_t len) {
  if (!g_initialized) {
    return;
  }

  // Add peer
#if defined(ESP8266)
  esp_now_add_peer((uint8_t*)mac, ESP_NOW_ROLE_COMBO, 0, nullptr, 0);
  esp_now_send((uint8_t*)mac, (uint8_t*)data, len);
#else
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, mac, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  esp_err_t result = esp_now_add_peer(&peerInfo);
  if (result != ESP_OK && result != ESP_ERR_ESPNOW_EXIST) {
    Serial.println("Failed to add peer");
    return;
  }
  esp_now_send(mac, data, len);
#endif
}

void parseMessage(const uint8_t *data, int len) {
  if (len != sizeof(RobotCommand)) {
    return;
  }

  RobotCommand cmd;
  memcpy(&cmd, data, sizeof(RobotCommand));

  // Verify checksum - different calculation for discovery vs command messages
  uint8_t expected_checksum;
  if (cmd.type == COMMAND) {
    expected_checksum = cmd.type ^ cmd.flags ^ cmd.speed ^ cmd.servo1_pos ^ cmd.servo2_pos ^ (cmd.timestamp & 0xFF);
  } else {
    // For discovery messages, don't include servo positions in checksum
    expected_checksum = cmd.type ^ cmd.flags ^ cmd.speed ^ (cmd.timestamp & 0xFF);
  }
  
  if (expected_checksum != cmd.checksum) {
    Serial.println("Checksum mismatch, ignoring packet");
    return;
  }

  // Print received message
  if (cmd.type == COMMAND) {
    Serial.printf("Received COMMAND: flags=0x%02X, speed=%d, servo1=%d, servo2=%d, timestamp=%lu\n",
                  cmd.flags, cmd.speed, cmd.servo1_pos, cmd.servo2_pos, cmd.timestamp);
  } else if (cmd.type == DISCOVERY_REQUEST) {
    Serial.printf("Received DISCOVERY_REQUEST: payload='%s'\n", cmd.payload);
  } else if (cmd.type == DISCOVERY_REPLY) {
    Serial.printf("Received DISCOVERY_REPLY: payload='%s'\n", cmd.payload);
  } else {
    Serial.printf("Received unknown type: %d\n", cmd.type);
  }

  if (cmd.type == COMMAND) {
    // Control servos
    if (cmd.servo1_pos != currentServo1Pos) {
      servo1.write(cmd.servo1_pos);
      currentServo1Pos = cmd.servo1_pos;
    }
    if (cmd.servo2_pos != currentServo2Pos) {
      servo2.write(cmd.servo2_pos);
      currentServo2Pos = cmd.servo2_pos;
    }

    // Control motors
    int motorSpeed = map(cmd.speed, 0, 100, 0, 255);
    if (cmd.flags & 0x01) { // forward
      moveForward(motorSpeed);
    } else if (cmd.flags & 0x02) { // backward
      moveBackward(motorSpeed);
    } else if (cmd.flags & 0x04) { // left
      turnLeft(motorSpeed);
    } else if (cmd.flags & 0x08) { // right
      turnRight(motorSpeed);
    } else if (cmd.flags & 0x10) { // spin_left
      turnLeft(motorSpeed);
    } else if (cmd.flags & 0x20) { // spin_right
      turnRight(motorSpeed);
    } else {
      stopMotors();
    }
  } else if (cmd.type == DISCOVERY_REQUEST) {
    if (strcmp(cmd.payload, "bocchi") == 0) {
      // Reply with robot name and MAC
      RobotCommand replyCmd;
      replyCmd.type = DISCOVERY_REPLY;
      replyCmd.flags = 0;
      replyCmd.speed = 0;
      replyCmd.servo1_pos = 90; // default
      replyCmd.servo2_pos = 90; // default
      replyCmd.timestamp = millis();
      // For discovery messages, don't include servo positions in checksum
      replyCmd.checksum = DISCOVERY_REPLY ^ 0 ^ 0 ^ (replyCmd.timestamp & 0xFF);
      snprintf(replyCmd.payload, sizeof(replyCmd.payload), "%s %s", ROBOT_NAME, WiFi.macAddress().c_str());

      Serial.printf("Replying to discovery: %s\n", replyCmd.payload);
      sendEspNowPacket(BROADCAST_MAC, reinterpret_cast<const uint8_t*>(&replyCmd), sizeof(replyCmd));
    }
  } else {
    Serial.println("Unknown message type, ignoring");
  }
}

#if defined(ESP8266)
void onReceive(uint8_t *mac, uint8_t *data, uint8_t len) {
  parseMessage(data, static_cast<int>(len));
}
#else
void onReceive(const uint8_t *mac, const uint8_t *data, int len) {
  parseMessage(data, len);
}
#endif

bool ensureWifiStaMode() {
  if (WiFi.getMode() != WIFI_STA) {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
  }
  return true;
}

bool initEspNow() {
  if (!ensureWifiStaMode()) {
    return false;
  }

#if defined(ESP8266)
  if (esp_now_init() != 0) {
    Serial.println("ESP-NOW init failed");
    return false;
  }
  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  esp_now_register_recv_cb(onReceive);
#else
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    return false;
  }
  esp_now_register_recv_cb(onReceive);
#endif

  return true;
}

}  // namespace

void init() {
  if (g_initialized) {
    return;
  }

  // Setup pins
  pinMode(in1, OUTPUT);
  pinMode(in2, OUTPUT);
  pinMode(pwm1, OUTPUT);
  pinMode(in3, OUTPUT);
  pinMode(in4, OUTPUT);
  pinMode(pwm2, OUTPUT);

  servo1.attach(servo1Pin);
  servo2.attach(servo2Pin);
  
  servo1.write(currentServo1Pos);
  servo2.write(currentServo2Pos);

  if (initEspNow()) {
    g_initialized = true;
    Serial.println("ESP-NOW receiver ready");
    Serial.printf("MAC Address: %s\n", WiFi.macAddress().c_str());
  }
}

void loop() {
  // No periodic work required; placeholder for future diagnostics.
}

}  // namespace tasks::espnow
