# ForgeBoot

**A custom UART-based firmware bootloader for the STM32F411CEU6 (Black Pill).**

ForgeBoot is an embedded firmware-update system built around a custom STM32 bootloader and a Python host-side uploader. It receives application firmware over UART, programs the STM32 internal Flash, verifies the programmed image using CRC32, stores firmware metadata, and transfers execution to the application when validation succeeds.

> **Release:** v0.1  
> **Target MCU:** STM32F411CEU6  
> **Transport:** USART1, 115200 baud, 8-N-1  
> **Status:** Successfully tested on physical hardware

## Highlights

- Custom register-level bootloader for STM32F411CEU6.
- UART firmware transfer using a framed packet protocol.
- Packet sequence numbers and ACK/NACK responses.
- CRC32 packet integrity checks and complete-image CRC validation.
- Internal Flash erase and word-programming routines.
- Metadata stored separately from the application.
- Metadata commit marker programmed last.
- Application vector-table and stack/reset-vector sanity checks.
- Recovery mode selected using a physical button.
- Python command-line uploader with firmware inspection and upload support.
- Physical hardware validation using an ST-LINK/V2 clone and CP2102 USB-to-UART adapter.

## System overview

```text
+----------------------------+
|        Host PC             |
|                            |
|  Python firmware uploader  |
|  - Inspect firmware        |
|  - Calculate CRC32         |
|  - Packetise image         |
|  - Retry / check ACK       |
+-------------+--------------+
              |
              | USB-to-UART adapter
              | 115200 baud, 8-N-1
              |
+-------------v--------------+
|      STM32F411CEU6          |
|                            |
|  ForgeBoot bootloader      |
|  - Recovery button         |
|  - UART protocol           |
|  - Flash management        |
|  - CRC32 validation        |
|  - Metadata management    |
|                            |
|  Internal Flash            |
|  - Sector 0: Bootloader    |
|  - Sector 1: Metadata      |
|  - Sectors 2–7: App        |
+-------------+--------------+
              |
              | Valid image
              v
+----------------------------+
|      User application      |
+----------------------------+
```

## Flash memory layout

The STM32F411CEU6 has 512 KiB of internal Flash. ForgeBoot uses the following layout:

| Region | Address range | Size | Purpose |
|---|---:|---:|---|
| Sector 0 | `0x08000000–0x08003FFF` | 16 KiB | Bootloader |
| Sector 1 | `0x08004000–0x08007FFF` | 16 KiB | Firmware metadata |
| Sectors 2–7 | `0x08008000–0x0807FFFF` | 480 KiB | Application |

The application is linked to start at `0x08008000`. The bootloader's Flash erase/program routines are intended to protect sector 0 from application-update operations.

## Hardware connections

### USB-to-UART adapter

Use a **3.3 V UART logic-level** USB-to-TTL adapter.

| CP2102 | STM32F411CEU6 Black Pill | Purpose |
|---|---|---|
| TXD | PA10 | USART1_RX |
| RXD | PA9 | USART1_TX |
| GND | GND | Common ground |

TX and RX must be crossed. Do not connect the adapter's VCC pin when the board is powered separately. Confirm the adapter's UART logic level is 3.3 V.

### Recovery button

| Signal | Connection |
|---|---|
| PB12 | Recovery-button input |
| Button other terminal | GND |

PB12 is configured as an input with an internal pull-up. The button is active-low.

### Onboard LED application test

The tested application configures PC13 as an output and toggles it. On the tested Black Pill board, the onboard LED is active-low.

## Firmware update flow

1. The STM32 starts in the bootloader.
2. ForgeBoot checks whether recovery mode has been requested.
3. If recovery is requested, it waits for firmware packets over USART1.
4. The host uploader sends a `BEGIN` packet containing image size and expected image CRC32.
5. The bootloader erases application sectors 2–7.
6. The host sends sequenced `DATA` packets. The bootloader checks packet integrity and programs the image into Flash.
7. The host sends an `END` packet.
8. The bootloader checks that the expected number of bytes was received and calculates CRC32 over the programmed application.
9. If validation succeeds, metadata is written. The commit marker is programmed last.
10. On the next boot, the bootloader validates metadata and the application image before transferring execution.

## UART packet format

Each packet has the following structure:

| Field | Size | Description |
|---|---:|---|
| Sync | 2 bytes | `0xA5 0x5A` |
| Type | 1 byte | Packet command/response type |
| Sequence | 2 bytes | Little-endian sequence number |
| Length | 2 bytes | Little-endian payload length |
| Payload | Variable | Command-specific data |
| CRC32 | 4 bytes | Little-endian CRC32 |

CRC32 is calculated over the packet header fields beginning with `Type` through the end of the payload. The sync bytes and CRC field itself are excluded.

The implementation uses the standard reflected CRC-32 polynomial `0xEDB88320`, initial value `0xFFFFFFFF`, and final XOR `0xFFFFFFFF`, compatible with Python's `zlib.crc32`.

