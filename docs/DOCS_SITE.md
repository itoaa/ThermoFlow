# Publik dokumentationssajt

ThermoFlows handböcker publiceras som en **statisk webbplats** (MkDocs Material) via **GitHub Pages**.

| | |
|--|--|
| **URL** | https://itoaa.github.io/ThermoFlow/ |
| **Källa** | `docs/` + `mkdocs.yml` i repot |
| **Bygge** | [`.github/workflows/docs.yml`](https://github.com/itoaa/ThermoFlow/blob/main/.github/workflows/docs.yml) |
| **ESP-UI** | Länkar + `docs_url` i `/api/device/info` |

## Automatisk uppdatering

Vid **push till `main`** som ändrar `docs/**`, `mkdocs.yml` eller docs-workflowen:

1. GitHub Actions installerar MkDocs Material  
2. `mkdocs build --strict`  
3. Resultatet (`site/`) publiceras till GitHub Pages  

Ingen separat “poll-tjänst” behövs. Manuell körning: **Actions → Documentation → Run workflow**.

### Engångsinställning i GitHub

1. Repo → **Settings → Pages**  
2. **Build and deployment → Source:** *GitHub Actions*  
3. Efter första lyckade körningen visas URL:en under Pages  

För organisation/repo med begränsningar: ge Actions behörighet att skriva till Pages (workflow permissions).

## Lokal utveckling

```bash
cd ThermoFlow   # repo-rot
pip install -r requirements-docs.txt
mkdocs serve          # http://127.0.0.1:8000
mkdocs build --strict # output i site/
```

`site/` ska **inte** committas (finns i `.gitignore`).

## Koppling till enhets-UI

| Mekanism | Beskrivning |
|----------|-------------|
| Nav **Dokumentation** | Öppnar docs startsida i ny flik |
| Inställningar → **Dokumentation** | Snabb länkar (sensorer, WiFi, lägen) |
| **?** hjälp-modal | “Öppna dokumentation” → rätt kapitel på sajten |
| API `docs_url` | Bas-URL (standard GitHub Pages); UI kan överskridas senare |

Firmware lagrar **inte** hela HTML-handboken — bara korta hjälptexter + externa länkar.

## Byta domän eller host

1. Ändra `site_url` i `mkdocs.yml`  
2. Ändra `THERMOFLOW_DOCS_BASE_URL` i `include/thermoflow_config.h`  
3. Om Cloudflare Pages / Netlify: peka samma repo, byggkommando `mkdocs build`, publiceringskatalog `site`  

## Felsökning

| Problem | Åtgärd |
|---------|--------|
| 404 på Pages | Kontrollera att Source = GitHub Actions och att deploy-jobbet lyckats |
| `mkdocs build --strict` failar | Trasig länk i markdown — rätta i `docs/` |
| UI länkar till fel sida | Kontrollera `doc:`-fält i `HELP_CATALOG` (`script.js`) och sidans slug |
