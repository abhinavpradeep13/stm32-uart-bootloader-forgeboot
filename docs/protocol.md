# ForgeBoot UART protocol (v0.2)

## Serial settings

115200 baud, 8-N-1. Multi-byte integer fields are little-endian.

## Frame format

```text
SYNC1 SYNC2 TYPE SEQUENCE(16) LENGTH(16) PAYLOAD CRC32(32)
  A5    5A   1B       2B       2B      0..256B   4B
```

CRC32 is calculated over `TYPE + SEQUENCE + LENGTH + PAYLOAD` (the sync bytes and CRC field are excluded).

## Commands

| Type | Name | Payload |
|---:|---|---|
| `0x01` | BEGIN | 4-byte image size followed by 4-byte expected image CRC32 |
| `0x02` | DATA | 4-byte image offset followed by image bytes (up to 252 data bytes per packet) |
| `0x03` | END | Empty |
| `0x04` | ABORT | Empty |
| `0x05` | PING | Empty |
| `0x80` | ACK | Empty |
| `0x81` | NACK | One-byte reason code |

The protocol packet payload maximum is 256 bytes. DATA reserves four payload bytes for the image offset, leaving up to 252 firmware bytes per DATA packet.

## Responses and NACK reasons

ACK echoes the request sequence number. NACK also echoes the sequence number and carries one reason byte:

| Reason | Meaning |
|---:|---|
| `0x01` | Invalid command |
| `0x02` | Invalid length |
| `0x03` | CRC error |
| `0x04` | Invalid sequence |
| `0x05` | Flash error |
| `0x06` | Invalid update state |

## Typical update flow

1. Host sends PING and expects ACK before starting a GUI upload.
2. Host sends BEGIN with image size and image CRC32. The bootloader validates the request and erases the application sectors.
3. Host sends DATA packets in sequence; each packet carries its image offset and data bytes.
4. Host sends END. The bootloader validates the received image CRC, writes application metadata and acknowledges completion.
5. The bootloader automatically jumps to the application after successful completion.

ABORT asks the bootloader to leave an in-progress/error transfer and return to the idle update state. The host uses retries and matching sequence numbers to handle lost responses.

## Safety characteristics

The application sectors are erased at the start of a new BEGIN. The bootloader sector is excluded from the normal update erase/program range. The design is single-slot and has no rollback image. CRC32 is an integrity check, not authentication.