See [UART protocol documentation](Documentation/UART_Protocol.md) for packet commands and details.

## Host uploader

The Python uploader is located in `Host/` in the suggested repository layout.

It supports:
- Offline firmware inspection.
- Basic vector-table sanity checks.
- Firmware size and CRC32 calculation.
- Packet generation.
- Serial upload.
- ACK/NACK and sequence validation.
- Retries after response timeouts.
- Self-tests using a mock serial interface.

### Dependencies

- Python 3
- `pyserial`

Install the dependency:

```bash
python -m pip install pyserial
```

### Inspect a firmware image

```bash
python forgeboot_uploader.py path/to/application.bin
```

### Run host-side self-tests

```bash
python forgeboot_uploader.py --self-test
```

### Upload firmware

Close any serial terminal currently using the target COM port, then run:

```bash
python forgeboot_uploader.py path/to/application.bin --upload --port COM19
```

Replace `COM19` with the serial port assigned to your adapter.

The target must be in recovery mode and connected to the host UART adapter before the upload begins. Follow the recovery-button procedure documented in [Build and Upload](Documentation/Build_and_Upload.md).

## Building the firmware

The firmware projects were developed using STM32CubeIDE for the STM32F411CEU6.

The repository should contain the bootloader and application source projects, including their linker scripts and required project configuration. Build each project separately.

Important linker configuration:

- Bootloader Flash origin: `0x08000000`
- Bootloader Flash length: `16K`
- Application Flash origin: `0x08008000`
- Application Flash length: `480K`
- SRAM: `128K`, beginning at `0x20000000`

Enable binary output (`.bin`) in the project build settings. The host uploader expects a raw binary application image, not an ELF or HEX file.

Detailed instructions: [Build and Upload](Documentation/Build_and_Upload.md).

## Physical hardware validation

ForgeBoot v0.1 was tested on an STM32F411CEU6 Black Pill using:
- ST-LINK/V2-compatible programmer/debugger for programming and debugging.
- CP2102 USB-to-UART adapter for bootloader communication.
- Tera Term for observing bootloader UART output.
- Python host uploader for firmware transfer.

The physical test successfully demonstrated:
- Bootloader startup over UART.
- Recovery-mode firmware reception.
- Successful ACK responses for all transmitted packets.
- Firmware transfer completion.
- Application validation on reboot.
- Transfer of execution to the application.
- Onboard PC13 LED blinking from the uploaded application.

See [Hardware Validation](Documentation/Hardware_Validation.md).

## Repository layout

A suggested layout for the GitHub repository:

```text
ForgeBoot/
├── README.md
├── CHANGELOG.md
├── .gitignore
├── Bootloader/
│   ├── Core/
│   ├── Drivers/
│   └── ...
├── Application/
│   ├── Core/
│   ├── Drivers/
│   └── ...
├── Host/
│   └── forgeboot_uploader.py
├── Documentation/
│   ├── Architecture.md
│   ├── Flash_Memory_Map.md
│   ├── UART_Protocol.md
│   ├── Build_and_Upload.md
│   ├── Hardware_Validation.md
│   └── Limitations_and_Future_Work.md
└── Release/
    └── v0.1/
        └── RELEASE_NOTES.md
```

Adjust folder names to match the actual project files before committing. Do not include machine-specific IDE workspace folders or unnecessary generated build artefacts.

## Current limitations

- **Single application slot:** the update process erases the existing application before programming the new one. A power interruption during an update can leave no valid application; recovery through the bootloader is required.
- **CRC32 is not authentication:** CRC32 detects accidental corruption but does not prove who produced the firmware.
- **No automatic rollback:** there is no second application image from which to roll back.
- **No cryptographic secure boot:** firmware authenticity/signature verification is not implemented.
- **Clock assumption:** the current UART configuration assumes the MCU is running from the default 16 MHz HSI clock. Changing the clock configuration requires reviewing the UART baud-rate and timeout assumptions.
- **Hardware write protection:** sector 0 is protected by the firmware's address checks, but hardware option-byte write protection has not been established as part of this release.

## Future development

Planned areas for later versions include:
- More robust Flash timeout and error cleanup.
- Metadata readback verification.
- Stronger malformed-packet recovery.
- Explicit application handoff and interrupt-state testing.
- Firmware version and hardware compatibility fields.
- A desktop GUI for the host uploader.
- More advanced update strategies and optional cryptographic firmware authentication.

## Release

See [v0.1 release notes](Release/v0.1/RELEASE_NOTES.md).

## Author

**Abhinav Pradeep**  
GitHub: [@abhinavpradeep13](https://github.com/abhinavpradeep13)

---

ForgeBoot is an educational embedded-systems project. Review the source, hardware connections, and limitations before adapting it for a product or safety-critical deployment.
