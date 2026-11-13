# ForgeBoot v0.2 release notes

ForgeBoot v0.2 builds on the initial hardware-validated UART bootloader release with a more complete host workflow and improved application handoff/recovery behavior.

## Highlights

- **Desktop GUI:** Tkinter interface combines serial monitoring, firmware selection, upload progress and logs.
- **Recovery-mode preflight:** GUI sends PING and requires a matching ACK before beginning an upload. A normal application that does not respond as the bootloader is not sent a firmware BEGIN request.
- **Transfer cancellation:** ABORT returns an interrupted transfer to the idle update state so another upload can be attempted without a power cycle, where the bootloader remains responsive.
- **Automatic launch:** after successful image verification and metadata commit, the bootloader automatically transfers control to the application.
- **Application handoff:** vector table relocation and interrupt/peripheral cleanup support applications with their own interrupt configuration.
- **Independent application compatibility:** a separately developed macropad application was configured for the ForgeBoot application region, uploaded using the GUI, and launched successfully. Its source remains in its own repository.

## Custom application configuration

- Flash origin: `0x08008000`
- Flash length: `480K`
- Vector table offset: `0x00008000`
- Image format: raw `.bin`

## Target and interface

- STM32F411CEU6
- USART1: PA9 TX, PA10 RX
- UART: 115200 baud, 8-N-1
- Recovery button: PB12 active-low

## Known limitations

- Single application slot; no rollback to the previous application after erase begins.
- CRC32 only; no signed firmware/authentication.
- Target-specific memory map and startup/handoff implementation.
