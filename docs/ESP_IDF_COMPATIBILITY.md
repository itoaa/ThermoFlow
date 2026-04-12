# ThermoFlow ESP-IDF v5.1.2 Compatibility Fixes - Status

**Senast uppdaterad:** 2026-04-12 22:50  
**Agent:** main (Ola)

## 🔴 Kritiskt Problem: ESP-IDF Header Inkonsistens

### Problembeskrivning
`esp_https_server.h` rad 101 använder `esp_tls_handshake_callback` som är definierad i `esp_tls.h` rad 223. Men när headern kompileras verkar typen inte vara tillgänglig, vilket tyder på ett inkonsistent API i ESP-IDF v5.1.2.

### Försökta Lösningar
1. ✅ Ändra inkluderingsordning - `esp_tls.h` före `esp_https_server.h`
2. ❌ Forward typedef av `mbedtls_ssl_hs_cb_t` - lyckades inte
3. ❌ Inkludera `mbedtls/ssl.h` direkt - `mbedtls_ssl_hs_cb_t` okänd

### Alternativa Lösningar

#### A. Kommentera bort cert_select_cb i config
HTTPD_SSL_CONFIG_DEFAULT() makrot kan behöva modifieras eller anropas utan cert_select_cb.

#### B. Använd äldre API
Använd `user_cb` istället för `cert_select_cb` om möjligt.

#### C. ESP-IDF Version Check
Kontrollera om detta är ett känt problem i ESP-IDF v5.1.2.

## ✅ Övriga Fixar Klara

| Fil | Status |
|-----|--------|
| cert_manager.c | ✅ Klar |
| web_server.c (övrigt) | ✅ Klar |
| security_manager.c | 🟡 Pågående (enkla fixar) |
| wifi_secure_storage.c | 🟡 Pågående (konstanter) |

## Rekommendation

web_server.c är blockerad av ett ESP-IDF API-problem som kan kräva:
1. **ESD-IDF patch** - Modifiera esp_https_server.h (ej rekommenderat)
2. **Använd HTTP istället för HTTPS** - Temporär lösning
3. **Uppgradera/downgradera ESP-IDF** - Kontrollera om problemet finns i andra versioner

## Tid Spenderad
- ~50 minuter

**Förslag:** Vill du att jag:
1. **Försöker mer** (kan ta ytterligare tid, osäker utfall)
2. **Kommenterar bort HTTPS** och får resten att bygga
3. **Dokumenterar och avbryter** så får du undersöka ESP-IDF versioner
