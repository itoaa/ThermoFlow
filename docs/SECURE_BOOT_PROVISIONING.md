# ThermoFlow Secure Boot V2 Provisioning Guide

## Overview

This document describes the provisioning procedure for ThermoFlow ESP32-S3 devices with Secure Boot V2 and Flash Encryption enabled.

## Prerequisites

- ESP-IDF v5.0 or later
- Python 3.7+
- ESP32-S3 development board or module
- Access to secure key storage for backup

## Files

| File | In repository | Purpose |
|------|---------------|---------|
| `keys/secure_boot_signing_key.pem` | **No** (gitignored) | Private signing key (3072-bit RSA) — generate locally |
| `keys/secure_boot_signing_key_public.pem` | Yes | Public verification key |
| `keys/README.md` | Yes | Key generation instructions |

Generate keys locally:

```bash
openssl genrsa -out keys/secure_boot_signing_key.pem 3072
openssl rsa -in keys/secure_boot_signing_key.pem -pubout -out keys/secure_boot_signing_key_public.pem
```

## ⚠️ CRITICAL: Key Security

**The private signing key must never be committed to version control.**

- Keep it offline when possible
- Use hardware security module (HSM) for production
- Listed in `.gitignore` — verify with `git status` before every commit
- Make encrypted backups in multiple secure locations
- If a key was previously exposed: rotate immediately and purge git history

## Provisioning Steps

### 1. Initial Build

```bash
# Set target
idf.py set-target esp32s3

# Configure project
idf.py menuconfig

# Build the project
idf.py build
```

### 2. Flash Bootloader (One-Time)

The bootloader must be flashed separately because secure boot enables additional checks:

```bash
# Flash only the bootloader first
esptool.py --port /dev/ttyUSB0 --baud 921600 --before default_reset --after hard_reset write_flash --flash_mode dio --flash_freq 80m --flash_size 8MB 0x0 build/bootloader/bootloader.bin
```

### 3. Flash Partition Table and Application

```bash
# Flash partition table and application
idf.py flash
```

### 4. First Boot - Secure Boot Enablement

On first boot, the bootloader will:

1. Generate or use the flash encryption key
2. Encrypt the bootloader and application in-place
3. Burn the secure boot digest into eFuse
4. Enable secure boot protection

**DO NOT INTERRUPT POWER DURING THIS PROCESS.**

### 5. Verify Secure Boot

Check the serial output for:

```
I (28) boot: ESP-IDF v5.x 2nd stage bootloader
I (29) boot: compile time ...
I (168) boot: Checking flash encryption...
I (168) flash_encrypt: Generating new flash encryption key...
I (187) flash_encrypt: Read & write protecting new key...
I (187) flash_encrypt: Disable UART bootloader decryption...
I (201) flash_encrypt: Disable UART bootloader MMU cache...
I (208) flash_encrypt: Disable JTAG...
I (212) flash_encrypt: Flash encryption completed
I (219) secure_boot: Secure Boot V2 enabled
```

## Development Mode vs Release Mode

### Development Mode (Current Configuration)

- Allows reflashing with plaintext firmware
- UART bootloader remains accessible
- Useful for development and testing

### Release Mode (Production)

- Only OTA updates are possible
- UART bootloader disabled for flash operations
- Maximum security

**To switch to Release Mode:**

1. In `sdkconfig.defaults`, change:
   ```
   # CONFIG_SECURE_FLASH_ENCRYPTION_MODE_DEVELOPMENT is not set
   CONFIG_SECURE_FLASH_ENCRYPTION_MODE_RELEASE=y
   ```

2. In application code, call:
   ```c
   #include "esp_flash_encrypt.h"
   
   if (!esp_flash_encryption_cfg_verify_release_mode()) {
       esp_flash_encryption_set_release_mode();
   }
   ```

3. Rebuild and flash

## OTA Updates with Secure Boot

OTA updates must be signed with the same key:

```bash
# Sign the OTA binary
idf.py secure-sign-data --keyfile keys/secure_boot_signing_key.pem --output firmware-signed.bin firmware-unsigned.bin

# Or let the build system sign automatically
idf.py build
```

The signed binary can then be uploaded via OTA.

## Recovery Options

### If Flash Encryption Key is Lost

**⚠️ The device is permanently bricked.**

Always backup the flash encryption key if using host-generated keys:

```bash
# Read the key (before first boot or in development mode)
idf.py efuse-read-protect flash_encryption
```

### Reverting Flash Encryption (Development Mode Only)

```bash
# Disable flash encryption in sdkconfig, then:
idf.py efuse-burn FLASH_CRYPT_CNT
```

**Note:** Can only be done 3 times per device.

## Troubleshooting

### "flash read err, 1000"

- Flash encryption is enabled but plaintext data was flashed
- Use `idf.py encrypted-flash` for encrypted devices

### "Failed to verify bootloader signature"

- Wrong signing key
- Bootloader was modified after signing
- Rebuild and reflash the bootloader

### "Secure boot check failed"

- The bootloader signature doesn't match the eFuse digest
- Only recovery: Flash the correctly signed bootloader that matches the eFuse

## References

- [ESP32 Secure Boot V2 Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/security/secure-boot-v2.html)
- [ESP32 Flash Encryption Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/security/flash-encryption.html)
