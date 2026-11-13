# ForgeBoot v0.1 — Initial Hardware-Validated Release

## Summary

ForgeBoot v0.1 is the first working release of a custom UART firmware bootloader for the STM32F411CEU6 Black Pill.

The release includes the STM32 bootloader, a separately linked application project, and a Python command-line uploader.

## Included functionality

- USART1 UART communication at 115200 baud, 8-N-1.
- PB12 active-low recovery input.
- Framed update protocol with sequence numbers and ACK/NACK responses.
- Packet CRC32 checks.
- Application Flash erase and word programming.
- Firmware image CRC32 verification.
- Metadata storage in Flash sector 1.
- Application vector-table sanity checks.
- Python uploader with serial upload, retry handling, image inspection and self-tests.

## Physical validation

The release was tested on a physical STM32F411CEU6 Black Pill.

The host uploader transferred the application over a CP2102 UART adapter and received ACKs for all six packets. After reset, the bootloader reported the application as valid and started it. The uploaded PC13 LED test application blinked the onboard LED.

## Known limitations

- Single application slot.
- No automatic rollback.
- CRC32 is not cryptographic authentication.
- No cryptographic signature verification.
- No formal power-interruption testing.
- The tested application handoff was a simple GPIO application.

## Release identity

- Version: `v0.1`
- Release type: Initial hardware-validated baseline
- Target: STM32F411CEU6
- UART: USART1, 115200 baud

Before publishing a binary asset, record its exact filename, size and SHA-256 hash. Do not publish guessed hashes or substitute a binary from a different build.
