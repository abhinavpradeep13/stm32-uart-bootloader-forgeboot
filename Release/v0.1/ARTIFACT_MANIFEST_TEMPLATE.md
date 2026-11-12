# v0.1 Artifact Manifest

Fill in this manifest from the exact binaries you intend to attach to the GitHub release.

| Artifact | Filename | Size (bytes) | SHA-256 |
|---|---|---:|---|
| Bootloader binary | _fill in_ | _fill in_ | _fill in_ |
| Application test binary | _fill in_ | _fill in_ | _fill in_ |
| Host uploader | `forgeboot_uploader.py` | _fill in_ | _fill in_ |

Example command in PowerShell to calculate SHA-256:

```powershell
Get-FileHash .\firmware.bin -Algorithm SHA256
```

Do not confuse SHA-256 artifact hashes with the firmware CRC32 used by the bootloader protocol. They serve different purposes.
