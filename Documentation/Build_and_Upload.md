# Build and Upload Guide

## 1. Tools

- STM32CubeIDE
- Python 3
- `pyserial`
- USB-to-UART adapter with 3.3 V logic
- ST-LINK-compatible SWD programmer/debugger for initial programming and debugging

## 2. Build the bootloader

1. Open the bootloader project in STM32CubeIDE.
2. Confirm the target device is STM32F411CEU6.
3. Confirm the bootloader linker Flash origin is `0x08000000` and length is 16 KiB.
4. Build the project.
5. Enable binary output in the project build settings if a bootloader `.bin` is required.

The bootloader must fit within sector 0 (16 KiB). The recorded v0.1 bootloader binary was approximately 7 KiB, within this limit. Recheck the actual build output before programming.

## 3. Build the application

1. Open the application project.
2. Confirm the application linker Flash origin is `0x08008000`.
3. Confirm the application Flash length is 480 KiB.
4. Build the project.
5. Enable binary output (`.bin`).

The host uploader expects the raw application binary, not the ELF file.

## 4. Initial programming

Use the ST-LINK-compatible programmer/debugger to program the bootloader at its configured address, `0x08000000`.

Use care when selecting erase options. A full-chip erase will remove the bootloader and metadata as well as the application. For initial setup, program the bootloader and then reset the target.

## 5. UART wiring

| USB-UART adapter | STM32F411CEU6 |
|---|---|
| TXD | PA10 |
| RXD | PA9 |
| GND | GND |

Connect TX to RX and RX to TX. Ensure 3.3 V UART logic. Do not connect adapter VCC when the board is independently powered.

## 6. Terminal configuration

A serial terminal such as Tera Term can be used to observe bootloader messages.

Configure:
- Correct COM port
- 115200 baud
- 8 data bits
- No parity
- 1 stop bit
- No flow control

Close the terminal before running the Python uploader because the COM port generally cannot be opened by both programs simultaneously.

## 7. Enter recovery mode

The recovery input is PB12, active-low, with an internal pull-up.

For the tested setup:
1. Connect the recovery button between PB12 and GND.
2. Hold the button while resetting/powering the board so the bootloader samples the recovery request.
3. Release the button once the bootloader has entered recovery mode and is waiting for firmware.
4. Close the serial terminal before launching the uploader.

If no valid application exists, the bootloader also enters recovery mode.

## 8. Install host dependency

From the host uploader directory:

```bash
python -m pip install pyserial
```

## 9. Inspect firmware

```bash
python forgeboot_uploader.py path/to/application.bin
```

The inspection mode reports image information and performs basic vector-table checks.

## 10. Run host self-tests

```bash
python forgeboot_uploader.py --self-test
```

The host-side tests exercise protocol packet construction, CRC, ACK/NACK handling, sequence validation and retry behaviour using a mock serial interface. These tests do not replace physical hardware tests.

## 11. Upload firmware

Example for Windows:

```powershell
python forgeboot_uploader.py "D:\path\to\application.bin" --upload --port COM19
```

Replace the path and COM port with the actual values.

The uploader sends BEGIN, DATA and END packets using stop-and-wait transmission and waits for ACK responses.

## 12. Verify the result

After upload:
1. Wait for the uploader to report successful transfer completion.
2. Reset the target without holding the recovery button.
3. Observe the bootloader UART output.
4. For the v0.1 LED test application, confirm that the onboard PC13 LED blinks.

A successful host transfer alone does not prove that the application can execute. Confirm the bootloader's validation message and the application's visible behaviour.
