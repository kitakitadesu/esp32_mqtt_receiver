#include "tasks/espnow_listener.h"
#include "robot_command.h"

#include <Arduino.h>

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

  // Verify checksum
  uint8_t expected_checksum = cmd.type ^ cmd.flags ^ cmd.speed ^ (cmd.timestamp & 0xFF);
  if (expected_checksum != cmd.checksum) {
    Serial.println("Checksum mismatch, ignoring packet");
    return;
  }

  if (cmd.type == COMMAND) {
    // Reconstruct the command string
    String dirStr = "";
    if (cmd.flags & 0x01) dirStr += "f";
    if (cmd.flags & 0x02) dirStr += "b";
    if (cmd.flags & 0x04) dirStr += "l";
    if (cmd.flags & 0x08) dirStr += "r";
    if (cmd.flags & 0x10) dirStr += "ql";
    if (cmd.flags & 0x20) dirStr += "qr";

    if (dirStr.length() == 0) {
      dirStr = "stop";
    }

    Serial.printf("%s %d\n", dirStr.c_str(), cmd.speed);
  } else if (cmd.type == DISCOVERY_REQUEST) {
    if (strcmp(cmd.payload, "bocchi") == 0) {
      // Reply with robot name and MAC
      RobotCommand replyCmd;
      replyCmd.type = DISCOVERY_REPLY;
      replyCmd.flags = 0;
      replyCmd.speed = 0;
      replyCmd.timestamp = millis();
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
