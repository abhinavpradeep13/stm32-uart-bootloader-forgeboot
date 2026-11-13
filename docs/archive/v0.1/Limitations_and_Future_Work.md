# Limitations and Future Work

## Current v0.1 limitations

### 1. Single application slot
The current memory layout reserves one application region in sectors 2–7. The update process erases that region before programming the new image. If power is lost during an update, the previous application may no longer be available. The bootloader should remain available to accept another firmware upload, assuming the bootloader and metadata sectors remain intact.

### 2. CRC32 is not authentication
CRC32 detects accidental corruption. An attacker who can replace the firmware can also calculate a matching CRC. Firmware authenticity would require a cryptographic signature scheme and a trusted public key in the bootloader.

### 3. No automatic rollback
There is no backup application slot or external staging image. A failed update does not automatically restore the previous application.

### 4. Flash protection
The software address checks prevent normal update routines from targeting the bootloader sector. This is not the same as configuring STM32 Flash write protection through option bytes.

### 5. Clock assumptions
The current UART baud-rate and DWT timeout implementation assume a 16 MHz clock. A later clock-tree change must update and retest these calculations.

### 6. Application handoff scope
The demonstrated application is a simple GPIO LED blink program. Applications using interrupts, SysTick, timers, DMA or complex peripheral state need dedicated handoff testing.

### 7. Protocol hardening
The protocol works for the tested host uploader path. More robust handling of malformed lengths, partial frames, parser resynchronisation and error reporting should be added.

## Planned improvements

### v0.2 — Reliability and usability
- Audit Flash timeout and cleanup paths.
- Verify metadata by reading it back after programming.
- Improve malformed-packet recovery.
- Add more explicit session and error handling.
- Test application handoff with interrupt-driven applications.
- Build a desktop GUI for the Python uploader while retaining CLI support.
- Add repeatable hardware regression tests.

### Later versions
- Firmware version and hardware compatibility fields.
- Device information query and uploader diagnostics.
- External Flash staging or another carefully designed update architecture.
- Optional cryptographic firmware signature verification.
- Carefully evaluated hardware write protection.
