# ESP-NOW Transmitter Serial Interface

This document describes the serial command format for controlling ESP-NOW robot transmitters.

## Serial Communication Setup

- **Baud Rate**: 115200
- **Data Bits**: 8
- **Parity**: None
- **Stop Bits**: 1
- **Flow Control**: None

## Command Format

### Movement Commands
```
[MAC_ADDRESS]directions speed [servo1] [servo2]
```

- **MAC_ADDRESS**: Target robot's MAC address in hexadecimal format `XX:XX:XX:XX:XX:XX`
- **directions**: One or more direction characters (see Direction Codes below)
- **speed**: Integer value from 0-100 (0 = stop, 100 = maximum speed)
- **servo1**: Optional servo 1 position (0-180 degrees, default 90) - **no brackets**
- **servo2**: Optional servo 2 position (0-180 degrees, default 90) - **no brackets**

### Direction Codes

| Code | Description | Flag Value |
|------|-------------|------------|
| f    | Forward     | 0x01       |
| b    | Backward    | 0x02       |
| l    | Left turn   | 0x04       |
| r    | Right turn  | 0x08       |
| ql   | Spin left   | 0x10       |
| qr   | Spin right  | 0x20       |

### Discovery Command
```
discovery
```

Broadcasts a discovery request to find available robots on the network.

## Examples

### Basic Movement
```
[24:0A:C4:12:34:56]f 50
```
Move forward at speed 50 to robot with MAC 24:0A:C4:12:34:56

```
[24:0A:C4:12:34:56]b 75
```
Move backward at speed 75

```
[24:0A:C4:12:34:56]l 60
```
Turn left at speed 60

```
[24:0A:C4:12:34:56]r 60
```
Turn right at speed 60

### Spin Movement
```
[24:0A:C4:12:34:56]ql 40
```
Spin left (rotate in place) at speed 40

```
[24:0A:C4:12:34:56]qr 40
```
Spin right (rotate in place) at speed 40

### Stop Command
```
[24:0A:C4:12:34:56] 0
```
Stop all movement (empty direction string with speed 0)

### Servo Control
```
[24:0A:C4:12:34:56]f 50 45 135
```
Move forward at speed 50 with servo1 at 45° and servo2 at 135°

```
[24:0A:C4:12:34:56] 0 90 0
```
Stop movement with servo1 at 90° (center) and servo2 at 0° (full left)

```
[24:0A:C4:12:34:56]l 30 180
```
Turn left at speed 30 with servo1 at 180° (full right), servo2 stays at default 90°

### Discovery
```
discovery
```
Send discovery broadcast to find robots

## Response Format

The transmitter will respond with detailed information about sent commands:

```
Sending to MAC 24:0A:C4:12:34:56: flags=0x01, speed=50, servo1=90, servo2=90, ts=12345, chk=0xAB
```

For discovery replies:
```
Discovery reply: RobotA 24:0A:C4:12:34:56
```

## Error Messages

- `No speed specified` - Command missing speed parameter
- `Invalid MAC address format` - MAC address not in correct format
- `Missing closing bracket ']'` - Command missing closing bracket
- `Input must start with '['` - Command doesn't start with opening bracket

## Notes

- Commands are case-sensitive
- Multiple direction codes can be combined (though typically only one is used)
- Servo positions default to 90 degrees if not specified
- Servo positions are clamped to 0-180 degree range
- Speed values are clamped to 0-100 range
- Commands are processed line-by-line (terminated by \n or \r)
- The transmitter remembers the last used MAC address for convenience