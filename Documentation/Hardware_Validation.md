# ForgeBoot v0.1 — Physical Hardware Validation

## 1. Objective

Validate the ForgeBoot v0.1 update path on a physical STM32F411CEU6 Black Pill, including bootloader startup, UART communication, firmware transfer, application validation and application execution.

## 2. Hardware used

| Item | Purpose |
|---|---|
| STM32F411CEU6 Black Pill | Target microcontroller |
| ST-LINK/V2-compatible programmer/debugger | Initial programming and SWD debugging |
| CP2102 USB-to-UART adapter | Host-to-target UART communication |
| PC running Windows | Host system |
| Tera Term | UART terminal observation |
| Python 3 and pyserial | Host firmware uploader |

The UART adapter was connected using 3.3 V logic-level signals.

## 3. Connections

| CP2102 | Black Pill |
|---|---|
| TXD | PA10 / USART1_RX |
| RXD | PA9 / USART1_TX |
| GND | GND |

Recovery button: PB12 to GND, active-low.

The onboard LED test application used PC13. On the tested board, the LED was active-low.

## 4. Test procedure and observations

### Test 1 — SWD target connection
The ST-LINK-compatible debugger connected to the STM32F411 target over SWD. The debugger identified a Cortex-M4 device and reported 512 KiB of Flash.

### Test 2 — Bootloader presence
The Flash vector table was inspected at `0x08000000`. The initial stack pointer and reset vector were consistent with the bootloader image.

### Test 3 — Peripheral configuration
The bootloader's GPIOB configuration was inspected during debugging. PB12 was configured as an input with pull-up. USART1 registers showed the configured baud-rate and enable settings.

### Test 4 — UART boot message
The CP2102 adapter was connected to USART1 with TX/RX crossed. Tera Term was configured for 115200 baud, 8-N-1, no flow control. The bootloader startup and recovery messages were observed.

### Test 5 — Firmware transfer
The Python uploader transferred the application image over COM19. The uploader reported six packets, with ACK responses for sequences 0 through 5 and 100% transfer completion.

### Test 6 — Application validation
After resetting the target, the bootloader reported that the application was valid and that it was starting the application.

### Test 7 — Application execution
The application was configured to toggle PC13. The Black Pill onboard LED blinked after the bootloader handed over execution.

## 5. Result

**The v0.1 physical hardware update path was successfully demonstrated.**

The observed result establishes that the tested firmware image could be transferred over UART, acknowledged by the bootloader, programmed into Flash, validated on reboot and executed.

## 6. Scope and limitations of this validation

This validation does not establish:
- Power-failure safety during Flash erase/program operations.
- Recovery from every possible malformed or interrupted packet sequence.
- Support for every interrupt-driven or peripheral-intensive application.
- Cryptographic authenticity of firmware.
- Hardware write protection of the bootloader sector.
- Long-duration reliability or production readiness.

Those are candidates for subsequent development and testing.

## 7. Evidence to add before public release

If available, add your own photographs or screenshots to the repository:
- `Hardware/Photos/board_setup.jpg`
- `Hardware/Photos/uart_boot_message.png`
- `Hardware/Photos/uploader_success.png`
- `Hardware/Photos/application_led.jpg`

These are suggested filenames only. Add evidence only if you actually capture it. The v0.1 documentation does not depend on Renode screenshots or simulation evidence.
