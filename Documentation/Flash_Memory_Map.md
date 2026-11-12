# Flash Memory Map

## Device

Target MCU: STM32F411CEU6  
Internal Flash: 512 KiB  
SRAM: 128 KiB

## Flash allocation

| Sector | Start address | End address (inclusive) | Size | Allocation |
|---|---:|---:|---:|---|
| 0 | `0x08000000` | `0x08003FFF` | 16 KiB | Bootloader |
| 1 | `0x08004000` | `0x08007FFF` | 16 KiB | Metadata |
| 2 | `0x08008000` | `0x0800BFFF` | 16 KiB | Application |
| 3 | `0x0800C000` | `0x0800FFFF` | 16 KiB | Application |
| 4 | `0x08010000` | `0x0801FFFF` | 64 KiB | Application |
| 5 | `0x08020000` | `0x0803FFFF` | 128 KiB | Application |
| 6 | `0x08040000` | `0x0805FFFF` | 128 KiB | Application |
| 7 | `0x08060000` | `0x0807FFFF` | 128 KiB | Application |

The application region is `0x08008000` through (but not including) `0x08080000`, giving 480 KiB.

## SRAM

The SRAM region is:
- Start: `0x20000000`
- End (exclusive): `0x20020000`
- Size: 128 KiB

The initial stack pointer may be at the top of SRAM, `0x20020000`. The bootloader checks that the initial stack pointer is within the allowed SRAM boundary and is 8-byte aligned.

## Linker configuration

Expected memory origins and lengths:

| Project | Flash origin | Flash length | RAM origin | RAM length |
|---|---:|---:|---:|---:|
| Bootloader | `0x08000000` | 16 KiB | `0x20000000` | 128 KiB |
| Application | `0x08008000` | 480 KiB | `0x20000000` | 128 KiB |

The application vector table must therefore be placed at `0x08008000`.

## Update safety note

The firmware update code is intended to erase/program sectors 2–7, leaving sector 0 and sector 1 outside the application erase range. The source-level address guard also rejects writes below the metadata region. This software guard is not equivalent to hardware write protection configured through option bytes.
