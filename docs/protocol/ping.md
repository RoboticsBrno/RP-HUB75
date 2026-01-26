# Ping

| Code     | Description |
| -------- | ----------- |
| 0x01     | Ping        |
| 0x02     | Pong        |

## Ping

### Request Format
```
[byte: 0x01]
```

Pings the connected device

### Response Format
```
[byte: 0x02]
```

Sends Pong back

## Pong

### Request Format
```
[byte: 0x02]
```

Recieves pong back confirming everything works
