#!/bin/bash
#
# ThermoFlow Remote Signing Script
# For SEC-024: Secure Boot V2 Implementation
#
# This script demonstrates how to sign firmware images remotely
# without exposing the private key to the build system.
#
# Usage: ./scripts/sign_firmware.sh [input.bin] [output.bin]
#

set -e

# Configuration
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# FÖRBÄTTRING: Nyckel lagras nu i projektkatalogen vilket är osäkert.
# TODO: Flytta nyckeln till HSM (t.ex. YubiHSM 2, NitroKey HSM) eller 
# använd en dedikerad signing-server som aldrig exponerar privat nyckel.
# Se: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/security/secure-boot-v2.html#key-management
KEY_FILE="${PROJECT_ROOT}/keys/secure_boot_signing_key.pem"
PUBLIC_KEY_FILE="${PROJECT_ROOT}/keys/secure_boot_signing_key_public.pem"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Logging functions
log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if IDF tools are available
check_idf() {
    if ! command -v idf.py &> /dev/null; then
        log_error "idf.py not found. Please source ESP-IDF environment."
        exit 1
    fi
    log_info "ESP-IDF environment found"
}

# Verify key exists
check_key() {
    if [ ! -f "$KEY_FILE" ]; then
        log_error "Signing key not found: $KEY_FILE"
        log_error "Generate a key first with: openssl genrsa -out $KEY_FILE 3072"
        exit 1
    fi

    # FÖRBÄTTRING: Kontrollera att nyckeln inte är incheckad i git
    # TODO: Lägg till .gitignore för keys/ och verifiera att filen inte spåras
    if git rev-parse --git-dir > /dev/null 2>&1; then
        if git ls-files --error-unmatch "$KEY_FILE" > /dev/null 2>&1; then
            log_error "CRITICAL: Signing key is tracked by git! Remove immediately:"
            log_error "  git rm --cached $KEY_FILE"
            log_error "  echo 'keys/*.pem' >> .gitignore"
            exit 1
        fi
    fi

    # Check key permissions
    local perms=$(stat -c "%a" "$KEY_FILE")
    if [ "$perms" != "600" ] && [ "$perms" != "400" ]; then
        log_warn "Signing key has permissions $perms, should be 600 or 400"
        chmod 600 "$KEY_FILE"
        log_info "Fixed key permissions to 600"
    fi

    log_info "Signing key verified: $KEY_FILE"
}

# Sign firmware binary
sign_binary() {
    local input_file="$1"
    local output_file="$2"

    if [ ! -f "$input_file" ]; then
        log_error "Input file not found: $input_file"
        exit 1
    fi

    # FÖRBÄTTRING: Remote signing borde använda HSM eller signing-server
    # TODO: Implementera stöd för PKCS#11 HSM eller AWS KMS/Azure Key Vault
    # Exempel med PKCS#11:
    #   openssl dgst -sha256 -sign "pkcs11:..." -out "${output_file}.sig" "$input_file"
    
    log_info "Signing binary: $input_file"
    log_info "Output file: $output_file"

    # Use idf.py to sign the binary
    idf.py secure-sign-data \
        --keyfile "$KEY_FILE" \
        --output "$output_file" \
        "$input_file"

    if [ $? -eq 0 ]; then
        log_info "Signing successful: $output_file"
    else
        log_error "Signing failed"
        exit 1
    fi
}

# Verify signature
verify_signature() {
    local signed_file="$1"

    if [ ! -f "$signed_file" ]; then
        log_error "File not found: $signed_file"
        exit 1
    fi

    log_info "Verifying signature for: $signed_file"

    # Extract and verify the signature
    idf.py secure-verify-signature \
        --keyfile "$PUBLIC_KEY_FILE" \
        "$signed_file"

    if [ $? -eq 0 ]; then
        log_info "Signature verification: PASSED"
    else
        log_error "Signature verification: FAILED"
        exit 1
    fi
}

# Show signature information
show_signature_info() {
    local signed_file="$1"

    log_info "Signature information for: $signed_file"
    
    # Use espsecure to get detailed info
    if command -v espsecure.py &> /dev/null; then
        espsecure.py verify_flash_signature \
            --version 2 \
            "$signed_file"
    else
        log_warn "espsecure.py not available, cannot show detailed info"
    fi
}

