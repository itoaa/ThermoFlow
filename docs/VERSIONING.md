# Versionshantering (CalVer)

**Senast uppdaterad:** 2026-07-14

ThermoFlow använder **Calendar Versioning** med format:

```
YYYY.WW.BUILD
```

| Del | Betydelse | Exempel |
|-----|-----------|---------|
| `YYYY` | Fullständigt år | `2026` |
| `WW` | ISO-veckonummer | `29` |
| `BUILD` | CI build-nummer eller revision | `42` |

Exempel: **`2026.29.42`**

---

## Release vs pre-release

| Läge | Miljövariabel | Format |
|------|---------------|--------|
| Pre-release (standard) | `USE_BUILD_VERSION=1` | `YYYY.WW.BUILD` |
| Release | `USE_BUILD_VERSION=0` | `YYYY.WW.REVISION` |

`REVISION` sätts manuellt vid release (t.ex. `REVISION=1` → `2026.29.1` i `calver`-fältet).

---

## Lokala byggen

Skript: `build-local.ps1` (Windows) / `scripts/generate_version.py`

Lokala byggen sätter:

- `BUILD_NUMBER` — hämtas från senaste GitHub Actions-körning (om `FETCH_GITHUB_BUILD=1`)
- `GIT_SHA` — kort git-commit
- `CHANNEL` — t.ex. `dev`

**Full version** (`version_full`):

```
2026.29.42+040f567
```

Genereras i `include/thermoflow_version.h` vid varje build (körs automatiskt av CMake/CI).

---

## Var versionen syns

| Plats | Fält |
|-------|------|
| `GET /api/device/info` | `firmware_version`, `version_full`, `build_number`, `git_sha` |
| Web UI Inställningar | Firmware-version |
| Boot-logg | `THERMOFLOW_VERSION_FULL` |

---

## CI (GitHub Actions)

`.github/ci-build.sh` sätter `BUILD_NUMBER` från `GITHUB_RUN_NUMBER`.

Vid push till `main` ökas build-numret automatiskt per CI-körning.

---

## Manuell generering

```bash
export USE_BUILD_VERSION=1
export CHANNEL=dev
export REVISION=1
python3 scripts/generate_version.py
```

**Redigera inte** `include/thermoflow_version.h` för hand — den skrivs över av generatorn.