# Changelog

All notable changes to ForgeBoot are documented here.

## [v0.1] - Initial hardware-validated release

### Added
- STM32F411CEU6 UART bootloader.
- USART1 communication at 115200 baud, 8-N-1.
- PB12 active-low recovery-button input.
- Internal Flash erase and word-programming routines.
- Dedicated metadata sector.
- Firmware metadata containing image size and CRC32.
- Commit marker written after metadata fields.
- CRC32 implementation compatible with Python `zlib.crc32`.
- Framed UART protocol with sequence numbers, ACK/NACK responses and packet CRC.
- Firmware update manager with BEGIN, DATA, END and ABORT handling.
- Application image validation using metadata, CRC32, stack pointer and reset vector checks.
- Python command-line firmware uploader.
- Host-side uploader self-tests and serial mock.
- Physical hardware test on STM32F411CEU6 Black Pill.

### Verified on hardware
- UART bootloader startup observed using Tera Term.
- Host uploader transferred firmware and received ACKs for all packets.
- Bootloader reported the stored application as valid after reset.
- Bootloader transferred execution to the uploaded application.
- Uploaded PC13 LED test application blinked the Black Pill onboard LED.

### Known limitations
- Single application slot; interrupted updates may require a new firmware upload.
- CRC32 provides error detection, not firmware authentication.
- No automatic rollback or cryptographic signature verification.
- Clock configuration assumptions are based on the default 16 MHz HSI setup.
- Hardware Flash write protection of the bootloader sector is not part of this release.

[ v0.1 ] represents the first physically validated baseline. Later releases should preserve this tag.
