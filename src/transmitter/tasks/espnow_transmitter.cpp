#include "tasks/espnow_transmitter.h"
#include "robot_command.h"

#include <Arduino.h>

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
char g_serialBuffer[256];
size_t g_serialBufferLen = 0;
uint8_t g_lastMac[6] = {0};
const uint8_t BROADCAST_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

bool parseMacAddress(const char *str, uint8_t *mac) {
  int values[6];
  if (sscanf(str, "%x:%x:%x:%x:%x:%x", &values[0], &values[1], &values[2], &values[3], &values[4], &values[5]) != 6) {
    return false;
  }
  for (int i = 0; i < 6; ++i) {
    mac[i] = static_cast<uint8_t>(values[i]);
  }
  return true;
}

uint8_t parseDirectionFlags(const char *dirStr) {
  uint8_t flags = 0;
  const char *p = dirStr;
  while (*p) {
    if (*p == 'f') {
      flags |= 0x01;  // forward
    } else if (*p == 'b') {
      flags |= 0x02;  // backward
    } else if (*p == 'l') {
      flags |= 0x04;  // left
    } else if (*p == 'r') {
      flags |= 0x08;  // right
    } else if (*p == 'q') {
      ++p;  // next char
      if (*p == 'l') {
        flags |= 0x10;  // spin left
      } else if (*p == 'r') {
        flags |= 0x20;  // spin right
      }
    }
    ++p;
  }
  return flags;
}

void parseReceivedMessage(const uint8_t *data, int len) {
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

  if (cmd.type == DISCOVERY_REPLY) {
    Serial.printf("Discovery reply: %s\n", cmd.payload);
  }
}

#if defined(ESP8266)
void onReceive(uint8_t *mac, uint8_t *data, uint8_t len) {
  parseReceivedMessage(data, static_cast<int>(len));
}
#else
void onReceive(const uint8_t *mac, const uint8_t *data, int len) {
  parseReceivedMessage(data, len);
}
#endif

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

void processSerialInput() {
  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (g_serialBufferLen > 0) {
        g_serialBuffer[g_serialBufferLen] = '\0';

        // Parse "[mac_address]command speed"
        char *bracketStart = strchr(g_serialBuffer, '[');
        if (bracketStart) {
          char *bracketEnd = strchr(bracketStart + 1, ']');
          if (bracketEnd) {
            *bracketEnd = '\0';
            char *macStr = bracketStart + 1;
            uint8_t mac[6];
            if (parseMacAddress(macStr, mac)) {
              memcpy(g_lastMac, mac, 6);
              const char *commandStr = bracketEnd + 1;
              // Parse command: "directions speed [servo1] [servo2]"
              char *space1 = strchr((char*)commandStr, ' ');
              if (space1) {
                *space1 = '\0';
                const char *dirStr = commandStr;
                const char *speedStr = space1 + 1;
                
                // Parse speed
                char *space2 = strchr((char*)speedStr, ' ');
                int servo1_pos = 90; // default
                int servo2_pos = 90; // default
                
                if (space2) {
                  *space2 = '\0';
                  // Parse servo1
                  char *space3 = strchr(space2 + 1, ' ');
                  if (space3) {
                    *space3 = '\0';
                    servo1_pos = atoi(space2 + 1);
                    servo2_pos = atoi(space3 + 1);
                  } else {
                    servo1_pos = atoi(space2 + 1);
                  }
                  // Clamp servo positions
                  if (servo1_pos < 0) servo1_pos = 0;
                  if (servo1_pos > 180) servo1_pos = 180;
                  if (servo2_pos < 0) servo2_pos = 0;
                  if (servo2_pos > 180) servo2_pos = 180;
                }
                
                uint8_t flags = parseDirectionFlags(dirStr);
                int speed = atoi(speedStr);
                if (speed < 0) speed = 0;
                if (speed > 100) speed = 100;

                Serial.printf("Parsed: dir='%s', speed=%d, servo1=%d, servo2=%d, flags=0x%02X\n", 
                             dirStr, speed, servo1_pos, servo2_pos, flags);

                RobotCommand cmd;
                cmd.type = COMMAND;
                cmd.flags = flags;
                cmd.speed = (uint8_t)speed;
                cmd.servo1_pos = (uint8_t)servo1_pos;
                cmd.servo2_pos = (uint8_t)servo2_pos;
                cmd.timestamp = millis();
                cmd.checksum = COMMAND ^ flags ^ speed ^ servo1_pos ^ servo2_pos ^ (cmd.timestamp & 0xFF);
                memset(cmd.payload, 0, sizeof(cmd.payload));

                Serial.printf("Sending to MAC %02X:%02X:%02X:%02X:%02X:%02X: flags=0x%02X, speed=%d, servo1=%d, servo2=%d, ts=%u, chk=0x%02X\n",
                              mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], flags, speed, cmd.servo1_pos, cmd.servo2_pos, cmd.timestamp, cmd.checksum);
                sendEspNowPacket(mac, reinterpret_cast<const uint8_t*>(&cmd), sizeof(cmd));
              } else {
                Serial.println("No speed specified");
              }
            } else {
              Serial.println("Invalid MAC address format");
            }
          } else {
            Serial.println("Missing closing bracket ']'");
          }
        } else {
          Serial.println("Input must start with '['");
        }

        // Check for discovery command
        if (strcmp(g_serialBuffer, "discovery") == 0) {
          RobotCommand discCmd;
          discCmd.type = DISCOVERY_REQUEST;
          discCmd.flags = 0;
          discCmd.speed = 0;
          discCmd.servo1_pos = 90; // default
          discCmd.servo2_pos = 90; // default
          discCmd.timestamp = millis();
          // For discovery messages, don't include servo positions in checksum
          discCmd.checksum = DISCOVERY_REQUEST ^ 0 ^ 0 ^ (discCmd.timestamp & 0xFF);
          strcpy(discCmd.payload, "bocchi");

          Serial.println("Sending discovery broadcast");
          sendEspNowPacket(BROADCAST_MAC, reinterpret_cast<const uint8_t*>(&discCmd), sizeof(discCmd));
        }

        g_serialBufferLen = 0;
      }
    } else if (g_serialBufferLen < sizeof(g_serialBuffer) - 1) {
      g_serialBuffer[g_serialBufferLen++] = c;
    }
  }
}

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
    Serial.println("ESP-NOW transmitter ready");
    Serial.printf("MAC Address: %s\n", WiFi.macAddress().c_str());
    Serial.println("Enter: [XX:XX:XX:XX:XX:XX]message");
  }
}

void loop() {
  processSerialInput();
}

}  // namespace tasks::espnow