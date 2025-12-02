#ifndef ROBOT_COMMAND_H_
#define ROBOT_COMMAND_H_

#include <stdint.h>

enum MessageType {
    COMMAND = 0,
    DISCOVERY_REQUEST = 2,
    DISCOVERY_REPLY = 3
};

struct RobotCommand {
    uint8_t type;       // MessageType: COMMAND, SYNC, DISCOVERY_REQUEST, DISCOVERY_REPLY
    uint8_t flags;      // Bit flags: 0x01=forward, 0x02=backward, 0x04=left, 0x08=right, 0x10=spin_left, 0x20=spin_right (0 for SYNC)
    uint8_t speed;      // 0-100 (0 for SYNC)
    uint8_t servo1_pos; // 0-180
    uint8_t servo2_pos; // 0-180
    uint32_t timestamp; // millis() from transmitter
    uint8_t checksum;   // Simple checksum: type ^ flags ^ speed ^ servo1_pos ^ servo2_pos ^ (timestamp & 0xFF)
    char payload[32];   // Payload for discovery messages
} __attribute__((packed));

#endif  // ROBOT_COMMAND_H_