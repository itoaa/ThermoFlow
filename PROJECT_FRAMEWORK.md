# ThermoFlow Project Framework

## Security & Development Governance Framework

**Version:** 1.0.0  
**Date:** 2026-03-22  
**Classification:** Internal  
**Framework Owner:** Ola Andersson  
**Security Compliance:** IEC 62443 SL-2, CERT C  

---

## 0. PROJECT OVERVIEW

### 0.1 Project Identity

| Attribute | Value |
|-----------|-------|
| **Name** | ThermoFlow |
| **Version** | 1.0.0 |
| **Purpose** | ESP32-based climate monitoring and control system for mobile AC units and DIY heat exchangers with humidity management and fan control |
| **Application Domain** | Home automation, HVAC monitoring, energy efficiency |
| **Criticality Level** | IEC 62443 Security Level 2 (SL-2) |
| **Target Platform** | ESP32-S3 (WiFi + Bluetooth) |

### 0.2 Use Cases

**Primary Use Case 1: Mobile AC Monitoring**
- Monitor cold air output temperature and humidity
- Monitor hot exhaust air temperature and humidity
- Track AC efficiency and performance
- Alert on abnormal conditions

**Primary Use Case 2: DIY Heat Exchanger Control**
- Monitor intake and exhaust air (temperature + humidity)
- Control 1-2 fans based on conditions
- Prevent condensation (anti-kondens protection at >90% RH)
- Optimize energy efficiency

### 0.3 Safety & Security Context

**Intended Use:**
- Indoor climate monitoring and basic fan control
- Home automation integration via MQTT
- Local web interface for configuration and monitoring
- Over-the-air (OTA) firmware updates with signature verification

**Foreseen Misuse:**
- Outdoor installation without weather protection
- Connection to unencrypted MQTT brokers
- OTA updates from untrusted sources
- Physical tampering with device

**Risk Assessment:**
| Hazard | Risk | Mitigation |
|--------|------|------------|
| Unauthorized device access | High | TLS, certificate pinning, secure boot |
| Manipulation of fan control | Medium | Authentication, rate limiting, fail-safe |
| Condensation damage | High | RH monitoring, automatic fan control |
| OTA compromise | High | Signed updates, rollback protection |
| Network eavesdropping | Medium | MQTT-TLS, WPA3 |

### 0.4 Technical Specifications

**Hardware:**
- **MCU:** ESP32-S3 (Dual-core 240MHz, WiFi + BLE)
- **Sensors:** 2-4x Sensirion SHT40 (I2C, ±0.2°C, ±1.8% RH)
- **Display:** OLED 0.96" I2C (optional but recommended)
- **Actuators:** 2x PWM fan control (3-pin or 4-pin)
- **Storage:** MicroSD for local logging fallback
- **Power:** 5V USB or 5V/2A adapter