# Generate keys (one-time setup)
generate_keys() {
    log_info "Generating Secure Boot V2 signing keys..."

    mkdir -p "$(dirname "$KEY_FILE")"

    # FÖRBÄTTRING: Nyckelgenerering bör ske offline på air-gapped system
    # TODO: Generera nyckeln på dedikerad offline-maskin och överför endast
    # publik nyckel till byggsystemet. Använd Shamir's Secret Sharing för backup.
    # 
    # Rekommenderad procedur:
    # 1. Generera på offline-maskin utan nätverk
    # 2. Säkerhetskopiera med Shamir's Secret Sharing (M-of-N)
    # 3. Lagring: Hardware Security Module (HSM) eller offline kassaskåp
    # 4. Förstör nyckel på genereringsmaskinen efter backup
    
    openssl genrsa -out "$KEY_FILE" 3072
    chmod 600 "$KEY_FILE"

    # Extract public key
    openssl rsa -in "$KEY_FILE" -pubout -out "$PUBLIC_KEY_FILE"
    chmod 644 "$PUBLIC_KEY_FILE"

    log_info "Keys generated successfully"
    log_warn "CRITICAL: Store private key securely (HSM recommended)"
    log_warn "Never commit private key to version control!"
    log_info "Public key: $PUBLIC_KEY_FILE"
}

# Backup key with encryption
backup_key() {
    local backup_dir="${PROJECT_ROOT}/backups/keys"
    local timestamp=$(date +%Y%m%d_%H%M%S)
    local backup_file="${backup_dir}/secure_boot_key_${timestamp}.enc"
    
    mkdir -p "$backup_dir"
    
    # FÖRBÄTTRING: Använd Shamir's Secret Sharing istället för enkel kryptering
    # TODO: Implementera ssss-split för M-of-N key sharing
    #   ssss-split -t 3 -n 5 -w <key_file>  # 3-of-5 threshold
    # Lagras på olika fysiska platser med nyckelbevarare
    
    log_info "Creating encrypted backup..."
    gpg --symmetric --cipher-algo AES256 --output "$backup_file" "$KEY_FILE"
    chmod 400 "$backup_file"
    
    log_info "Backup created: $backup_file"
    log_warn "Store backup in secure offline location (e.g., bank vault)"
}

# Print usage
usage() {
    echo "ThermoFlow Secure Boot Signing Tool"
    echo ""
    echo "Usage:"
    echo "  $0 sign <input.bin> <output.bin>   Sign a binary"
    echo "  $0 verify <signed.bin>            Verify a signed binary"
    echo "  $0 info <signed.bin>              Show signature info"
    echo "  $0 generate                       Generate new signing keys"
    echo "  $0 backup                         Create encrypted backup of key"
    echo ""
    echo "SECURITY WARNINGS:"
    echo "  - Private key must never be committed to version control"
    echo "  - Use HSM (YubiHSM, NitroKey) for production"
    echo "  - Implement Shamir's Secret Sharing for backup"
    echo ""
    echo "Examples:"
    echo "  $0 sign build/thermoflow.bin build/thermoflow-signed.bin"
    echo "  $0 verify build/thermoflow-signed.bin"
    echo "  $0 generate"
}

# Main function
main() {
    case "${1:-}" in
        sign)
            if [ $# -ne 3 ]; then
                echo "Usage: $0 sign <input.bin> <output.bin>"
                exit 1
            fi
            check_idf
            check_key
            sign_binary "$2" "$3"
            verify_signature "$3"
            ;;
        verify)
            if [ $# -ne 2 ]; then
                echo "Usage: $0 verify <signed.bin>"
                exit 1
            fi
            check_idf
            check_key
            verify_signature "$2"
            ;;
        info)
            if [ $# -ne 2 ]; then
                echo "Usage: $0 info <signed.bin>"
                exit 1
            fi
            check_idf
            show_signature_info "$2"
            ;;
        generate)
            generate_keys
            ;;
        backup)
            backup_key
            ;;
        *)
            usage
            exit 1
            ;;
    esac
}

main "$@"
