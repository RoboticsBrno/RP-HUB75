# USB

| Code     | Description         |
| -------- | ------------------- |
| 0x20     | Get device data     |

## Supported Device Types

- `0x00`: No device connected
- `0x01`: Keyboard
- `0x02`: Mouse
- `0x03`: Gamepad
- `0x04`: Mass Storage

## Get Device Data
### Request
Send **0x20** to pull queued USB reports from the attached device (only one device is supported).

### Response Format
```
[byte: 0x20] [uint16: packet_length] [byte: device_type] [byte: report_count] [reports...]
```

- `packet_length`: bytes following this field.
- `report_count`: number of reports included (0 when no data or no device).
### Device Data Formats

#### No Device (0x00)
`report_count` is 0; no additional bytes.

#### Keyboard (0x01)
Each report:
```
[byte: modifier_keys] [byte: reserved] [6 bytes: key_codes]
```

**Modifier Keys Flags:**
- Bit 0: Left Ctrl
- Bit 1: Left Shift
- Bit 2: Left Alt
- Bit 3: Left GUI (Windows/Command)
- Bit 4: Right Ctrl
- Bit 5: Right Shift
- Bit 6: Right Alt
- Bit 7: Right GUI

#### Mouse (0x02)
Each report:
```
[byte: buttons] [byte: x_movement] [byte: y_movement] [byte: wheel]
```

**Buttons:**
- Bit 0: Left button
- Bit 1: Right button
- Bit 2: Middle button
- Bits 3-7: Reserved

#### Gamepad (0x03)
Each report:
```
[byte: buttons] [byte: lt_analog] [byte: rt_analog] [byte: lx_analog] [byte: ly_analog] [byte: rx_analog] [byte: ry_analog]
```

**Buttons:**
- Bit 0: A / Cross
- Bit 1: B / Circle
- Bit 2: X / Square
- Bit 3: Y / Triangle
- Bit 4: LB / L1
- Bit 5: RB / R1
- Bit 6: Back / Select
- Bit 7: Start

#### Mass Storage (0x04)

**TODO**