**Communication:**
- **WiFi:** WPA2/WPA3, MQTT-TLS to Home Assistant
- **Web Interface:** HTTPS (self-signed or Let's Encrypt)
- **OTA:** Signed firmware updates (Ed25519)

**Software Stack:**
- **Framework:** ESP-IDF v5.1+ (FreeRTOS)
- **Languages:** C/C++17
- **Libraries:** ESP-MQTT, mbedTLS, SHT4x driver

---

## 1. EXECUTIVE SUMMARY

ThermoFlow is an ESP32-based climate monitoring and control system designed for home HVAC applications. It monitors temperature and humidity for both mobile AC units and DIY heat exchangers, with optional fan control for the latter.

**Key Principles:**
- Security by Design (IEC 62443 SL-2)
- Defense in Depth (network, device, application layers)
- Fail-Safe Operation (fans OFF on system errors)
- Privacy by Design (local processing, encrypted communication)
- Maintainability (OTA updates, remote diagnostics)

---

## 2. PROJECT STRUCTURE

### 2.1 Directory Hierarchy

```
ThermoFlow/
├── src/                       # Application source code
│   ├── main/                  # Main application
│   ├── sensors/               # SHT40 sensor drivers
│   ├── control/               # Fan control logic
│   ├── network/               # WiFi, MQTT, Web server
│   ├── security/              # TLS, OTA signature, auth
│   └── display/               # OLED display driver
├── include/                   # Public headers
├── tests/                     # Unit tests (Unity)
├── docs/                      # Documentation
│   ├── architecture/          # System design
│   ├── api/                   # API reference
│   └── security/              # Threat model, security docs
├── config/                    # Build configurations
├── ci/                        # CI/CD scripts
├── tools/                     # Build tools
├── .clang-format              # Code formatting
├── .editorconfig              # Editor configuration
├── CHANGELOG.md               # Version history
├── LICENSE                    # License
└── PROJECT_FRAMEWORK.md       # This document
```

---

## 3. SECURITY REQUIREMENTS (MANDATORY)

### SR-001: Input Validation
- **Requirement:** All external inputs (MQTT, HTTP, sensors) MUST be validated
- **Scope:** Sensor readings (range check), MQTT messages (schema validation), HTTP requests (sanitize)
- **Failure:** Reject input, log security event, use last known good value
- **Verification:** Fuzz testing, boundary testing

### SR-002: Authentication & Authorization
- **Requirement:** All control operations require authentication
- **Scope:** Web interface (session-based), MQTT (certificate or username/password), OTA (signature verification)
- **Implementation:**
  - Web: Session tokens with timeout (30 min idle)
  - MQTT: Mutual TLS (mTLS) or strong passwords
  - Admin functions: Separate auth from monitoring
- **Failure:** 403 Forbidden, log attempt

### SR-003: Secure Communication
- **Requirement:** All network communication MUST use TLS 1.3
- **Scope:** MQTT (port 8883), Web server (port 443), OTA (HTTPS)
- **Implementation:**
  - mbedTLS with certificate pinning
  - No fallback to unencrypted
  - Certificate validation (not bypassable)
- **Failure:** Connection refused

### SR-004: Fail-Safe Defaults
- **Requirement:** System defaults to SAFE state on any error
- **Implementation:**
  - Fans: OFF on boot until explicitly enabled
  - Fans: OFF on communication loss (>5 min)
  - Fans: OFF on sensor failure
  - Safe RH threshold: <90% for fan operation
- **Verification:** Fault injection testing

### SR-005: Audit Logging
- **Requirement:** Security-relevant events logged persistently
- **Events:** Authentication attempts, configuration changes, fan state changes, OTA updates, errors
- **Retention:** 30 days on SD card, 7 days in flash
- **Protection:** Append-only, checksum verification

### SR-006: Resource Limits
- **Requirement:** Protection against resource exhaustion
- **Implementation:**
  - Rate limiting: Max 10 web requests/sec
  - MQTT: Max 100 msg/sec
  - Memory: Watchdog on heap exhaustion
  - Network: Connection timeout 30s

### SR-007: Error Handling
- **Requirement:** No sensitive information in error messages
- **Implementation:**
  - User-facing: Generic messages ("Authentication failed")
  - Logs: Detailed for debugging
  - Stack traces: Never exposed externally

### SR-008: Dependency Management
- **Requirement:** All dependencies tracked and vetted
- **Format:** Software Bill of Materials (SBOM)
- **Updates:** Security patches within 30 days
- **Scanning:** Weekly CVE checks

### SR-009: Actuator Fail-Safe (NEW - for fan control)
- **Requirement:** Fans MUST default to OFF on any system fault
- **Implementation:**
  - Hardware watchdog resets → fans OFF
  - Software watchdog timeout → fans OFF
  - Sensor communication failure → fans OFF
  - Network loss >5 minutes → fans OFF
- **Verification:** Hardware-in-loop testing

### SR-010: Environmental Limits (Anti-Kondens) (NEW)
- **Requirement:** Fan operation prohibited at high humidity
- **Threshold:** >90% RH → automatic fan shutdown + alert
- **Hysteresis:** Fans resume at <85% RH
- **Alert:** MQTT notification to Home Assistant

### SR-011: OTA Security (NEW)
- **Requirement:** Firmware updates MUST be cryptographically signed
- **Signature:** Ed25519
- **Rollback:** Prevention of downgrade attacks
- **Verification:** Signature check before flash write
- **Backup:** Keep last known good firmware

---

## 4. CHANGE MANAGEMENT

### 4.1 Change Classification

| Class | Description | Approval | Documentation |
|-------|-------------|----------|---------------|
| Critical | Security/fan control changes | Security Officer | Full + Security Review |
| Major | New features, API changes | Tech Lead | Architecture + API docs |
| Minor | Bug fixes, optimizations | Peer Review | CHANGELOG |
| Patch | Typos, comments | Self | Commit message |

### 4.2 Commit Message Format

```
type(scope): subject

body (what and why)

Breaking Changes: (if any)
Security Impact: [None/Low/Medium/High/Critical]
Safety Impact: [None/Low/Medium/High] (for fan control)
Documentation: (what was updated)

Refs: #123
```

**Types:** feat, fix, docs, security, refactor, test, chore, ci
**Scopes:** sensors, control, network, security, display, config

---

## 5. CODE QUALITY STANDARDS

### 5.1 Standards Stack

| Standard | Application | Tool |
|----------|-------------|------|
| CERT C | Secure coding | esp-idf static analysis |
| CWE Top 25 | Vulnerability prevention | Manual review |
| OWASP IoT | IoT security | Security audit |
| ESP-IDF Style | Code style | `idf.py clang-check` |

### 5.2 Pre-Commit Security Checklist

```
□ Input validation present for all external data
□ No hardcoded secrets (keys, passwords)
□ Buffer bounds checked (string ops, memcpy)
□ Integer overflow checked (arithmetic)
□ Resource cleanup on all paths (free, close)
□ Error messages generic (no info leakage)
□ Dependencies scanned for CVEs
□ Fan control has fail-safe default
```

---

## 6. DOCUMENTATION

### Required Documents

| Document | Purpose |
|----------|---------|
| PROJECT_FRAMEWORK.md | This document |
| CHANGELOG.md | Version history |
| README.md | Setup and usage |
| docs/architecture/system.md | System design |
| docs/security/threat-model.md | STRIDE analysis |
| docs/api/mqtt.md | MQTT API spec |
| docs/api/rest.md | REST API spec (if applicable) |

---

## 7. APPENDICES

### Appendix A: Threat Model
See `docs/security/threat-model.md`

### Appendix B: SBOM Template

| Component | Version | License | CVE Check |
|-----------|---------|---------|-----------|
| ESP-IDF | 5.1.x | Apache 2.0 | Weekly |
| mbedTLS | 2.28.x | Apache 2.0 | Weekly |
| SHT4x driver | Latest | BSD/MIT | Weekly |

### Appendix C: Approved Tools

| Tool | Version | Purpose |
|------|---------|---------|
| ESP-IDF | 5.1.2 | SDK |
| Python | 3.11+ | Build scripts |
| OpenSSL | 3.x | Certificate gen |
| Unity | 2.5+ | Unit tests |

---

**Next Review:** 2026-06-22  
**Project Owner:** Ola Andersson
