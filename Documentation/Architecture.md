# ForgeBoot Architecture

## 1. Purpose

ForgeBoot provides a custom firmware update path for the STM32F411CEU6. A host computer transfers a raw application binary over UART. The bootloader receives the image, programs internal Flash, validates the programmed image, stores metadata, and starts the application when the image is valid.

## 2. Main components

### Bootloader
Runs from Flash sector 0. It:
- Initializes the UART and recovery-button input.
- Checks whether an application is valid.
- Enters recovery mode when requested or when no valid application is available.
- Receives and processes update packets.
- Erases and programs application Flash.
- Validates the final image.
- Stores metadata and transfers execution to the application.

### Application
Linked to start at `0x08008000`. The application has its own vector table at that address. The tested application configures PC13 as an output and toggles it to demonstrate that the bootloader can launch a separately linked application.

### Host uploader
A Python program that:
- Inspects the binary image.
- Checks basic vector-table plausibility.
- Calculates CRC32.
- Splits the image into protocol packets.
- Sends packets over a serial port.
- Checks ACK/NACK responses and sequence numbers.
- Retries when a response times out.

## 3. Boot decision

At startup, the bootloader checks the recovery input and application validity. A valid application is started when recovery is not requested. If recovery is requested or the application is invalid, the bootloader waits for an update over UART.

Application validity includes:
- Metadata magic, version and commit marker checks.
- Application image size bounds.
- CRC32 comparison between metadata and Flash contents.
- Initial stack pointer within the STM32 SRAM range and aligned to 8 bytes.
- Reset vector with the Thumb bit set.
- Reset-handler address within the programmed image.

## 4. Update transaction

The update protocol uses BEGIN, DATA and END commands, with ABORT available to terminate an update session.

1. BEGIN supplies the image size and expected image CRC32.
2. The bootloader checks the requested size and erases application sectors.
3. DATA packets carry an image offset and a chunk of firmware bytes.
4. The bootloader checks packet CRC, sequence, offset and length before programming.
5. END confirms that the expected number of bytes was received.
6. The bootloader calculates CRC32 over the programmed image.
7. Metadata is written only after the image CRC matches.
8. The bootloader acknowledges completion.

The metadata commit marker is written last so that incomplete metadata is not treated as a committed image.

## 5. Application handoff

The bootloader reads the application's initial stack pointer and reset vector from the application vector table. Before branching, it prepares the processor state, updates the vector-table offset, disables/clears selected interrupt and peripheral state, and sets the application stack pointer.

The tested LED application does not depend on interrupts. Applications that use interrupts, timers, DMA or other peripherals should receive additional handoff testing before being considered supported.

## 6. Design boundaries

ForgeBoot v0.1 is a single-slot update system. The bootloader occupies sector 0, metadata occupies sector 1, and the application occupies sectors 2–7. An update erases the current application before writing the new one. This means the bootloader remains available for recovery, but the previous application is not preserved if an update is interrupted.

CRC32 is used for accidental corruption detection. It is not a cryptographic authenticity mechanism.
