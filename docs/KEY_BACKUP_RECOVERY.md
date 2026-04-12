# ThermoFlow Secure Boot Key Backup and Recovery Procedures

## Critical Assets

The following assets must be backed up and protected:

1. **Secure Boot Signing Key** (`keys/secure_boot_signing_key.pem`)
2. **Public Key** (`keys/secure_boot_signing_key_public.pem`)
3. **Flash Encryption Key** (if using host-generated keys)

## Backup Procedures

### 1. Local Encrypted Backup

```bash
# Create encrypted archive of keys
cd /home/ola/.openclaw/workspace/ThermoFlow

# Create a strong password-protected archive
gpg --symmetric --cipher-algo AES256 --compress-algo 1 --s2k-cipher-algo AES256 --s2k-digest-algo SHA512 --s2k-count 65011712 --output keys/secure_boot_keys_backup.gpg keys/secure_boot_signing_key.pem keys/secure_boot_signing_key_public.pem

# Move to secure storage
cp keys/secure_boot_keys_backup.gpg /secure/backup/location/
```

### 2. Shamir's Secret Sharing (Recommended for Production)

Split the private key into multiple shares that require a threshold to reconstruct:

```bash
# Install ssss (Shamir's Secret Sharing)
# On Ubuntu/Debian: sudo apt-get install ssss

# Split the private key into 5 shares, requiring 3 to reconstruct
ssss-split -t 3 -n 5 -w "Secure Boot Key" < keys/secure_boot_signing_key.pem

# Distribute shares to different secure locations
# Store 3 shares: CEO, CFO, Security Officer
# Distribute to different physical locations
```

### 3. Hardware Security Module (HSM) Storage

For production environments, store keys in HSM:

```bash
# Import key to PKCS#11 HSM (example with SoftHSM)
pkcs11-tool --module /usr/lib/softhsm/libsofthsm2.so --login --write-object keys/secure_boot_signing_key.pem --type privkey --label "ThermoFlow Secure Boot"
```

## Recovery Procedures

### Scenario 1: Key File Corruption

1. Restore from most recent backup:
   ```bash
   gpg --decrypt keys/secure_boot_keys_backup.gpg > keys/secure_boot_signing_key.pem
   ```

2. Verify key integrity:
   ```bash
   openssl rsa -in keys/secure_boot_signing_key.pem -check -noout
   ```

3. Regenerate public key:
   ```bash
   openssl rsa -in keys/secure_boot_signing_key.pem -pubout -out keys/secure_boot_signing_key_public.pem
   ```

### Scenario 2: Lost Key (Shamir Reconstruction)

1. Collect minimum required shares (e.g., 3 of 5)
2. Reconstruct the key:
   ```bash
   # Each shareholder enters their share
   ssss-combine -t 3 > keys/secure_boot_signing_key_recovered.pem
   ```

### Scenario 3: Complete Key Loss

**⚠️ CRITICAL: If the secure boot signing key is lost, no new firmware can be installed on existing devices.**

Options:

1. **If devices are in Development Mode:**
   - Re-flash with new key (requires physical access)
   - Update eFuse with new key digest

2. **If devices are in Release Mode:**
   - Devices are effectively bricked for updates
   - Physical replacement required

## Key Rotation Strategy

### For Development Devices

1. Generate new signing key pair
2. Update eFuse with new key digest
3. Sign all future firmware with new key
4. Keep old key for existing devices

### For Production Devices

Production key rotation requires:

1. OTA update signed with old key
2. New public key digest burned to eFuse
3. All future updates signed with new key
4. Old key marked as revoked in eFuse

```c
// In application code for key rotation
#include "esp_secure_boot.h"

// Add new key digest (up to 3 keys supported in ESP32-S3)
esp_err_t add_new_key_digest(const uint8_t *new_key_digest) {
    return esp_secure_boot_write_key_digest(new_key_digest);
}

// Revoke old key
esp_err_t revoke_old_key(uint8_t key_index) {
    return esp_secure_boot_revoke_key(key_index);
}
```

## Security Checklist

- [ ] Private key never committed to version control
- [ ] Private key encrypted at rest
- [ ] Multiple encrypted backups in different physical locations
- [ ] Access to backups restricted to authorized personnel
- [ ] Shamir secret sharing implemented for production
- [ ] HSM used for production key storage
- [ ] Key rotation procedure documented and tested
- [ ] Disaster recovery plan tested annually

## Contact Information

**Security Officer:** Ola Andersson
**Backup Locations:** [REDACTED - Physical locations only]
**Emergency Contact:** [REDACTED]

## Audit Log

| Date | Action | Performed By | Notes |
|------|--------|--------------|-------|
| 2026-04-12 | Key generation | coding-agent | Initial secure boot setup |
| | | | |
| | | | |

---

**Document Version:** 1.0  
**Classification:** CONFIDENTIAL  
**Last Updated:** 2026-04-12
