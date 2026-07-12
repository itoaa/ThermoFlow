# Signing Keys

Private signing keys are **not** stored in this repository.

## Generate a new Secure Boot signing key

```bash
# From project root (requires OpenSSL)
openssl genrsa -out keys/secure_boot_signing_key.pem 3072
openssl rsa -in keys/secure_boot_signing_key.pem -pubout -out keys/secure_boot_signing_key_public.pem
```

The private key file is listed in `.gitignore`. Store it offline or in an HSM for production.

## Public key

`secure_boot_signing_key_public.pem` may be committed — it is required for verification only.

## Key rotation

If a private key was ever exposed in git history:

1. Generate a new key pair (commands above)
2. Re-sign all firmware with the new key
3. Rewrite git history to purge the old private key (`git filter-repo` or BFG)

See [docs/SECURE_BOOT_PROVISIONING.md](../docs/SECURE_BOOT_PROVISIONING.md) and [docs/KEY_BACKUP_RECOVERY.md](../docs/KEY_BACKUP_RECOVERY.md).