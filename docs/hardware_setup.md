# Hardware setup

## Target

- MCU: STM32F411CEU6 Black Pill
- UART: USART1, 115200 baud, 8 data bits, no parity, 1 stop bit
- Recovery input: PB12, active-low, internal pull-up

## Required hardware

- STM32F411CEU6 Black Pill target board
- ST-LINK V2 or compatible SWD programmer for initial bootloader programming/debugging
- CP2102 or another 3.3 V USB-to-UART adapter for routine PC communication
- USB cable and suitable board power supply
- Momentary pushbutton for PB12 recovery selection
- Jumper wires and a PC running Python 3.8 or newer

The ST-LINK is used to program the bootloader initially. Normal firmware uploads use UART and do not require the ST-LINK to remain connected.

## Connections

| STM32 | Connect to |
|---|---|
| PA9 (USART1_TX) | USB-UART RX |
| PA10 (USART1_RX) | USB-UART TX |
| GND | USB-UART GND |
| PB12 | Momentary pushbutton to GND |

Use 3.3 V UART logic. Cross TX/RX and share ground. Do not connect 5 V UART signals to the STM32 pins. If the board is powered separately, leave USB-UART VCC disconnected.

## Entering recovery mode

Hold PB12 low while resetting or powering up the board. Release after the bootloader starts. The serial output should include `Recovery Mode Active` and `Waiting for firmware update...`.

## Flash map

- `0x08000000–0x08003FFF`: bootloader, sector 0
- `0x08004000–0x08007FFF`: metadata, sector 1
- `0x08008000–0x0807FFFF`: application, sectors 2–7

Program the bootloader at `0x08000000` using SWD/ST-LINK. Application images are installed by the UART updater at `0x08008000`.
