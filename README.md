# ESP32 Communication Robot

This project implements an ESP-NOW based robot control system with motor and servo control capabilities.

## Features

- **ESP-NOW Communication**: Peer-to-peer wireless communication between transmitter and receiver
- **Motor Control**: Bidirectional motor control with variable speed
- **Servo Control**: Dual servo control with position commands
- **Discovery Protocol**: Automatic robot discovery and identification
- **Multiple Board Support**: Compatible with various ESP32 development boards

## Supported Boards

### ESP32 Boards
- `arduino_nano_esp32` - Arduino Nano ESP32
- `upesy_wroom` - uPesy ESP32 Wroom DevKit
- `node32s` - Node32s (NEW!)
- `nodemcuv2` - NodeMCU v2 (ESP8266)

## Build Environments

### Receiver Environments (Robot Controllers)
These environments build the robot receiver firmware with motor and servo control:

- `RobotA` - Arduino Nano ESP32, robot name "RobotA"
- `RobotB1` - Arduino Nano ESP32, robot name "RobotB1"
- `RobotB2` - Arduino Nano ESP32, robot name "RobotB2"
- `RobotC` - Arduino Nano ESP32, robot name "RobotC"
- `RobotD` - Arduino Nano ESP32, robot name "RobotD"
- `node32s_receiver` - Node32s, robot name "Node32sRobot"
- `upesy_wroom_receiver` - uPesy Wroom
- `arduino_nano_esp32_receiver` - Arduino Nano ESP32, robot name "DefaultRobot"
- `nodemcuv2_receiver` - NodeMCU v2
- `nodemcuv2_RobotA` - NodeMCU v2, robot name "RobotA"
- `nodemcuv2_RobotB1` - NodeMCU v2, robot name "RobotB1"
- `nodemcuv2_RobotB2` - NodeMCU v2, robot name "RobotB2"
- `nodemcuv2_RobotC` - NodeMCU v2, robot name "RobotC"
- `nodemcuv2_RobotD` - NodeMCU v2, robot name "RobotD"

### Transmitter Environments (Remote Controllers)
These environments build the transmitter firmware for sending commands:

- `upesy_wroom_transmitter` - uPesy Wroom
- `arduino_nano_esp32_transmitter` - Arduino Nano ESP32
- `node32s_transmitter` - Node32s (NEW!)
- `nodemcuv2_transmitter` - NodeMCU v2

### Legacy Environments
- `mqtt` - MQTT-based communication (legacy)

## Building

Use PlatformIO to build for your target board:

```bash
# Build for Node32s receiver
pio run -e node32s_receiver

# Build for Node32s transmitter
pio run -e node32s_transmitter

# Build for Arduino Nano ESP32 RobotA
pio run -e RobotA
```

## Hardware Pin Configuration

### Motor Control Pins
- Left Motor: IN1=13, IN2=14, PWM=12
- Right Motor: IN3=33, IN4=27, PWM=32

### Servo Control Pins
- Servo 1: Pin 26
- Servo 2: Pin 22

## Usage

1. **Upload receiver firmware** to your robot ESP32 board
2. **Upload transmitter firmware** to your controller ESP32 board
3. **Connect to transmitter serial** at 115200 baud
4. **Send commands** using the format: `[MAC_ADDRESS]directions speed [servo1] [servo2]`

### Example Commands
```
# Discover robots
discovery

# Move forward at speed 50
[24:0A:C4:12:34:56]f 50

# Turn left at speed 30 with servo1 at 45°
[24:0A:C4:12:34:56]l 30 45

# Stop with servos at center position
[24:0A:C4:12:34:56] 0 90 90
```

## Documentation

- [Message Protocol](message.md) - ESP-NOW message format and structure
- [Transmitter Interface](transmitter.md) - Serial command format and examples

## Dependencies

- ESP32Servo library for servo control
- PubSubClient for MQTT (legacy)
- Arduino-PS2X-ESP32 for gamepad support (future use)

## License

This project is open source. See individual files for license information.