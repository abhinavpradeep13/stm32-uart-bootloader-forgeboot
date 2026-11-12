# UART Protocol

## 1. Serial configuration

- Peripheral: USART1
- TX: PA9, alternate function AF7
- RX: PA10, alternate function AF7
- Baud rate: 115200
- Data bits: 8
- Parity: None
- Stop bits: 1
- Flow control: None

The current implementation assumes a 16 MHz peripheral/core clock configuration when calculating the baud rate and receive timeout. Review these assumptions if clock configuration changes.

## 2. Packet format

| Field | Length | Encoding |
|---|---:|---|
| Sync | 2 bytes | `A5 5A` |
| Type | 1 byte | Command/response identifier |
| Sequence | 2 bytes | Unsigned little-endian |
| Length | 2 bytes | Unsigned little-endian |
| Payload | `Length` bytes | Command-specific |
| CRC32 | 4 bytes | Unsigned little-endian |

CRC32 covers `Type + Sequence + Length + Payload`. It excludes the sync bytes and the CRC field.

The firmware parser supports a maximum payload of 256 bytes. The host uploader's DATA payload includes a 4-byte image offset, so the firmware data portion of a DATA packet is smaller than the maximum packet payload.

## 3. CRC32

ForgeBoot uses the standard reflected CRC-32 algorithm:
- Reflected polynomial: `0xEDB88320`
- Initial value: `0xFFFFFFFF`
- Final XOR: `0xFFFFFFFF`

The standard test vector `123456789` produces `0xCBF43926`. Python's `zlib.crc32` uses a compatible CRC32 variant.

## 4. Update commands

The protocol command identifiers and NACK reason values are defined in the firmware's `protocol.h` and related source files. Keep this document aligned with those definitions if the protocol changes.

### BEGIN
The BEGIN payload is 8 bytes:
- Image size: 4-byte little-endian unsigned integer.
- Expected image CRC32: 4-byte little-endian unsigned integer.

The bootloader validates the requested image size, erases the application region and starts a receiving session.

### DATA
The DATA payload contains:
- Image offset: 4-byte little-endian unsigned integer.
- Firmware data bytes.

The bootloader checks the sequence number, offset, length and packet CRC before programming the image. Image offsets are expected to be contiguous and aligned as required by the flash-word programming implementation. The final partial word is padded with `0xFF` for word programming; image CRC is calculated over the exact image size.

### END
The END payload must be empty. The bootloader verifies that the expected image length was received, calculates CRC32 over the programmed application, and writes metadata if the CRC matches.

### ABORT
The ABORT payload must be empty. It terminates an active receiving session.

## 5. Responses

- ACK: response packet with an empty payload and matching sequence number.
- NACK: response packet with a one-byte reason payload and matching sequence number.

The host uploader validates response framing, CRC and sequence number. It retries on response timeout and rejects a NACK response.

## 6. Sequence numbers and retransmission

Sequence numbers are 16-bit little-endian values. The host uses stop-and-wait transmission: it sends one packet and waits for the corresponding response before advancing.

The bootloader contains duplicate handling for selected repeated commands, including the immediately previous DATA packet, to support retransmission after a lost ACK. This is not a general sliding-window protocol.

## 7. Protocol limitations

The v0.1 parser and host implementation are functional for the tested transfer path, but malformed-frame recovery can be improved. In particular, the handling of oversized or malformed packets should be reviewed and strengthened in a later version. Do not assume the protocol is hardened against arbitrary hostile input.
