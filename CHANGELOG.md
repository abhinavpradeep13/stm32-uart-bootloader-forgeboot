# Changelog

## [0.2] - 2026-11-13

### Added
- Python Tkinter desktop GUI combining serial monitoring and firmware upload.
- Recovery-mode preflight using the PING command; GUI blocks upload unless a matching ACK is received.
- ABORT-based transfer cancellation/recovery and retry-aware host transfer handling.
- Automatic application launch after successful firmware verification and metadata commit.
- Independently developed application compatibility demonstration using a separate macropad firmware project.

### Improved
- Application handoff and interrupt/vector-table handling, including application VTOR relocation to `0x08008000`.
- Host uploader/backend organization while retaining the command-line uploader.
- Release documentation and host setup instructions.

### Validation
- User-reported hardware validation on STM32F411CEU6: application handoff, SysTick-interrupt application, firmware upload, automatic launch, transfer abort/retry, GUI monitoring, recovery-mode preflight, and independent macropad application upload/launch.

## [0.1]
- Initial hardware-validated UART bootloader release.
