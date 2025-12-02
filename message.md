# ESP-NOW Robot Communication Protocol

This document describes the message protocol used for ESP-NOW communication between robot transmitters and receivers.

## Message Structure

All messages use the `RobotCommand` struct, which is packed to ensure consistent byte layout:

```cpp
struct RobotCommand {
    uint8_t type;       // MessageType: COMMAND, DISCOVERY_REQUEST, DISCOVERY_REPLY
    uint8_t flags;      // Bit flags for movement directions
    uint8_t speed;      // Movement speed (0-100)
    uint8_t servo1_pos; // Servo 1 position (0-180 degrees)
    uint8_t servo2_pos; // Servo 2 position (0-180 degrees)
    uint32_t timestamp; // Timestamp from transmitter (millis())
    uint8_t checksum;   // Simple XOR checksum
    char payload[32];   // Payload for discovery messages
} __attribute__((packed));
```

## Message Types

### COMMAND (0)
Control message for robot movement and servo positioning.

- **flags**: Bit flags indicating movement direction
- **speed**: Motor speed (0-100, maps to PWM 0-255)
- **servo1_pos**: Position for servo 1 (0-180 degrees, default 90)
- **servo2_pos**: Position for servo 2 (0-180 degrees, default 90)
- **payload**: Unused (set to empty string)

### DISCOVERY_REQUEST (2)
Broadcast message to discover available robots.

- **flags**: 0
- **speed**: 0
- **servo1_pos**: 90 (default)
- **servo2_pos**: 90 (default)
- **payload**: "bocchi" (identification string)

### DISCOVERY_REPLY (3)
Response from robot to discovery request.

- **flags**: 0
- **speed**: 0
- **servo1_pos**: 90 (default)
- **servo2_pos**: 90 (default)
- **payload**: "RobotName MAC:XX:XX:XX:XX:XX:XX" (robot name and MAC address)

## Direction Flags

Movement directions are encoded as bit flags in the `flags` field:

| Bit | Value | Direction | Description |
|-----|-------|-----------|-------------|
| 0   | 0x01  | forward   | Move forward |
| 1   | 0x02  | backward | Move backward |
| 2   | 0x04  | left     | Turn left |
| 3   | 0x08  | right    | Turn right |
| 4   | 0x10  | spin_left| Spin left (rotate in place) |
| 5   | 0x20  | spin_right| Spin right (rotate in place) |

Multiple flags can be combined, though typically only one direction is active at a time.

## Checksum Calculation

The checksum is calculated as:
```
checksum = type ^ flags ^ speed ^ servo1_pos ^ servo2_pos ^ (timestamp & 0xFF)
```

Messages with invalid checksums are ignored.

## Transmitter Usage

### Serial Command Format
```
[MAC_ADDRESS]directions speed [servo1] [servo2]
```

- **MAC_ADDRESS**: Target robot's MAC address in format XX:XX:XX:XX:XX:XX
- **directions**: String of direction characters (f, b, l, r, ql, qr)
- **speed**: Integer speed value (0-100)
- **servo1**: Optional servo 1 position (0-180, default 90) - **no brackets in command**
- **servo2**: Optional servo 2 position (0-180, default 90) - **no brackets in command**

Examples:
- `[24:0A:C4:12:34:56]f 50` - Move forward at speed 50
- `[24:0A:C4:12:34:56]f 50 45 135` - Move forward at speed 50 with servos at 45° and 135°
- `[24:0A:C4:12:34:56] 0 90 0` - Stop with servo1 at 90° and servo2 at 0°

### Discovery
Send `discovery` (without brackets) to broadcast discovery requests.

## Receiver Behavior

- **COMMAND**: Controls motors based on flags and speed, sets servo positions
- **DISCOVERY_REQUEST**: Replies with robot name and MAC address if payload matches "bocchi"
- **DISCOVERY_REPLY**: Logged to serial (received by transmitter)

## Hardware Mapping

### Motor Control
- Left motor: Pins 13 (IN1), 14 (IN2), 12 (PWM)
- Right motor: Pins 33 (IN3), 27 (IN4), 32 (PWM)

### Servo Control
- Servo 1: Pin 26
- Servo 2: Pin 22

## Notes

- All communication is peer-to-peer using ESP-NOW protocol
- No encryption is used
- Messages are sent on channel 0
- Servo positions default to 90 degrees if not specified
- Speed 0 stops motors regardless of direction flags