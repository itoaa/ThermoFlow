/**
 * ThermoFlow Modern Web Interface
 * SPA with real-time updates, charts, and theme support
 */

// Configuration
const API_BASE = '/api';
const DEFAULT_UPDATE_INTERVAL = 5000;
const DEMO_MODE = new URLSearchParams(window.location.search).has('demo') ||
    localStorage.getItem('thermoflowDemo') === '1';

/** Chart window lengths (ms). History is kept for the longest window. */
const HISTORY_RANGES_MS = {
    '5m': 5 * 60 * 1000,
    '10m': 10 * 60 * 1000,
    '1h': 60 * 60 * 1000,
    '6h': 6 * 60 * 60 * 1000,
    '24h': 24 * 60 * 60 * 1000
};
const HISTORY_KEEP_MS = HISTORY_RANGES_MS['24h'];
const HISTORY_MAX_POINTS = 2000;

function emptyHistoryData() {
    return {
        timestamps: [],
        outdoor: [],
        indoor: [],
        extract: [],
        exhaust: [],
        coolingDelta: []
    };
}

// State
const state = {
    connected: false,
    currentView: 'dashboard',
    theme: localStorage.getItem('theme') || 'dark',
    updateInterval: parseInt(localStorage.getItem('updateInterval')) || DEFAULT_UPDATE_INTERVAL,
    historyRange: localStorage.getItem('historyRange') || '10m',
    historyData: emptyHistoryData(),
    dataSource: 'auto',       // auto | simulation | hardware
    simulationMode: false,    // runtime: using simulated sensor stream
    sensors: [],
    fans: [],
    ftx: null,
    applicationProfile: localStorage.getItem('applicationProfile') || 'heat_exchanger'
};

/**
 * Application modes — mirrors firmware docs/APPLICATION_MODES.md
 * API capabilities can override labels/views at runtime.
 */
const APPLICATION_PROFILES = {
    mini_ftx: {
        label: 'Mini-FTX',
        description: 'Regenerativ värmeåtervinning med keramiskt element. Fläkt växelvis in och ut.',
        views: ['dashboard', 'sensors', 'ftx', 'logs', 'settings'],
        ftxNavLabel: 'FTX',
        dashboardSubtitle: 'Regenerativ ventilation med värmeåtervinning',
        sensorsSubtitle: 'Till-, från- och uteluft',
        gaugeOutdoor: 'Utomhus',
        gaugeIndoor: 'Tilluft',
        ftxTitle: 'Mini-FTX',
        ftxSubtitle: 'Keramisk lagring · växlande flöde in/ut',
        controlHint: 'Styr fläkthastighet och cykel. Av = endast mätning.',
        fan1Label: 'Fläkt (växlande riktning)',
        fan2Label: null,
        sensors: [
            { name: 'Utomhus', icon: 'cloud-sun', tempKey: 'outdoor_temp', rhKey: 'outdoor_rh' },
            { name: 'Tilluft', icon: 'wind', tempKey: 'supply_temp', rhKey: 'supply_rh' },
            { name: 'Frånluft', icon: 'home', tempKey: 'extract_temp', rhKey: 'extract_rh' },
            { name: 'Avluft', icon: 'sign-out-alt', tempKey: 'exhaust_temp', rhKey: 'exhaust_rh' }
        ],
        gaugePrimary: { tempKey: 'outdoor_temp', rhKey: 'outdoor_rh' },
        gaugeSecondary: { tempKey: 'supply_temp', rhKey: 'supply_rh' },
        showFtxStats: true,
        showPwmFans: true,
        showFan2: false,
        showCycle: true,
        showAcControls: false,
        capabilitiesSummary: [
            '1 fläkt, regenerativ cykel',
            'Värmeåtervinningsstatistik',
            'Valfri styrning (PWM)'
        ]
    },
    heat_exchanger: {
        label: 'Värmeväxlare',
        description: 'Kontinuerlig tilluft och frånluft samtidigt. Oberoende styrning av två fläktar.',
        views: ['dashboard', 'sensors', 'ftx', 'logs', 'settings'],
        ftxNavLabel: 'Fläktar',
        dashboardSubtitle: 'Överblick av värmeväxlaren',
        sensorsSubtitle: 'Intags- och utblåsluft',
        gaugeOutdoor: 'Tilluft / intag',
        gaugeIndoor: 'Frånluft / avluft',
        ftxTitle: 'Värmeväxlare',
        ftxSubtitle: 'Kontinuerligt flöde · oberoende in- och utfläkt',
        controlHint: 'Styr tillufts- och frånluftsfläkt var för sig. Av = endast mätning.',
        fan1Label: 'Tilluft (in)',
        fan2Label: 'Frånluft (ut)',
        sensors: [
            { name: 'Tilluft / intag', icon: 'sign-in-alt', tempKey: 'supply_temp', rhKey: 'supply_rh' },
            { name: 'Frånluft', icon: 'home', tempKey: 'extract_temp', rhKey: 'extract_rh' },
            { name: 'Avluft', icon: 'sign-out-alt', tempKey: 'exhaust_temp', rhKey: 'exhaust_rh' },
            { name: 'Uteluft', icon: 'cloud-sun', tempKey: 'outdoor_temp', rhKey: 'outdoor_rh' }
        ],
        gaugePrimary: { tempKey: 'supply_temp', rhKey: 'supply_rh' },
        gaugeSecondary: { tempKey: 'exhaust_temp', rhKey: 'exhaust_rh' },
        showFtxStats: false,
        showPwmFans: true,
        showFan2: true,
        showCycle: false,
        showAcControls: false,
        capabilitiesSummary: [
            '2 fläktar, oberoende',
            'Kontinuerligt flöde',
            'Kondenseringsskydd'
        ]
    },
    ac_monitor: {
        label: 'Mobil AC',
        description: 'Portabel AC: fyra mätpunkter (kallsida/varmsida intag + ut). Valfria tillval: IR-fjärr, linjestyrning, hjälpfläktar.',
        views: ['dashboard', 'sensors', 'ftx', 'logs', 'settings'],
        ftxNavLabel: 'Mobil AC',
        dashboardSubtitle: 'Kylprestanda och temperaturer för din AC',
        sensorsSubtitle: 'Kallsida och varmsida (intag + ut)',
        gaugeOutdoor: 'Utgående kall luft',
        gaugeIndoor: 'Utgående varm luft',
        ftxTitle: 'Mobil AC',
        ftxSubtitle: 'Fyra mätpunkter · kyllyft · verkningsgrad',
        controlHint: 'Aktivera tillval under Inställningar, sedan styrning här.',
        fan1Label: 'Hjälpfläkt 1',
        fan2Label: 'Hjälpfläkt 2',
        sensors: [
            { name: 'Utgående kall luft', icon: 'snowflake', tempKey: 'supply_temp', rhKey: 'supply_rh', role: 'supply' },
            { name: 'Kallsida intag', icon: 'wind', tempKey: 'extract_temp', rhKey: 'extract_rh', role: 'extract' },
            { name: 'Utgående varm luft', icon: 'fire', tempKey: 'exhaust_temp', rhKey: 'exhaust_rh', role: 'exhaust' },
            /* Slot outdoor_temp: FTX=ute; Mobil AC=varmsida intag (1-slang ofta rum, 2-slang ofta ute) */
            { name: 'Varmsida intag', icon: 'sign-in-alt', tempKey: 'outdoor_temp', rhKey: 'outdoor_rh', role: 'hot_in' }
        ],
        gaugePrimary: { tempKey: 'supply_temp', rhKey: 'supply_rh' },
        gaugeSecondary: { tempKey: 'exhaust_temp', rhKey: 'exhaust_rh' },
        showFtxStats: false,
        showPwmFans: false,
        showFan2: false,
        showCycle: false,
        showAcControls: true,
        isMobileAc: true,
        capabilitiesSummary: [
            '4 mätpunkter (kall/varm in+ut)',
            'Kyllyft, COP-proxy, verkningsgrad',
            'Valfri IR / linje / hjälpfläktar'
        ]
    },
    sensor_only: {
        label: 'Endast sensorer',
        description: 'Temperatur och luftfuktighet utan fläkt- eller AC-styrning.',
        views: ['dashboard', 'sensors', 'logs', 'settings'],
        ftxNavLabel: null,
        dashboardSubtitle: 'Sensorövervakning',
        sensorsSubtitle: 'Alla anslutna sensorer',
        gaugeOutdoor: 'Sensor 1',
        gaugeIndoor: 'Sensor 2',
        ftxTitle: null,
        ftxSubtitle: null,
        controlHint: null,
        fan1Label: null,
        fan2Label: null,
        sensors: [
            { name: 'Sensor 1', icon: 'thermometer-half', tempKey: 'outdoor_temp', rhKey: 'outdoor_rh' },
            { name: 'Sensor 2', icon: 'thermometer-half', tempKey: 'supply_temp', rhKey: 'supply_rh' },
            { name: 'Sensor 3', icon: 'thermometer-half', tempKey: 'extract_temp', rhKey: 'extract_rh' },
            { name: 'Sensor 4', icon: 'thermometer-half', tempKey: 'exhaust_temp', rhKey: 'exhaust_rh' }
        ],
        gaugePrimary: { tempKey: 'outdoor_temp', rhKey: 'outdoor_rh' },
        gaugeSecondary: { tempKey: 'supply_temp', rhKey: 'supply_rh' },
        showFtxStats: false,
        showPwmFans: false,
        showFan2: false,
        showCycle: false,
        showAcControls: false,
        capabilitiesSummary: ['Endast mätning och logg']
    }
};

function getProfileConfig(profileId = state.applicationProfile) {
    return APPLICATION_PROFILES[profileId] || APPLICATION_PROFILES.heat_exchanger;
}

/**
 * Public documentation base URL (MkDocs → GitHub Pages).
 * Overridden by GET /api/device/info → docs_url when the device is online.
 */
let DOCS_BASE_URL = 'https://itoaa.github.io/ThermoFlow';

/**
 * Build a URL into the public docs site.
 * @param {string} [docField] e.g. "MOBILE_AC.md#sensorplacering" or "SENSOR_WIRING.md"
 */
function resolveDocsUrl(docField) {
    const base = (DOCS_BASE_URL || 'https://itoaa.github.io/ThermoFlow').replace(/\/$/, '');
    if (!docField) {
        return `${base}/`;
    }
    if (/^https?:\/\//i.test(docField)) {
        return docField;
    }
    const hashIdx = docField.indexOf('#');
    const file = hashIdx >= 0 ? docField.slice(0, hashIdx) : docField;
    const hash = hashIdx >= 0 ? docField.slice(hashIdx + 1) : '';
    const page = file.replace(/\.md$/i, '').replace(/^\//, '');
    // MkDocs serves docs/index.md at site root
    let url = (!page || page.toLowerCase() === 'index') ? `${base}/` : `${base}/${page}/`;
    if (hash) {
        url += `#${hash}`;
    }
    return url;
}

function setDocsBaseUrl(url) {
    if (url && typeof url === 'string' && url.startsWith('http')) {
        DOCS_BASE_URL = url.replace(/\/$/, '');
    }
    document.querySelectorAll('[data-docs-home]').forEach((el) => {
        if (el.tagName === 'A') {
            el.href = resolveDocsUrl();
        }
    });
    document.querySelectorAll('[data-docs-path]').forEach((el) => {
        if (el.tagName === 'A') {
            el.href = resolveDocsUrl(el.getAttribute('data-docs-path'));
        }
    });
}

/** Help texts synced with public docs (short = hover, long = click) */
const HELP_CATALOG = {
    'wiring.overview': {
        title: 'Ansluta temp/fukt-givare',
        short: 'SHT40 via Cat 5e/6 till GPIO 8/9.',
        long: 'ThermoFlow läser SHT40 över I2C: SDA = GPIO 8 (blå), SCL = GPIO 9 (orange), 3V3 (brun), GND (vit/blå + vit/orange). Använd expansionskortets skruvplintar. Full färgkod och kopplingsdiagram finns i dokumentationen.',
        doc: 'SENSOR_WIRING.md'
    },
    'docs.home': {
        title: 'Dokumentation',
        short: 'Publik handbok (uppdateras från GitHub).',
        long: 'Fullständig dokumentation hostas på GitHub Pages och byggs automatiskt när filer under docs/ ändras i repot. Enhetens UI visar live-data; handböcker och kopplingsscheman ligger externt.',
        doc: 'index.md'
    },
    'ac.modules_intro': {
        title: 'Mobil AC – tillval',
        short: 'Välj hur ThermoFlow kopplas till din portabla AC.',
        long: 'Sensorövervakning är alltid aktiv. IR-fjärr, linjestyrning och hjälpfläktar är valfria tillval som aktiverar fliken Styrning. Se dokumentationen för montering och API.',
        doc: 'MOBILE_AC.md#integrationer-val-under-inställningar'
    },
    'ac.mod_ir': {
        title: 'IR-fjärr',
        short: 'Styr AC som en infraröd fjärrkontroll.',
        long: 'ThermoFlow ska kunna efterlikna fjärrkontrollens IR-koder (på/av, läge, fläkt). Mjukvaran sparar avsikt redan nu; faktisk sändning kräver IR-LED och protokollmappning.',
        doc: 'MOBILE_AC.md#ir-fjärr'
    },
    'ac.mod_line': {
        title: 'Linjestyrning',
        short: 'På/av via relä eller torrkontakt.',
        long: 'Elektrisk signal (relä/GPIO) för enkla kommandon som ström. Kräver mappad utgång. Sparas som linjestyrning tills hårdvara är ansluten.',
        doc: 'MOBILE_AC.md#linjestyrning'
    },
    'ac.mod_assist': {
        title: 'Hjälpfläktar',
        short: 'PWM-fläktar som stödjer luftflöde.',
        long: 'Använder ThermoFlows fläktutgångar som tillägg till AC:ns egen fläkt, t.ex. för avluft eller cirkulation. Oberoende av IR/linje.',
        doc: 'MOBILE_AC.md#hjälpfläktar'
    },
    'ac.cold_side': {
        title: 'Kallsida (intag + ut)',
        short: 'Kallsida intag och utgående kall luft.',
        long: 'Kallsida intag (extract) tas in till förångaren; utgående kall luft (supply/kylutblås) blåses ut i rummet. Skillnaden är kyllyftet i luftströmmen.',
        doc: 'MOBILE_AC.md#sensorplacering'
    },
    'ac.cold_in': {
        title: 'Kallsida intag',
        short: 'Luft in till förångaren (kalla sidan).',
        long: 'Mätpunkt för luft som går in till kallsida/förångare. API-slot extract_temp. Används som T_kall_in i kyllyft och COP-proxy. Parallell benämning till Varmsida intag.',
        doc: 'MOBILE_AC.md#sensorplacering'
    },
    'ac.hot_side': {
        title: 'Varmsida (intag + ut)',
        short: 'Varmsida intag och utgående varm luft.',
        long: 'Varmsida intag = luft in till kondensorn (API-slot outdoor_temp). Vid 1-slangars AC är det oftast rumsluft; vid 2-slangars oftast uteluft. Utgående varm luft (exhaust) lämnar via slang. Skillnaden är värmeavgivningen. Mät blandad bulktemp i utblåset.',
        doc: 'MOBILE_AC.md#sensorplacering'
    },
    'ac.hot_in': {
        title: 'Varmsida intag',
        short: 'Luft in till kondensorn — inte nödvändigtvis utomhus.',
        long: 'Neutral mätpunkt för varmsidans intag. 1-slang: ofta rumsluft som sugs in till kondensorn. 2-slang: oftast uteluft via separat intagsslang. Används som T_varm_in i värmeavgivning och COP-proxy. API-nyckeln är outdoor_temp (delas med FTX-lägets uteluft-slot).',
        doc: 'MOBILE_AC.md#varmsida-intag'
    },
    'ac.mixed_out': {
        title: 'Blandtemperatur ut',
        short: 'Medel av kall ut och varm ut.',
        long: 'T_mix ≈ (T_kall_ut + T_varm_ut) / 2. Används som snabb kontroll: vid rimlig energibalans bör blandtemperaturen ligga nära genomsnittlig intagstemperatur plus kompressorvärme. Avvikelse kan tyda på ojämnt massflöde eller dålig sensormontering i utblåset.',
        doc: 'MOBILE_AC.md#blandtemperatur-och-verkningsgrad'
    },
    'ac.cooling_delta': {
        title: 'Kyllyft (ΔT)',
        short: 'Hur många grader luftströmmen kyls.',
        long: 'Kyllyft = T_kallsida_intag − T_kall_ut (°C). Det är den direkta “hur många grader den kyler”-mätningen i strömmen. Saknas kallsida intag används varmsida intag som fallback-referens.',
        doc: 'MOBILE_AC.md#kyllyft-δt'
    },
    'ac.heat_reject': {
        title: 'Värmeavgivning (ΔT)',
        short: 'Hur mycket luften värms på varmsidan.',
        long: 'Värmeavgivning = T_varm_ut − T_varmsida_intag (°C). Visar att värme pumpas bort. Varmsida intag är inte nödvändigtvis utomhus (1-slang ofta rum). Mät blandad bulktemperatur i utblåset.',
        doc: 'MOBILE_AC.md#värmeavgivning-δt'
    },
    'ac.side_balance': {
        title: 'Sidobalans',
        short: 'Förhållande mellan varm ΔT och kyl ΔT.',
        long: 'ΔT_varm / ΔT_kall när kyllyft > 0,5 °C. Vid ungefär lika massflöde brukar värdet ligga > 1 eftersom kompressorarbete också blir värme på varmsidan.',
        doc: 'MOBILE_AC.md#sidobalans'
    },
    'ac.thermal_eff': {
        title: 'Termisk verkningsgrad',
        short: 'Andel av avgiven värme som kommer från kyla.',
        long: 'η = ΔT_kall / ΔT_varm × 100 % (antag lika massflöde). Högre värde betyder att mer av varmsidans värme kommer från kyleffekten. Inte elektrisk COP, men en bra hälsokoll utan elmätare.',
        doc: 'MOBILE_AC.md#blandtemperatur-och-verkningsgrad'
    },
    'ac.cop_proxy': {
        title: 'COP-proxy (luftsidig)',
        short: 'Uppskattad kyleffekt per tillförd “kompressorvärme”.',
        long: 'COP ≈ ΔT_kall / (ΔT_varm − ΔT_kall) när ΔT_varm > ΔT_kall. Bygger på energibalans: Q_varm ≈ Q_kall + W. Kräver att massflödena är ungefär lika och att utgående varmuft mäts som blandad bulktemperatur. Inte samma sak som fabrikens COP (som kräver eleffekt).',
        doc: 'MOBILE_AC.md#blandtemperatur-och-verkningsgrad'
    },
    'ac.cond_risk': {
        title: 'Kondensrisk',
        short: 'Risk för kondens i kalluft/slang.',
        long: 'Baseras på fukt och temperatur i kylutblås. Hög RH + låg temperatur → högre risk. Klassas som låg / medel / hög enligt dokumentationen.',
        doc: 'MOBILE_AC.md#kondensrisk'
    },
    'ac.cooling_index': {
        title: 'Kylindex',
        short: '0–100: hur hårt AC:n kyler just nu.',
        long: 'Normaliserat från kyllyft (0 °C → 0, ca 12 °C → 100). Lättläst indikator utan energimätare.',
        doc: 'MOBILE_AC.md#kylindex'
    },
    'ac.cold_rh': {
        title: 'Fukt i kylutblås',
        short: 'Relativ fukt i den kalla luften.',
        long: 'Hög RH på kall luft ökar kondensrisk i slang och utblås. Kombinera med kylutblåstemperatur.',
        doc: 'MOBILE_AC.md#fukt-i-kylutblås'
    },
    'ac.policy': {
        title: 'Driftpolicy',
        short: 'Hur fläktar och fjärr beter sig i normal drift och vid risk.',
        long: 'Manuell = du sätter PWM. Automatisk = hastighet beräknas från fukt/temperatur. Vid kondensrisk kan policy förstärka fläktar eller begära fläktläge på AC:n (IR/linje).',
        doc: 'MOBILE_AC.md#styrning-driftpolicy'
    },
    'ac.run_mode': {
        title: 'Reglerläge',
        short: 'Manuell eller automatisk fläktstyrning.',
        long: 'Manuell: börvärden från reglage. Automatisk: baseras på fukt i kylutblås och kylbehov, likt vanliga ESPHome/HA PWM-fläktregulatorer.',
        doc: 'MOBILE_AC.md#manuell-vs-automatisk'
    },
    'ac.cond_action': {
        title: 'Vid kondensrisk',
        short: 'Vad systemet gör när kalluft + hög RH indikerar kondens.',
        long: 'Endast övervaka = logg/UI. Förstärk hjälpfläktar = höj PWM för att torka/transportera fuktig luft. Begär fläktläge = skicka avsikt till AC via IR/linje (stub tills hårdvara).',
        doc: 'MOBILE_AC.md#kondensrisk-åtgärder'
    },
    'ac.assist_live': {
        title: 'Hjälpfläktar – live',
        short: 'Börvärde (PWM %) och RPM från tachometer.',
        long: 'Börvärde är den signal ThermoFlow skickar (0–100 %). RPM kräver 4-pin fläkt med tach till GPIO. Saknas tach visas —. Börvärde > 0 och RPM = 0 kan tyda på fel eller saknad sensor.',
        doc: 'MOBILE_AC.md#hjälpfläktar-och-rpm'
    },
    'ac.remote_cmd': {
        title: 'Fjärrkommando',
        short: 'IR eller linje till själva AC-aggregatet.',
        long: 'Ström, kylläge och fläktläge är avsikter som sparas och loggas. När IR-sändare eller relä är inkopplat utförs de fysiskt. Se liknande mönster i Home Assistant IR/climate + ESPHome.',
        doc: 'MOBILE_AC.md#fjärrkommando'
    }
};

function setupHelpSystem() {
    // Bind static docs links (nav / settings) to public site
    setDocsBaseUrl(DOCS_BASE_URL);

    document.body.addEventListener('click', (e) => {
        const tip = e.target.closest('.help-tip');
        if (tip) {
            e.preventDefault();
            e.stopPropagation();
            openHelp(tip.dataset.help);
            return;
        }
        if (e.target.closest('[data-help-close]')) {
            closeHelp();
        }
        const docLink = e.target.closest('.doc-link');
        if (docLink) {
            e.preventDefault();
            openHelp(docLink.dataset.doc === 'MOBILE_AC' ? 'ac.modules_intro' : docLink.dataset.help);
        }
    });

    document.body.addEventListener('mouseover', (e) => {
        const tip = e.target.closest('.help-tip');
        if (!tip || tip.dataset.tooltipBound) return;
        const entry = HELP_CATALOG[tip.dataset.help];
        if (entry?.short) {
            tip.title = entry.short;
            tip.dataset.tooltipBound = '1';
        }
    });
}

function openHelp(helpId) {
    const entry = HELP_CATALOG[helpId];
    const modal = document.getElementById('help-modal');
    if (!modal || !entry) return;
    document.getElementById('help-modal-title').textContent = entry.title || 'Hjälp';
    const body = document.getElementById('help-modal-body');
    body.replaceChildren();
    const p1 = document.createElement('p');
    p1.textContent = entry.short || '';
    const p2 = document.createElement('p');
    p2.textContent = entry.long || '';
    body.append(p1, p2);
    const link = document.getElementById('help-modal-doc-link');
    if (link) {
        link.href = resolveDocsUrl(entry.doc || 'index.md');
        link.textContent = 'Öppna full dokumentation';
        link.target = '_blank';
        link.rel = 'noopener noreferrer';
    }
    modal.classList.remove('profile-hidden');
}

function closeHelp() {
    document.getElementById('help-modal')?.classList.add('profile-hidden');
}

/**
 * True when UI may show simulated numbers.
 * - Demo (?demo=1): always
 * - Simulering: always
 * - Auto: yes (device may fall back to sim when no sensors)
 * - Sensorer (hardware): no — only real readings or N/A
 */
function allowSyntheticMeasurements() {
    return DEMO_MODE
        || state.dataSource === 'simulation'
        || state.dataSource === 'auto';
}

/** True when the payload is a synthetic stream (sim/demo), not live sensors. */
function isSyntheticPayload(data) {
    if (DEMO_MODE) return true;
    if (state.dataSource === 'simulation') return true;
    if (data?.simulation_mode || data?.mode === 'SIMULATION') return true;
    return false;
}

/**
 * Whether measurement numbers may be shown.
 * Auto/Sim/Demo: show stream (sim or real). Hardware: only non-synthetic data.
 * Per-channel N/A still comes from valid{} / null when a sensor is missing.
 */
function mayDisplayMeasurements(data) {
    if (allowSyntheticMeasurements()) return true;
    if (isSyntheticPayload(data)) return false;
    return true;
}

function toFiniteNumber(value) {
    if (value == null || value === '') return null;
    const n = Number(value);
    return Number.isFinite(n) ? n : null;
}

function formatTempC(value) {
    const n = toFiniteNumber(value);
    return n == null ? 'N/A' : `${n.toFixed(1)}°C`;
}

function formatRh(value) {
    const n = toFiniteNumber(value);
    return n == null ? 'N/A' : `${n.toFixed(0)}% RH`;
}

function formatNum(value, digits = 1) {
    const n = toFiniteNumber(value);
    return n == null ? 'N/A' : n.toFixed(digits);
}

/**
 * Sanitize sensor object for display: null invalid/missing channels.
 * Synthetic streams are shown in Auto/Sim/Demo; hardware mode blanks them.
 * Missing sensors (valid[role]=false or null) become N/A via formatters.
 */
function sanitizeSensorsForDisplay(sensors, dataMeta = null) {
    if (!sensors || typeof sensors !== 'object') return null;
    if (!mayDisplayMeasurements(dataMeta || { simulation_mode: state.simulationMode })) {
        return {
            outdoor_temp: null, outdoor_rh: null,
            supply_temp: null, supply_rh: null,
            extract_temp: null, extract_rh: null,
            exhaust_temp: null, exhaust_rh: null
        };
    }

    const valid = sensors.valid || {};
    const pick = (key, role) => {
        const roleOk = valid[role];
        if (roleOk === false) return null;
        return toFiniteNumber(sensors[key]);
    };

    return {
        outdoor_temp: pick('outdoor_temp', 'outdoor'),
        outdoor_rh: pick('outdoor_rh', 'outdoor'),
        supply_temp: pick('supply_temp', 'supply'),
        supply_rh: pick('supply_rh', 'supply'),
        extract_temp: pick('extract_temp', 'extract'),
        extract_rh: pick('extract_rh', 'extract'),
        exhaust_temp: pick('exhaust_temp', 'exhaust'),
        exhaust_rh: pick('exhaust_rh', 'exhaust')
    };
}

/**
 * Mobil AC metrics from four-point measurements.
 *
 * Mapping (API keys):
 *   supply  = utgående kall luft
 *   extract = kallsida intag
 *   exhaust = utgående varm luft
 *   outdoor_temp slot = varmsida intag (1-slang ofta rum; 2-slang ofta ute)
 *
 * Energy-balance COP proxy (equal mass-flow assumption):
 *   Q ∝ m·cp·ΔT  →  COP ≈ ΔT_c / (ΔT_h − ΔT_c) when ΔT_h > ΔT_c
 * Thermal effectiveness:
 *   η = ΔT_c / ΔT_h  (share of rejected heat that came from cooling)
 * Mixed outlet check:
 *   T_mix = (T_kall_ut + T_varm_ut) / 2
 */
function computeAcMetrics(sensors) {
    if (!sensors) return null;

    const TkOut = toFiniteNumber(sensors.supply_temp);
    const TkIn = toFiniteNumber(sensors.extract_temp);
    const TvOut = toFiniteNumber(sensors.exhaust_temp);
    const TvIn = toFiniteNumber(sensors.outdoor_temp);
    const RHk = toFiniteNumber(sensors.supply_rh);
    const RHv = toFiniteNumber(sensors.exhaust_rh);
    const RHkIn = toFiniteNumber(sensors.extract_rh);
    const RHvIn = toFiniteNumber(sensors.outdoor_rh);

    let coolingDelta = null;
    if (TkIn != null && TkOut != null) {
        coolingDelta = TkIn - TkOut;
    } else if (TvIn != null && TkOut != null) {
        /* Fallback: use hot-side inlet (often room air) as reference */
        coolingDelta = TvIn - TkOut;
    }

    let heatReject = null;
    if (TvOut != null && TvIn != null) {
        heatReject = TvOut - TvIn;
    }

    const balance = (coolingDelta != null && coolingDelta > 0.5 && heatReject != null)
        ? heatReject / coolingDelta
        : null;

    /* η = Qc/Qh under equal mass flow */
    let thermalEff = null;
    if (coolingDelta != null && heatReject != null && heatReject > 0.3) {
        thermalEff = Math.max(0, Math.min(150, (coolingDelta / heatReject) * 100));
    }

    /* COP_proxy = Qc / W, W ≈ Qh − Qc */
    let copProxy = null;
    if (coolingDelta != null && heatReject != null && heatReject > coolingDelta + 0.3 && coolingDelta > 0) {
        copProxy = coolingDelta / (heatReject - coolingDelta);
        if (copProxy < 0 || copProxy > 20) copProxy = null;
    }

    let mixedOut = null;
    if (TkOut != null && TvOut != null) {
        mixedOut = (TkOut + TvOut) / 2;
    }

    let condRisk = null;
    if (RHk != null && TkOut != null) {
        condRisk = 'Låg';
        if (RHk >= 85 && TkOut <= 16) condRisk = 'Hög';
        else if (RHk >= 70 && TkOut <= 18) condRisk = 'Medel';
    }

    let index = null;
    if (coolingDelta != null) {
        index = Math.max(0, Math.min(100, (coolingDelta / 12) * 100));
    }

    return {
        TkOut, TkIn, TvOut, TvIn, RHk, RHv, RHkIn, RHvIn,
        coolingDelta, heatReject, balance, thermalEff, copProxy, mixedOut,
        condRisk, index
    };
}

function setText(id, text) {
    const el = document.getElementById(id);
    if (el) el.textContent = text;
}

function updateAcOverview(data) {
    const raw = data?.sensors || data;
    const s = sanitizeSensorsForDisplay(raw, data);
    const m = computeAcMetrics(s);

    const synthetic = isSyntheticPayload(data) && allowSyntheticMeasurements();
    const badge = document.getElementById('ac-data-source-badge');
    if (badge) {
        if (DEMO_MODE) {
            badge.textContent = 'Demo';
            badge.className = 'status-badge warning';
        } else if (state.dataSource === 'simulation' || (data?.simulation_mode && allowSyntheticMeasurements())) {
            badge.textContent = 'Simulering';
            badge.className = 'status-badge warning';
        } else if (!mayDisplayMeasurements(data)) {
            badge.textContent = 'Ingen riktig data';
            badge.className = 'status-badge warning';
        } else {
            badge.textContent = 'Live';
            badge.className = 'status-badge ok';
        }
    }

    if (!m) {
        ['ac-cold-in-temp', 'ac-cold-out-temp', 'ac-hot-in-temp', 'ac-hot-out-temp',
            'ac-cold-in-rh', 'ac-cold-out-rh', 'ac-hot-in-rh', 'ac-hot-out-rh',
            'ac-cooling-index', 'ac-core-cooling-delta', 'ac-mixed-temp',
            'ac-metric-cooling-delta', 'ac-metric-heat-reject', 'ac-metric-balance',
            'ac-metric-thermal-eff', 'ac-metric-cop', 'ac-metric-cond-risk',
            'ac-metric-index', 'ac-metric-cold-rh'].forEach(id => setText(id, 'N/A'));
        setText('ac-metric-cond-detail', 'ingen data');
        const bar = document.getElementById('ac-index-bar');
        if (bar) bar.style.width = '0%';
        return;
    }

    setText('ac-cold-in-temp', formatTempC(m.TkIn));
    setText('ac-cold-in-rh', formatRh(m.RHkIn));
    setText('ac-cold-out-temp', formatTempC(m.TkOut));
    setText('ac-cold-out-rh', formatRh(m.RHk));
    setText('ac-hot-in-temp', formatTempC(m.TvIn));
    setText('ac-hot-in-rh', formatRh(m.RHvIn));
    setText('ac-hot-out-temp', formatTempC(m.TvOut));
    setText('ac-hot-out-rh', formatRh(m.RHv));

    setText('ac-cooling-index', m.index == null ? 'N/A' : Math.round(m.index).toString());
    setText('ac-core-cooling-delta', m.coolingDelta == null ? 'N/A' : `${m.coolingDelta.toFixed(1)} °C`);
    setText('ac-mixed-temp', formatTempC(m.mixedOut));

    setText('ac-metric-cooling-delta', formatNum(m.coolingDelta, 1));
    setText('ac-metric-heat-reject', formatNum(m.heatReject, 1));
    setText('ac-metric-balance', m.balance == null ? 'N/A' : m.balance.toFixed(2));
    setText('ac-metric-thermal-eff', m.thermalEff == null ? 'N/A' : `${m.thermalEff.toFixed(0)}%`);
    setText('ac-metric-cop', m.copProxy == null ? 'N/A' : m.copProxy.toFixed(2));
    setText('ac-metric-cond-risk', m.condRisk == null ? 'N/A' : m.condRisk);
    setText('ac-metric-cond-detail',
        m.RHk != null && m.TkOut != null
            ? `${m.RHk.toFixed(0)}% RH · ${m.TkOut.toFixed(1)}°C`
            : 'saknar kalluftsmätning');
    setText('ac-metric-index', m.index == null ? 'N/A' : Math.round(m.index).toString());
    setText('ac-metric-cold-rh', m.RHk == null ? 'N/A' : m.RHk.toFixed(0));

    const bar = document.getElementById('ac-index-bar');
    if (bar) bar.style.width = `${m.index == null ? 0 : Math.round(m.index)}%`;

    updateAcChart(s, m);
    void synthetic;
}

function isMobileAcProfile(id = state.applicationProfile) {
    return id === 'ac_monitor' || getProfileConfig(id).isMobileAc;
}

function profileAllowsView(viewName, profileId = state.applicationProfile) {
    return getProfileConfig(profileId).views.includes(viewName);
}

function applyApplicationProfile(profileId, options = {}) {
    const { switchIfNeeded = true, control = null, capabilities = null } = options;
    const config = getProfileConfig(profileId);
    const prevProfile = state.applicationProfile;
    const seriesLayoutChanged = isMobileAcProfile(prevProfile) !== isMobileAcProfile(profileId);

    state.applicationProfile = profileId;
    localStorage.setItem('applicationProfile', profileId);
    document.documentElement.dataset.profile = profileId;

    const views = capabilities?.views?.length
        ? capabilities.views.map(v => (v === 'control' ? 'ftx' : v))
        : config.views;

    document.querySelectorAll('.nav-link[data-view]').forEach(link => {
        const view = link.dataset.view;
        const allowed = views.includes(view);
        link.classList.toggle('profile-hidden', !allowed);
    });

    const navLabel = capabilities?.control_nav_label || config.ftxNavLabel;
    const ftxNavLabel = document.getElementById('nav-ftx-label');
    if (ftxNavLabel) {
        ftxNavLabel.textContent = navLabel || 'Styrning';
    }

    const navFtx = document.getElementById('nav-ftx');
    if (navFtx) {
        const icon = profileId === 'heat_exchanger' ? 'fa-fan'
            : profileId === 'ac_monitor' ? 'fa-snowflake'
            : profileId === 'mini_ftx' ? 'fa-recycle'
            : 'fa-sliders-h';
        navFtx.querySelector('i').className = `fas ${icon}`;
    }

    document.getElementById('dashboard-subtitle')?.replaceChildren(
        document.createTextNode(config.dashboardSubtitle)
    );
    document.getElementById('sensors-subtitle')?.replaceChildren(
        document.createTextNode(config.sensorsSubtitle)
    );

    const outdoorGauge = document.getElementById('gauge-outdoor-label');
    if (outdoorGauge) {
        outdoorGauge.innerHTML = `<i class="fas fa-temperature-low"></i> ${config.gaugeOutdoor}`;
    }
    const indoorGauge = document.getElementById('gauge-indoor-label');
    if (indoorGauge) {
        indoorGauge.innerHTML = `<i class="fas fa-temperature-high"></i> ${config.gaugeIndoor}`;
    }

    document.getElementById('ftx-title')?.replaceChildren(
        document.createTextNode(config.ftxTitle || 'Styrning')
    );
    document.getElementById('ftx-subtitle')?.replaceChildren(
        document.createTextNode(config.ftxSubtitle || '')
    );

    const showFtxStats = capabilities?.features?.heat_recovery_stats ?? config.showFtxStats;
    const showCycle = capabilities?.features?.alternating_cycle ?? config.showCycle;
    const showPwm = capabilities?.features?.pwm_control ?? config.showPwmFans;
    const showFan2 = capabilities?.features?.dual_fan ?? config.showFan2;
    const isAc = isMobileAcProfile(profileId);

    document.querySelectorAll('.profile-ftx-only').forEach(el => {
        el.classList.toggle('profile-hidden', !showFtxStats && !showCycle);
    });
    document.querySelectorAll('.profile-ac-only').forEach(el => {
        el.classList.toggle('profile-hidden', !isAc);
    });
    document.querySelectorAll('.profile-pwm-fans').forEach(el => {
        el.classList.toggle('profile-hidden', !showPwm);
    });
    document.getElementById('mode-panel-ac')?.classList.toggle('profile-hidden', !isAc);
    document.getElementById('mode-panel-generic')?.classList.toggle('profile-hidden', isAc);
    document.getElementById('ac-modules-panel')?.classList.toggle('profile-hidden', !isAc);

    if (isAc) {
        updateAcModulesUi(control?.ac_modules);
        const hasAct = !!(control?.ac_has_actuation ||
            control?.ac_modules?.ir_remote ||
            control?.ac_modules?.line_control ||
            control?.ac_modules?.assist_fans);
        document.getElementById('ac-subtab-control')?.classList.toggle('profile-hidden', !hasAct);
        document.getElementById('ac-ir-line-panel')?.classList.toggle('profile-hidden',
            !(control?.ac_modules?.ir_remote || control?.ac_modules?.line_control));
        document.getElementById('ac-assist-fans-panel')?.classList.toggle('profile-hidden',
            !control?.ac_modules?.assist_fans);
        if (!hasAct) {
            switchAcTab('monitor');
        }
    }

    const fan2 = document.getElementById('fan2-control');
    if (fan2) fan2.classList.toggle('profile-hidden', !showFan2);

    if (config.fan1Label) {
        const el = document.getElementById('fan1-label');
        if (el) el.textContent = config.fan1Label;
    }
    if (config.fan2Label) {
        const el = document.getElementById('fan2-label');
        if (el) el.textContent = config.fan2Label;
    }

    const hint = document.getElementById('control-mode-hint');
    if (hint && config.controlHint) {
        hint.textContent = config.controlHint;
    }

    const profileSelect = document.getElementById('application-profile');
    if (profileSelect && profileSelect.value !== profileId) {
        profileSelect.value = profileId;
    }
    document.getElementById('profile-description')?.replaceChildren(
        document.createTextNode(config.description)
    );
    document.getElementById('profile-active-label')?.replaceChildren(
        document.createTextNode(`Aktivt läge: ${config.label}`)
    );

    const capList = document.getElementById('profile-capability-list');
    if (capList) {
        capList.replaceChildren();
        (config.capabilitiesSummary || []).forEach(text => {
            const li = document.createElement('li');
            li.textContent = text;
            capList.appendChild(li);
        });
    }

    if (control) {
        applyControlState(control);
    }

    /* Rebuild chart only when series layout changes (AC 5-series vs FTX 2-series).
     * Do NOT wipe history on every poll — fetchDeviceInfo calls this each cycle. */
    if (seriesLayoutChanged && tempChart) {
        tempChart.destroy();
        tempChart = null;
        initChart();
        syncChartsFromHistory();
    }

    if (switchIfNeeded && !views.includes(state.currentView)) {
        switchView('dashboard', { skipProfileCheck: true });
    } else if (state.currentView === 'sensors') {
        fetchSensorsDetail();
    }
}

function applyControlState(control) {
    if (!control) return;
    state.control = control;

    const en = document.getElementById('control-enabled');
    if (en) en.checked = !!control.enabled;

    const mode = control.fan_mode || 'auto';
    document.querySelectorAll('#fan-mode-toggle .btn-toggle').forEach(btn => {
        btn.classList.toggle('active', btn.dataset.mode === mode);
    });
    document.querySelectorAll('#ac-run-mode-toggle .btn-toggle').forEach(btn => {
        const m = btn.dataset.acRun;
        btn.classList.toggle('active', m === mode || (mode === 'off' && m === 'manual'));
    });
    const manualOnly = mode === 'manual';
    document.querySelectorAll('.ac-manual-only').forEach(el => {
        el.classList.toggle('profile-hidden', !manualOnly);
    });

    const s1 = document.getElementById('supply-fan');
    const s1v = document.getElementById('supply-fan-value');
    if (s1 && control.fan1_speed != null) {
        s1.value = control.fan1_speed;
        if (s1v) s1v.textContent = `${control.fan1_speed}%`;
    }
    const s2 = document.getElementById('exhaust-fan');
    const s2v = document.getElementById('exhaust-fan-value');
    if (s2 && control.fan2_speed != null) {
        s2.value = control.fan2_speed;
        if (s2v) s2v.textContent = `${control.fan2_speed}%`;
    }
    const a1 = document.getElementById('ac-assist-fan1');
    const a1v = document.getElementById('ac-assist-fan1-value');
    if (a1 && control.fan1_speed != null) {
        a1.value = control.fan1_speed;
        if (a1v) a1v.textContent = `${control.fan1_speed}%`;
    }
    const a2 = document.getElementById('ac-assist-fan2');
    const a2v = document.getElementById('ac-assist-fan2-value');
    if (a2 && control.fan2_speed != null) {
        a2.value = control.fan2_speed;
        if (a2v) a2v.textContent = `${control.fan2_speed}%`;
    }
    const cy = document.getElementById('cycle-period');
    const cyv = document.getElementById('cycle-period-value');
    if (cy && control.cycle_period_s != null) {
        cy.value = control.cycle_period_s;
        if (cyv) cyv.textContent = `${control.cycle_period_s} s`;
    }
    const phase = document.getElementById('cycle-phase-label');
    if (phase && control.cycle_phase) {
        const map = { idle: 'Viloläge', intake: 'In (tilluft)', exhaust: 'Ut (frånluft)' };
        phase.textContent = `Fas: ${map[control.cycle_phase] || control.cycle_phase}`;
    }

    if (control.ac_modules) {
        updateAcModulesUi(control.ac_modules);
    }

    const condSel = document.getElementById('ac-cond-action');
    if (condSel && control.ac_cond_action) {
        condSel.value = control.ac_cond_action;
    }

    updateAcControlStatus(control, state.ftx);
}

function updateAcControlStatus(control, ftxData) {
    if (!isMobileAcProfile()) return;
    const runMap = { auto: 'Automatisk', manual: 'Manuell', off: 'Av' };
    const condMap = {
        observe: 'Endast övervaka',
        boost_assist: 'Förstärk fläktar',
        request_fan_only: 'Begär fläktläge'
    };
    const set = (id, t) => { const el = document.getElementById(id); if (el) el.textContent = t; };
    set('ac-status-run-mode', runMap[control?.fan_mode] || control?.fan_mode || '—');
    set('ac-status-cond-action', condMap[control?.ac_cond_action] || control?.ac_cond_action || '—');

    const sensors = ftxData?.sensors
        ? sanitizeSensorsForDisplay(ftxData.sensors, ftxData)
        : null;
    const m = computeAcMetrics(sensors);
    set('ac-status-cond-risk', m?.condRisk || 'N/A');
    set('ac-status-cooling-index', m?.index == null ? 'N/A' : Math.round(m.index).toString());

    const last = control?.ac_last_command;
    set('ac-last-command', last ? `Senaste kommando: ${last}` : 'Senaste kommando: —');

    const fans = control?.assist_fans_status || [];
    for (let i = 0; i < 2; i++) {
        const f = fans[i] || {};
        const n = i + 1;
        set(`ac-fan${n}-setpoint`, f.setpoint_pct != null ? `${f.setpoint_pct}%` : '—%');
        set(`ac-fan${n}-rpm`, f.rpm > 0 ? `${f.rpm}` : (f.setpoint_pct > 0 ? '0 (saknas tach?)' : '—'));
        set(`ac-fan${n}-fault`, f.fault ? 'Fel' : (f.setpoint_pct > 0 && !f.rpm ? 'Ingen feedback' : 'OK'));
    }
}

function updateAcModulesUi(mod) {
    const m = mod || { sensing: true, ir_remote: false, line_control: false, assist_fans: false };
    const ir = document.getElementById('ac-mod-ir');
    const line = document.getElementById('ac-mod-line');
    const assist = document.getElementById('ac-mod-assist');
    if (ir) ir.checked = !!m.ir_remote;
    if (line) line.checked = !!m.line_control;
    if (assist) assist.checked = !!m.assist_fans;
}

function switchAcTab(tab) {
    document.querySelectorAll('#ac-subtabs .subtab').forEach(b => {
        b.classList.toggle('active', b.dataset.acTab === tab);
    });
    document.getElementById('ac-tab-monitor')?.classList.toggle('profile-hidden', tab !== 'monitor');
    document.getElementById('ac-tab-control')?.classList.toggle('profile-hidden', tab !== 'control');
}

// Chart instances
let tempChart = null;
let acTempChart = null;
let pollTimer = null;

// Initialize on page load
document.addEventListener('DOMContentLoaded', () => {
    init();
});

function init() {
    if (DEMO_MODE) {
        document.title = 'ThermoFlow | Demo';
        const brand = document.querySelector('.nav-brand span');
        if (brand) brand.textContent = 'ThermoFlow Demo';
        showToast('Demo-läge aktivt — simulerad data', 'info');
    }

    setupHelpSystem();

    // Apply theme
    applyTheme(state.theme);
    
    // Hide loading screen
    setTimeout(() => {
        document.getElementById('loading-screen').classList.add('hidden');
    }, 1000);
    
    // Setup navigation
    setupNavigation();
    
    // Setup theme toggle
    setupThemeToggle();
    
    // Setup refresh button
    document.getElementById('refresh-btn').addEventListener('click', () => {
        showToast('Uppdaterar...', 'info');
        fetchAll();
    });
    
    // Setup chart + history range (5m / 10m / 1h / …)
    initChart();
    setupHistoryRangeControls();
    
    // Setup sliders
    setupSliders();
    
    // Setup settings
    setupSettings();
    setupLogs();
    
    // Start data fetching
    fetchAll();
    startPolling();
    
    // Handle visibility change
    document.addEventListener('visibilitychange', () => {
        if (!document.hidden) {
            fetchAll();
        }
    });
}

// Theme Management
function applyTheme(theme) {
    if (theme === 'auto') {
        const prefersDark = window.matchMedia('(prefers-color-scheme: dark)').matches;
        document.documentElement.setAttribute('data-theme', prefersDark ? 'dark' : 'light');
    } else {
        document.documentElement.setAttribute('data-theme', theme);
    }
    
    // Update icon
    const icon = document.querySelector('#theme-toggle i');
    if (icon) {
        icon.className = theme === 'light' ? 'fas fa-sun' : 'fas fa-moon';
    }
}

function setupThemeToggle() {
    const toggle = document.getElementById('theme-toggle');
    const themes = ['dark', 'light', 'auto'];
    
    toggle.addEventListener('click', () => {
        const currentIndex = themes.indexOf(state.theme);
        state.theme = themes[(currentIndex + 1) % themes.length];
        localStorage.setItem('theme', state.theme);
        applyTheme(state.theme);
        showToast(`Tema: ${state.theme === 'auto' ? 'Automatiskt' : state.theme === 'dark' ? 'Mörkt' : 'Ljust'}`, 'success');
    });
}

// Navigation
function setupNavigation() {
    const navLinks = document.querySelectorAll('.nav-link');
    
    navLinks.forEach(link => {
        link.addEventListener('click', (e) => {
            e.preventDefault();
            const view = link.dataset.view;
            switchView(view);
        });
    });
}

function switchView(viewName, options = {}) {
    if (!options.skipProfileCheck && !profileAllowsView(viewName)) {
        viewName = 'dashboard';
    }

    // Update nav
    document.querySelectorAll('.nav-link').forEach(link => {
        link.classList.toggle('active', link.dataset.view === viewName);
    });
    
    // Update views
    document.querySelectorAll('.view').forEach(view => {
        view.classList.toggle('active', view.id === `view-${viewName}`);
    });
    
    state.currentView = viewName;
    
    // Fetch data for new view
    if (viewName === 'sensors') {
        fetchSensorsDetail();
    } else if (viewName === 'ftx' && profileAllowsView('ftx')) {
        fetchFTX();
    } else if (viewName === 'settings') {
        fetchDeviceInfo();
    } else if (viewName === 'logs') {
        fetchLogs();
    }
}

// Toast Notifications
function showToast(message, type = 'info') {
    const container = document.getElementById('toast-container');
    const toast = document.createElement('div');
    toast.className = `toast ${type}`;
    
    const icons = {
        success: 'check-circle',
        error: 'exclamation-circle',
        warning: 'exclamation-triangle',
        info: 'info-circle'
    };
    
    toast.innerHTML = `
        <i class="fas fa-${icons[type]}"></i>
        <span>${message}</span>
    `;
    
    container.appendChild(toast);
    
    setTimeout(() => {
        toast.style.opacity = '0';
        toast.style.transform = 'translateX(100%)';
        setTimeout(() => toast.remove(), 300);
    }, 4000);
}

// Demo data for static hosting (?demo=1 on any web server)
function generateDemoFtxPayload() {
    const now = Date.now() / 1000;
    const dayPhase = ((now % 86400) / 86400) * Math.PI * 2;
    const hourWobble = Math.sin(((now % 3600) / 3600) * Math.PI * 2) * 0.4;
    const noise = (Math.random() - 0.5) * 0.3;
    const acMode = state.applicationProfile === 'ac_monitor';

    let outdoorTemp, extractTemp, exhaustTemp, supplyTemp;
    let outdoorRh, extractRh, exhaustRh, supplyRh;

    if (acMode) {
        /* Mobile AC demo: cold in/out + hot in/out */
        const room = 24.5 + 1.2 * Math.sin(dayPhase + 0.3) + noise * 0.4;
        extractTemp = room;                         // kallsida intag (rumsluft till förångare)
        supplyTemp = room - (9 + 2.5 * Math.sin(dayPhase + 1.1) + noise); // kall ut
        outdoorTemp = room - 0.5 + hourWobble * 0.3; // varm in (ofta rumsluft)
        exhaustTemp = outdoorTemp + (14 + 3 * Math.sin(dayPhase) + noise); // varm ut
        extractRh = 48 + 8 * Math.sin(dayPhase + 0.5);
        supplyRh = Math.min(95, extractRh + 25 + 5 * Math.sin(dayPhase));
        outdoorRh = extractRh - 2;
        exhaustRh = Math.max(20, outdoorRh - 15);
    } else {
        outdoorTemp = 2 + 9 * Math.sin(dayPhase - 1.2) + hourWobble + noise;
        extractTemp = 20.5 + 1.2 * Math.sin(dayPhase + 0.4) + noise * 0.5;
        exhaustTemp = extractTemp + 1.8;
        supplyTemp = outdoorTemp + (exhaustTemp - outdoorTemp) * 0.82;
        outdoorRh = 78 - 18 * Math.sin(dayPhase);
        extractRh = 42 + 6 * Math.sin(dayPhase + 0.8);
        exhaustRh = extractRh - 4;
        supplyRh = outdoorRh - 12;
    }

    const efficiency = Math.max(0, Math.min(100,
        Math.abs(exhaustTemp - outdoorTemp) > 0.1
            ? ((supplyTemp - outdoorTemp) / (exhaustTemp - outdoorTemp)) * 100
            : 0));
    const fanSpeed = 35 + Math.round(15 * Math.sin(dayPhase + 0.6));

    return {
        valid: true,
        simulation_mode: true,
        mode: 'SIMULATION',
        sensors: {
            outdoor_temp: outdoorTemp,
            outdoor_rh: outdoorRh,
            supply_temp: supplyTemp,
            supply_rh: supplyRh,
            extract_temp: extractTemp,
            extract_rh: extractRh,
            exhaust_temp: exhaustTemp,
            exhaust_rh: exhaustRh,
            valid: { outdoor: true, supply: true, exhaust: true, extract: true }
        },
        efficiency: {
            percent: efficiency,
            power_recovered_w: Math.max(0, Math.abs(supplyTemp - outdoorTemp) * 18),
            airflow_m3h: 120
        },
        fans: {
            supply: fanSpeed,
            exhaust: fanSpeed
        }
    };
}

function mockDemoApi(endpoint, options = {}) {
    if (options.method === 'PUT' && endpoint === '/device/profile' && options.body) {
        try {
            const body = JSON.parse(options.body);
            if (body.profile) {
                state.applicationProfile = body.profile;
                localStorage.setItem('applicationProfile', body.profile);
            }
        } catch (_) { /* ignore */ }
        return {
            success: true,
            application_profile: state.applicationProfile,
            application_profile_label: getProfileConfig().label
        };
    }

    if (options.method === 'POST') {
        return { success: true };
    }

    switch (endpoint) {
        case '/ftx':
            return generateDemoFtxPayload();
        case '/ftx/sensors': {
            const ftx = generateDemoFtxPayload();
            return {
                simulation_mode: true,
                outdoor_temp: ftx.sensors.outdoor_temp,
                outdoor_rh: ftx.sensors.outdoor_rh,
                supply_temp: ftx.sensors.supply_temp,
                supply_rh: ftx.sensors.supply_rh,
                extract_temp: ftx.sensors.extract_temp,
                extract_rh: ftx.sensors.extract_rh,
                exhaust_temp: ftx.sensors.exhaust_temp,
                exhaust_rh: ftx.sensors.exhaust_rh
            };
        }
        case '/ftx/status':
            return {
                simulation_mode: true,
                frost_risk: false,
                bypass_active: false,
                filter_warning: false
            };
        case '/device/info':
            return {
                device_id: 'ThermoFlow-DEMO',
                device_name: 'ThermoFlow-DEMO',
                default_name: 'ThermoFlow-DEMO',
                mac_address: '44:1B:F6:8C:14:40',
                firmware_version: '2026.29.42',
                version_full: '2026.29.42',
                version_scheme: 'build',
                calver: '2026.29.1',
                version_year: 2026,
                version_week: 29,
                version_revision: 42,
                build_number: 42,
                git_sha: 'demo',
                channel: 'demo',
                docs_url: 'https://itoaa.github.io/ThermoFlow',
                docs_repo: 'https://github.com/itoaa/ThermoFlow',
                ip_address: '127.0.0.1',
                wifi_state: 'connected',
                simulation_mode: true,
                free_heap: 180000,
                min_free_heap: 140000,
                memory: {
                    free_heap: 180000,
                    min_free_heap: 140000,
                    largest_free_block: 65536,
                    free_internal: 120000,
                    total_internal: 327680,
                    free_psram: 7 * 1024 * 1024,
                    total_psram: 8 * 1024 * 1024,
                    psram_available: true,
                    psram_policy: 'prefer_bulk_only'
                },
                application_profile: state.applicationProfile,
                application_profile_label: getProfileConfig().label,
                application_profile_description: getProfileConfig().description,
                control: state.control || {
                    enabled: true, method: 'pwm', fan_mode: 'auto',
                    fan1_speed: 40, fan2_speed: 40, cycle_period_s: 60, cycle_phase: 'intake'
                },
                capabilities: {
                    views: getProfileConfig().views,
                    control_nav_label: getProfileConfig().ftxNavLabel,
                    features: {
                        heat_recovery_stats: getProfileConfig().showFtxStats,
                        alternating_cycle: getProfileConfig().showCycle,
                        dual_fan: getProfileConfig().showFan2,
                        ir_control: getProfileConfig().showAcControls,
                        electrical_control: getProfileConfig().showAcControls,
                        pwm_control: getProfileConfig().showPwmFans,
                        control_optional: true,
                        has_control_view: getProfileConfig().views.includes('ftx')
                    }
                }
            };
        case '/device/profile':
            if (options.method === 'PUT') {
                return {
                    success: true,
                    application_profile: state.applicationProfile,
                    application_profile_label: getProfileConfig().label,
                    control: state.control || { enabled: true, method: 'pwm', fan_mode: 'auto', fan1_speed: 40, fan2_speed: 40, cycle_period_s: 60, cycle_phase: 'idle' }
                };
            }
            return {
                application_profile: state.applicationProfile,
                application_profile_label: getProfileConfig().label,
                available_profiles: Object.entries(APPLICATION_PROFILES).map(([id, cfg]) => ({
                    id,
                    label: cfg.label,
                    description: cfg.description,
                    active: id === state.applicationProfile
                }))
            };
        case '/hardware/mode':
            return {
                data_source: 'auto',
                simulation_mode: true,
                sensor_count: 0,
                status: 'SIMULATION - No sensors detected'
            };
        case '/ota/status':
            return {
                state: 'idle',
                partition: 'demo',
                update_method: 'Demo-läge: ingen OTA. Anslut till en riktig ThermoFlow-enhet för uppdateringar.'
            };
        default:
            return { success: true };
    }
}

function normalizeFtxData(data) {
    if (!data) return null;
    if (data.valid === false) return null;
    if (data.valid && data.sensors) {
        if (typeof data.simulation_mode === 'boolean') {
            state.simulationMode = data.simulation_mode;
        }
        return data;
    }

    if (data.outdoor_temp !== undefined || data.supply_temp !== undefined ||
        data.extract_temp !== undefined || data.exhaust_temp !== undefined) {
        if (typeof data.simulation_mode === 'boolean') {
            state.simulationMode = data.simulation_mode;
        }
        return {
            valid: true,
            simulation_mode: data.simulation_mode,
            mode: data.mode,
            sensors: {
                outdoor_temp: data.outdoor_temp,
                outdoor_rh: data.outdoor_rh,
                supply_temp: data.supply_temp,
                supply_rh: data.supply_rh,
                extract_temp: data.extract_temp,
                extract_rh: data.extract_rh,
                exhaust_temp: data.exhaust_temp,
                exhaust_rh: data.exhaust_rh,
                valid: data.valid && typeof data.valid === 'object' ? data.valid : undefined
            },
            efficiency: {
                percent: data.efficiency_percent || 0,
                power_recovered_w: data.energy_recovery_w || 0,
                airflow_m3h: data.airflow_m3h || 120
            },
            fans: {
                supply: data.fan_speed_percent || 0,
                exhaust: data.fan_speed_percent || 0
            }
        };
    }

    return data.valid ? data : null;
}

// API Functions
async function fetchAPI(endpoint, options = {}) {
    if (DEMO_MODE) {
        return mockDemoApi(endpoint, options);
    }

    try {
        const response = await fetch(`${API_BASE}${endpoint}`, {
            ...options,
            headers: {
                'Content-Type': 'application/json',
                ...options.headers
            }
        });
        
        if (!response.ok) throw new Error(`HTTP ${response.status}`);
        return await response.json();
    } catch (error) {
        console.error(`API Error: ${error}`);
        state.connected = false;
        updateConnectionStatus();
        return null;
    }
}

function startPolling() {
    if (pollTimer) {
        clearInterval(pollTimer);
    }
    pollTimer = setInterval(fetchAll, state.updateInterval);
}

async function fetchAll() {
    const tasks = [
        fetchDashboard(),
        fetchSensors(),
        fetchFTX(),
        fetchDeviceInfo(),
        fetchOtaStatus(),
        fetchHardwareMode()
    ];

    if (state.currentView === 'logs') {
        tasks.push(fetchLogs({ silent: true }));
    } else if (state.currentView === 'sensors') {
        tasks.push(fetchSensorsDetail());
    }

    await Promise.all(tasks);

    state.connected = true;
    updateConnectionStatus();
}

async function fetchDashboard() {
    const raw = await fetchAPI('/ftx');
    const data = normalizeFtxData(raw);
    if (!data) return;
    
    state.ftx = data;
    
    const profile = getProfileConfig();
    const sensors = sanitizeSensorsForDisplay(data.sensors, data);
    if (sensors) {
        const primaryTemp = sensors[profile.gaugePrimary.tempKey];
        const secondaryTemp = sensors[profile.gaugeSecondary.tempKey];
        const primaryRh = sensors[profile.gaugePrimary.rhKey];

        updateGauge('outdoor-temp', primaryTemp, '°C');
        updateGauge('indoor-temp', secondaryTemp, '°C');
        updateGauge('humidity', primaryRh, '%');

        const avgEl = document.getElementById('avg-temp');
        const humEl = document.getElementById('avg-humidity');
        if (avgEl) {
            if (primaryTemp != null && secondaryTemp != null) {
                avgEl.textContent = ((primaryTemp + secondaryTemp) / 2).toFixed(1);
            } else if (primaryTemp != null) {
                avgEl.textContent = primaryTemp.toFixed(1);
            } else {
                avgEl.textContent = 'N/A';
            }
        }
        if (humEl) {
            humEl.textContent = primaryRh == null ? 'N/A' : primaryRh.toFixed(1);
        }
    }
    
    const effEl = document.getElementById('ftx-efficiency');
    const powerEl = document.getElementById('power-saved');
    if (isMobileAcProfile() && sensors) {
        const m = computeAcMetrics(sensors);
        const coolEl = document.getElementById('dash-ac-cooling');
        const acEffEl = document.getElementById('dash-ac-eff');
        if (coolEl) coolEl.textContent = m?.coolingDelta == null ? 'N/A' : m.coolingDelta.toFixed(1);
        if (acEffEl) acEffEl.textContent = m?.thermalEff == null ? 'N/A' : m.thermalEff.toFixed(0);
    } else {
        if (effEl && data.efficiency && mayDisplayMeasurements(data)) {
            effEl.textContent = toFiniteNumber(data.efficiency.percent) == null
                ? 'N/A' : data.efficiency.percent.toFixed(1);
        } else if (effEl) {
            effEl.textContent = 'N/A';
        }
        if (powerEl && data.efficiency && data.efficiency.power_recovered_w != null && mayDisplayMeasurements(data)) {
            powerEl.textContent = Math.round(data.efficiency.power_recovered_w);
        } else if (powerEl && data.fans && mayDisplayMeasurements(data)) {
            powerEl.textContent = Math.round(data.fans.supply * 0.5);
        } else if (powerEl) {
            powerEl.textContent = 'N/A';
        }
    }
    
    // Update chart
    updateChart({ ...data, sensors: sensors || data.sensors });
}

async function fetchSensors() {
    const data = await fetchAPI('/ftx/sensors');
    if (data) {
        state.sensors = data;
    }
}

async function fetchFTX() {
    const data = normalizeFtxData(await fetchAPI('/ftx'));
    const status = await fetchAPI('/ftx/status');
    
    if (data) {
        if (isMobileAcProfile()) {
            updateAcOverview(data);
            if (state.control) {
                updateAcControlStatus(state.control, data);
            }
        }

        const sensors = sanitizeSensorsForDisplay(data.sensors, data);
        // Update flow diagram (FTX/HX)
        if (sensors) {
            const o = document.getElementById('ftx-outdoor-temp');
            if (o) o.textContent = formatTempC(sensors.outdoor_temp);
            const s = document.getElementById('ftx-supply-temp');
            if (s) s.textContent = formatTempC(sensors.supply_temp);
            const e = document.getElementById('ftx-extract-temp');
            if (e) e.textContent = formatTempC(sensors.extract_temp);
            const x = document.getElementById('ftx-exhaust-temp');
            if (x) x.textContent = formatTempC(sensors.exhaust_temp);
        }
        
        if (data.efficiency && mayDisplayMeasurements(data)) {
            const badge = document.getElementById('ftx-badge');
            const pct = toFiniteNumber(data.efficiency.percent);
            if (badge) badge.textContent = pct == null ? 'N/A' : `${pct.toFixed(0)}%`;
        } else {
            const badge = document.getElementById('ftx-badge');
            if (badge) badge.textContent = 'N/A';
        }
        
        // Update status badges
        if (status) {
            updateStatusBadge('status-frost', status.frost_risk, 'Aktiv', 'Inaktiv');
            updateStatusBadge('status-bypass', status.bypass_active, 'Öppen', 'Stängd');
            updateStatusBadge('status-filter', status.filter_warning, 'Byt filter', 'OK', 
                status.filter_warning ? 'warning' : 'ok');
        }
    }
}

async function fetchSensorsDetail() {
    const data = await fetchAPI('/ftx/sensors');
    if (!data) return;
    
    const container = document.getElementById('sensors-detail-grid');
    container.innerHTML = '';

    const sanitized = sanitizeSensorsForDisplay(data, data);
    const layout = getProfileConfig().sensors;
    const sensors = layout.map(sensor => ({
        name: sensor.name,
        icon: sensor.icon,
        temp: sanitized ? sanitized[sensor.tempKey] : null,
        rh: sanitized ? sanitized[sensor.rhKey] : null
    }));
    
    sensors.forEach(sensor => {
        const card = document.createElement('div');
        card.className = 'sensor-detail-card';
        card.innerHTML = `
            <div class="sensor-header">
                <div class="sensor-icon">
                    <i class="fas fa-${sensor.icon}"></i>
                </div>
                <div>
                    <h3>${sensor.name}</h3>
                    <span class="subtitle">Sensor</span>
                </div>
            </div>
            <div class="sensor-data-grid">
                <div class="sensor-data-item">
                    <div class="sensor-data-label">Temperatur</div>
                    <div class="sensor-data-value">${sensor.temp == null ? 'N/A' : `${sensor.temp.toFixed(1)}°C`}</div>
                </div>
                <div class="sensor-data-item">
                    <div class="sensor-data-label">Fuktighet</div>
                    <div class="sensor-data-value">${sensor.rh == null ? 'N/A' : `${sensor.rh.toFixed(1)}%`}</div>
                </div>
            </div>
        `;
        container.appendChild(card);
    });
}

const wifiStateLabels = {
    connected: 'Ansluten',
    ap_mode: 'AP-läge',
    connecting: 'Ansluter...',
    disconnected: 'Frånkopplad',
    unknown: 'Okänd'
};

const otaStateLabels = {
    idle: 'Väntar',
    checking: 'Söker uppdatering',
    downloading: 'Laddar ner',
    verifying: 'Verifierar',
    ready: 'Klar att installera',
    applying: 'Installerar',
    rollback: 'Återställer',
    error: 'Fel',
    unavailable: 'Ej tillgänglig',
    unknown: 'Okänd'
};

function formatFirmwareVersion(data) {
    const version = data.version_full || data.firmware_version;
    if (!version) return '--';
    if (data.channel && data.channel !== 'stable') {
        return `${version} (${data.channel})`;
    }
    return version;
}

function formatBytes(bytes) {
    if (bytes == null || Number.isNaN(Number(bytes))) return '--';
    const n = Number(bytes);
    if (n >= 1024 * 1024) return `${(n / (1024 * 1024)).toFixed(2)} MB`;
    if (n >= 1024) return `${(n / 1024).toFixed(1)} KB`;
    return `${n} B`;
}

function updateMemoryInfo(data) {
    const mem = data.memory || {};
    const freeHeap = mem.free_heap ?? data.free_heap;
    const minFree = mem.min_free_heap ?? data.min_free_heap;
    const largest = mem.largest_free_block;
    const freeInternal = mem.free_internal;
    const totalInternal = mem.total_internal;
    const freePsram = mem.free_psram;
    const totalPsram = mem.total_psram;

    const freeEl = document.getElementById('free-heap');
    const minEl = document.getElementById('min-free-heap');
    const largestEl = document.getElementById('largest-free-block');
    const internalEl = document.getElementById('internal-ram');
    const psramEl = document.getElementById('psram-status');

    if (freeEl) freeEl.textContent = formatBytes(freeHeap);
    if (minEl) minEl.textContent = formatBytes(minFree);
    if (largestEl) largestEl.textContent = formatBytes(largest);
    if (internalEl) {
        if (freeInternal != null && totalInternal != null) {
            internalEl.textContent = `${formatBytes(freeInternal)} / ${formatBytes(totalInternal)} ledigt`;
        } else {
            internalEl.textContent = '--';
        }
    }
    if (psramEl) {
        const available = mem.psram_available;
        if (available && totalPsram && totalPsram > 0) {
            psramEl.textContent = `${formatBytes(freePsram)} / ${formatBytes(totalPsram)} ledigt`;
        } else if (totalPsram && totalPsram > 0) {
            psramEl.textContent = `${formatBytes(totalPsram)} (ej initierat)`;
        } else {
            psramEl.textContent = 'Ej tillgängligt (kör utan PSRAM)';
        }
    }
}

async function fetchDeviceInfo() {
    const data = await fetchAPI('/device/info');
    if (!data) return;

    if (data.docs_url) {
        setDocsBaseUrl(data.docs_url);
    }
    
    const deviceId = data.device_id || data.default_name || '--';
    const displayName = data.device_name || data.name || deviceId;
    document.getElementById('device-id').textContent = deviceId;
    document.getElementById('device-name').textContent = displayName;
    document.getElementById('device-mac').textContent = data.mac_address || '--:--:--:--:--:--';
    document.getElementById('firmware-version').textContent = formatFirmwareVersion(data);
    document.getElementById('device-ip').textContent = data.ip_address || '--';
    document.getElementById('wifi-state').textContent =
        wifiStateLabels[data.wifi_state] || wifiStateLabels.unknown;
    updateMemoryInfo(data);

    const nameInput = document.getElementById('device-name-input');
    if (nameInput && document.activeElement !== nameInput) {
        nameInput.placeholder = data.has_custom_name ? 'Visningsnamn' : 'Samma som enhets-ID';
    }

    if (data.application_profile) {
        applyApplicationProfile(data.application_profile, {
            switchIfNeeded: false,
            control: data.control,
            capabilities: data.capabilities
        });
    }
}

async function saveApplicationProfile(profileId) {
    const response = await fetchAPI('/device/profile', {
        method: 'PUT',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ profile: profileId })
    });

    if (response?.success || response?.application_profile) {
        applyApplicationProfile(response.application_profile || profileId, {
            control: response.control,
            capabilities: response.capabilities
        });
        showToast(`Läge: ${getProfileConfig().label}`, 'success');
        return true;
    }

    if (DEMO_MODE) {
        applyApplicationProfile(profileId);
        showToast(`Demo: ${getProfileConfig().label}`, 'success');
        return true;
    }

    showToast('Kunde inte spara applikationsläge', 'error');
    return false;
}

async function saveControlSettings(partial) {
    const response = await fetchAPI('/device/profile', {
        method: 'PUT',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(partial)
    });
    if (response?.success || response?.control) {
        applyApplicationProfile(response.application_profile || state.applicationProfile, {
            switchIfNeeded: false,
            control: response.control,
            capabilities: response.capabilities
        });
        return true;
    }
    if (DEMO_MODE) {
        state.control = { ...(state.control || {}), ...partial };
        applyControlState({
            enabled: partial.control_enabled ?? state.control?.enabled,
            method: partial.control_method ?? state.control?.method ?? 'none',
            fan_mode: partial.fan_mode ?? state.control?.fan_mode ?? 'auto',
            fan1_speed: partial.fan1_speed ?? state.control?.fan1_speed ?? 0,
            fan2_speed: partial.fan2_speed ?? state.control?.fan2_speed ?? 0,
            cycle_period_s: partial.cycle_period_s ?? state.control?.cycle_period_s ?? 60,
            cycle_phase: state.control?.cycle_phase || 'idle'
        });
        return true;
    }
    return false;
}

const dataSourceLabels = {
    auto: 'Automatiskt',
    simulation: 'Simulering',
    hardware: 'Riktiga sensorer'
};

const sensorStatusLabels = {
    'SIMULATION - Forced by setting': 'Simulering (tvingat läge)',
    'SIMULATION - No sensors detected': 'Simulering (inga sensorer)',
    'HARDWARE - No sensors detected': 'Sensorer (inga hittade)',
    'HARDWARE - Sensors connected': 'Riktiga sensorer'
};

async function fetchHardwareMode() {
    const data = await fetchAPI('/hardware/mode');
    if (!data) return;

    if (data.data_source) {
        state.dataSource = data.data_source;
    }
    if (typeof data.simulation_mode === 'boolean') {
        state.simulationMode = data.simulation_mode;
    }

    const statusEl = document.getElementById('sensor-mode-status');
    const countEl = document.getElementById('sensor-count');
    const hintEl = document.getElementById('sensor-mode-hint');

    if (statusEl) {
        statusEl.textContent = sensorStatusLabels[data.status] || data.status || '--';
    }
    if (countEl) {
        countEl.textContent = data.sensor_count ?? '--';
    }
    if (hintEl) {
        if (data.data_source === 'simulation') {
            hintEl.textContent = 'Simulering: simulerade värden visas i UI. Byt till Sensorer för endast riktiga mätvärden.';
        } else if (data.data_source === 'hardware') {
            hintEl.textContent = 'Endast riktiga sensorer. Saknade eller oläsbara kanaler visas som N/A.';
        } else {
            hintEl.textContent = 'Auto: riktiga sensorer om de finns, annars simulerad data. Sensor-läge visar N/A när en sensor saknas.';
        }
    }

    document.querySelectorAll('#data-source-control .segment').forEach(segment => {
        segment.classList.toggle('active', segment.dataset.source === data.data_source);
    });
}

const logSeverityClass = {
    DEBUG: 'debug',
    INFO: 'info',
    WARN: 'warn',
    ERROR: 'error',
    CRITICAL: 'critical'
};

function formatLogAge(ageS) {
    if (ageS == null || Number.isNaN(ageS)) return '';
    if (ageS < 60) return `${Math.round(ageS)} s sedan`;
    if (ageS < 3600) return `${Math.round(ageS / 60)} min sedan`;
    return `${Math.round(ageS / 3600)} h sedan`;
}

function renderLogs(data) {
    const tbody = document.getElementById('log-table-body');
    const countEl = document.getElementById('log-count');
    const totalEl = document.getElementById('log-total');
    const sinksEl = document.getElementById('log-sinks');
    if (!tbody) return;

    const logs = data?.logs || [];
    countEl.textContent = `${logs.length} händelser i bufferten (kapacitet ${data?.capacity ?? 100})`;
    totalEl.textContent = `Totalt loggade: ${data?.total_logged ?? 0}`;
    if (sinksEl) {
        const sinks = Array.isArray(data?.sinks) ? data.sinks.join(', ') : '--';
        sinksEl.textContent = `Sinks: ${sinks}`;
    }

    if (logs.length === 0) {
        tbody.innerHTML = '<tr class="log-empty"><td colspan="6">Ingen loggdata ännu</td></tr>';
        return;
    }

    tbody.innerHTML = logs.map(entry => {
        const severity = entry.severity || entry.level || 'INFO';
        const badgeClass = logSeverityClass[severity] || 'info';
        const timeLabel = entry.time || '--';
        const ageLabel = formatLogAge(entry.age_s);
        const timeCell = ageLabel ? `${timeLabel}<br><small>${ageLabel}</small>` : timeLabel;
        return `
            <tr>
                <td>${timeCell}</td>
                <td><span class="log-badge ${badgeClass}">${severity}</span></td>
                <td>${entry.category || '--'}</td>
                <td>${entry.component || '--'}</td>
                <td>${entry.event || '--'}</td>
                <td>${entry.message || ''}</td>
            </tr>
        `;
    }).join('');
}

function getDemoLogs() {
    return {
        count: 4,
        total_logged: 42,
        logs: [
            { time: '+00:00:03', age_s: 120, severity: 'INFO', event: 'BOOT', message: 'Boot ThermoFlow 2026.29.39' },
            { time: '+00:00:08', age_s: 115, severity: 'INFO', event: 'NET_CONNECT', message: 'HTTP web server started' },
            { time: '+00:01:12', age_s: 11, severity: 'INFO', event: 'CONFIG_CHANGE', message: 'WiFi credentials updated for SSID S22' },
            { time: '+00:01:45', age_s: 3, severity: 'WARN', event: 'RATE_LIMIT', message: 'Rate limit exceeded for /api/ftx' }
        ]
    };
}

async function fetchLogs(options = {}) {
    const { silent = false } = options;

    if (DEMO_MODE) {
        renderLogs(getDemoLogs());
        return;
    }

    const data = await fetchAPI('/logs');
    if (!data) {
        if (!silent) {
            showToast('Kunde inte hämta loggdata från enheten', 'error');
        }
        return;
    }
    renderLogs(data);
}

async function fetchLogConfig() {
    if (DEMO_MODE) return;
    const data = await fetchAPI('/logs/config');
    if (!data) return;

    const levelEl = document.getElementById('log-min-level');
    if (levelEl && data.min_level) levelEl.value = data.min_level;

    const serialJsonEl = document.getElementById('log-serial-json');
    if (serialJsonEl) serialJsonEl.checked = !!data.serial_json;

    const active = new Set(Array.isArray(data.sinks) ? data.sinks : []);
    document.querySelectorAll('.log-sink').forEach(cb => {
        cb.checked = active.has(cb.value);
    });
}

async function saveLogConfig() {
    const sinks = Array.from(document.querySelectorAll('.log-sink:checked')).map(cb => cb.value);
    const body = {
        min_level: document.getElementById('log-min-level')?.value || 'WARN',
        serial_json: !!document.getElementById('log-serial-json')?.checked,
        sinks
    };

    const response = await fetchAPI('/logs/config', {
        method: 'PUT',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(body)
    });

    if (response?.success) {
        showToast('Loggkonfiguration sparad', 'success');
        await fetchLogs();
    } else {
        showToast('Kunde inte spara loggkonfiguration', 'error');
    }
}

function setupLogs() {
    document.getElementById('refresh-logs')?.addEventListener('click', () => {
        fetchLogs();
        showToast('Logg uppdaterad', 'info');
    });

    document.getElementById('toggle-log-config')?.addEventListener('click', async () => {
        const panel = document.getElementById('log-config-panel');
        if (!panel) return;
        const show = panel.style.display === 'none';
        panel.style.display = show ? 'block' : 'none';
        if (show) await fetchLogConfig();
    });

    document.getElementById('save-log-config')?.addEventListener('click', saveLogConfig);

    document.getElementById('export-logs')?.addEventListener('click', () => {
        if (DEMO_MODE) {
            showToast('Export ej tillgänglig i demo-läge', 'info');
            return;
        }
        window.open('/api/logs/export?format=ndjson', '_blank');
    });

    document.getElementById('clear-logs')?.addEventListener('click', async () => {
        if (!confirm('Rensa systemloggen?')) return;

        if (DEMO_MODE) {
            renderLogs({ count: 0, total_logged: 0, logs: [] });
            showToast('Demo-logg rensad', 'success');
            return;
        }

        const response = await fetchAPI('/logs', { method: 'DELETE' });
        if (response?.success) {
            await fetchLogs();
            showToast('Logg rensad', 'success');
        } else {
            showToast('Kunde inte rensa loggen', 'error');
        }
    });
}

async function fetchOtaStatus() {
    const data = await fetchAPI('/ota/status');
    if (!data) return;

    const stateEl = document.getElementById('ota-state');
    const partitionEl = document.getElementById('ota-partition');
    const hintEl = document.getElementById('ota-hint');

    if (stateEl) {
        stateEl.textContent = otaStateLabels[data.state] || data.state || '--';
    }
    if (partitionEl) {
        partitionEl.textContent = data.partition || '--';
    }
    if (hintEl) {
        hintEl.textContent = data.update_method ||
            'OTA kräver en konfigurerad uppdateringsserver. Flasha via USB tills dess.';
    }
}

// UI Updates
function updateConnectionStatus() {
    const statusEl = document.getElementById('connection-status');
    const dot = statusEl.querySelector('.status-dot');
    const text = statusEl.querySelector('.status-text');
    
    if (state.connected) {
        dot.classList.add('connected');
        text.textContent = 'Ansluten';
    } else {
        dot.classList.remove('connected');
        text.textContent = 'Frånkopplad';
    }
}

function updateGauge(id, value, unit) {
    const element = document.getElementById(`${id}-value`);
    if (!element) return;
    const n = toFiniteNumber(value);
    element.textContent = n == null ? 'N/A' : `${n.toFixed(1)}${unit}`;
}

function updateStatusBadge(id, isActive, activeText, inactiveText, type = null) {
    const badge = document.querySelector(`#${id} .status-badge`);
    if (badge) {
        badge.textContent = isActive ? activeText : inactiveText;
        badge.className = 'status-badge';
        if (type) {
            badge.classList.add(type);
        } else {
            badge.classList.add(isActive ? 'warning' : 'inactive');
        }
    }
}

// Chart Functions
function chartThemeColors() {
    const isDark = document.documentElement.getAttribute('data-theme') === 'dark';
    return {
        gridColor: isDark ? 'rgba(255,255,255,0.1)' : 'rgba(0,0,0,0.1)',
        textColor: isDark ? '#a0a0b0' : '#666'
    };
}

function chartLineDefaults() {
    return {
        tension: 0.3,
        fill: false,
        spanGaps: true,
        pointRadius: 0,
        pointHoverRadius: 4,
        borderWidth: 2
    };
}

function formatHistoryLabel(ts) {
    const d = new Date(ts);
    const short = state.historyRange === '5m' || state.historyRange === '10m';
    return d.toLocaleTimeString('sv-SE', short
        ? { hour: '2-digit', minute: '2-digit', second: '2-digit' }
        : { hour: '2-digit', minute: '2-digit' });
}

function shiftHistorySample() {
    state.historyData.timestamps.shift();
    state.historyData.outdoor.shift();
    state.historyData.indoor.shift();
    state.historyData.extract.shift();
    state.historyData.exhaust.shift();
    state.historyData.coolingDelta.shift();
}

function pruneHistory() {
    const cutoff = Date.now() - HISTORY_KEEP_MS;
    while (state.historyData.timestamps.length > 0 && state.historyData.timestamps[0] < cutoff) {
        shiftHistorySample();
    }
    while (state.historyData.timestamps.length > HISTORY_MAX_POINTS) {
        shiftHistorySample();
    }
}

/** Slice of history visible for the selected range button. */
function getVisibleHistory() {
    const rangeMs = HISTORY_RANGES_MS[state.historyRange] || HISTORY_RANGES_MS['10m'];
    const cutoff = Date.now() - rangeMs;
    const h = state.historyData;
    let start = 0;
    while (start < h.timestamps.length && h.timestamps[start] < cutoff) {
        start++;
    }
    const labels = [];
    for (let i = start; i < h.timestamps.length; i++) {
        labels.push(formatHistoryLabel(h.timestamps[i]));
    }
    return {
        labels,
        outdoor: h.outdoor.slice(start),
        indoor: h.indoor.slice(start),
        extract: h.extract.slice(start),
        exhaust: h.exhaust.slice(start),
        coolingDelta: h.coolingDelta.slice(start)
    };
}

function pushHistoryPoint(sensors, coolingDelta) {
    const now = Date.now();
    /* Avoid duplicate points if two fetch paths race within same second */
    const last = state.historyData.timestamps[state.historyData.timestamps.length - 1];
    if (last != null && now - last < 400) {
        return;
    }

    state.historyData.timestamps.push(now);
    state.historyData.outdoor.push(toFiniteNumber(sensors?.outdoor_temp));
    state.historyData.indoor.push(toFiniteNumber(sensors?.supply_temp));
    state.historyData.extract.push(toFiniteNumber(sensors?.extract_temp));
    state.historyData.exhaust.push(toFiniteNumber(sensors?.exhaust_temp));
    state.historyData.coolingDelta.push(toFiniteNumber(coolingDelta));
    pruneHistory();
}

function applyHistoryToTempChart(visible) {
    if (!tempChart || !visible) return;
    tempChart.data.labels = visible.labels;
    if (isMobileAcProfile() && tempChart.data.datasets.length >= 5) {
        tempChart.data.datasets[0].data = visible.indoor;
        tempChart.data.datasets[1].data = visible.extract;
        tempChart.data.datasets[2].data = visible.exhaust;
        tempChart.data.datasets[3].data = visible.outdoor;
        tempChart.data.datasets[4].data = visible.coolingDelta;
    } else if (tempChart.data.datasets.length >= 2) {
        tempChart.data.datasets[0].data = visible.outdoor;
        tempChart.data.datasets[1].data = visible.indoor;
    }
    tempChart.update('none');
}

function applyHistoryToAcChart(visible) {
    if (!acTempChart || !visible) return;
    acTempChart.data.labels = visible.labels;
    acTempChart.data.datasets[0].data = visible.indoor;
    acTempChart.data.datasets[1].data = visible.extract;
    acTempChart.data.datasets[2].data = visible.exhaust;
    acTempChart.data.datasets[3].data = visible.outdoor;
    acTempChart.data.datasets[4].data = visible.coolingDelta;
    acTempChart.update('none');
}

function syncChartsFromHistory() {
    const visible = getVisibleHistory();
    applyHistoryToTempChart(visible);
    applyHistoryToAcChart(visible);
}

function initChart() {
    const ctx = document.getElementById('temp-chart');
    if (!ctx || typeof Chart === 'undefined') return;

    if (tempChart) {
        tempChart.destroy();
        tempChart = null;
    }

    const { gridColor, textColor } = chartThemeColors();
    const base = chartLineDefaults();
    const ac = isMobileAcProfile();

    tempChart = new Chart(ctx, {
        type: 'line',
        data: {
            labels: [],
            datasets: ac
                ? [
                    { ...base, label: 'Utgående kall', data: [], borderColor: '#38bdf8', backgroundColor: 'rgba(56,189,248,0.08)' },
                    { ...base, label: 'Kallsida intag', data: [], borderColor: '#22d3ee', backgroundColor: 'rgba(34,211,238,0.08)' },
                    { ...base, label: 'Utgående varm', data: [], borderColor: '#f97316', backgroundColor: 'rgba(249,115,22,0.08)' },
                    { ...base, label: 'Varmsida intag', data: [], borderColor: '#fb7185', backgroundColor: 'rgba(251,113,133,0.08)' },
                    { ...base, label: 'Kyllyft °C', data: [], borderColor: '#a78bfa', backgroundColor: 'rgba(167,139,250,0.08)', borderDash: [6, 4] }
                ]
                : [
                    { ...base, label: 'Utomhus', data: [], borderColor: '#4ecdc4', backgroundColor: 'rgba(78, 205, 196, 0.1)', fill: true },
                    { ...base, label: 'Tilluft / inomhus', data: [], borderColor: '#ff6b6b', backgroundColor: 'rgba(255, 107, 107, 0.1)', fill: true }
                ]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            animation: false,
            interaction: { intersect: false, mode: 'index' },
            plugins: {
                legend: { labels: { color: textColor } }
            },
            scales: {
                x: {
                    grid: { color: gridColor },
                    ticks: { color: textColor, maxRotation: 0, autoSkip: true, maxTicksLimit: 8 }
                },
                y: { grid: { color: gridColor }, ticks: { color: textColor } }
            }
        }
    });

    initAcChart();
}

function initAcChart() {
    const ctx = document.getElementById('ac-temp-chart');
    if (!ctx || typeof Chart === 'undefined') return;

    if (acTempChart) {
        acTempChart.destroy();
        acTempChart = null;
    }

    const { gridColor, textColor } = chartThemeColors();
    const base = chartLineDefaults();
    acTempChart = new Chart(ctx, {
        type: 'line',
        data: {
            labels: [],
            datasets: [
                { ...base, label: 'Utgående kall', data: [], borderColor: '#38bdf8' },
                { ...base, label: 'Kallsida intag', data: [], borderColor: '#22d3ee' },
                { ...base, label: 'Utgående varm', data: [], borderColor: '#f97316' },
                { ...base, label: 'Varmsida intag', data: [], borderColor: '#fb7185' },
                { ...base, label: 'Kyllyft °C', data: [], borderColor: '#a78bfa', borderDash: [5, 4] }
            ]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            animation: false,
            interaction: { intersect: false, mode: 'index' },
            plugins: { legend: { labels: { color: textColor } } },
            scales: {
                x: {
                    grid: { color: gridColor },
                    ticks: { color: textColor, maxRotation: 0, autoSkip: true, maxTicksLimit: 8 }
                },
                y: { grid: { color: gridColor }, ticks: { color: textColor } }
            }
        }
    });
}

function updateChart(data) {
    if (!data?.sensors) return;
    if (!tempChart) {
        initChart();
    }

    const sensors = data.sensors;
    const m = isMobileAcProfile() ? computeAcMetrics(sensors) : null;
    pushHistoryPoint(sensors, m?.coolingDelta);
    syncChartsFromHistory();
}

function updateAcChart(sensors, metrics) {
    if (!acTempChart) {
        initAcChart();
    }
    /* History is advanced by dashboard updateChart; only re-sync AC canvas */
    syncChartsFromHistory();
    void sensors;
    void metrics;
}

function setupHistoryRangeControls() {
    const root = document.getElementById('history-range-controls');
    if (!root) return;

    if (!HISTORY_RANGES_MS[state.historyRange]) {
        state.historyRange = '10m';
    }

    root.querySelectorAll('[data-range]').forEach(btn => {
        btn.classList.toggle('active', btn.dataset.range === state.historyRange);
        btn.addEventListener('click', () => {
            const range = btn.dataset.range;
            if (!HISTORY_RANGES_MS[range]) return;
            state.historyRange = range;
            localStorage.setItem('historyRange', range);
            root.querySelectorAll('[data-range]').forEach(b => {
                b.classList.toggle('active', b.dataset.range === range);
            });
            syncChartsFromHistory();
        });
    });
}

// Sliders + control panel
function setupSliders() {
    const supplyFan = document.getElementById('supply-fan');
    const exhaustFan = document.getElementById('exhaust-fan');
    const cyclePeriod = document.getElementById('cycle-period');
    const controlEnabled = document.getElementById('control-enabled');
    const controlMethod = document.getElementById('control-method');

    if (supplyFan) {
        supplyFan.addEventListener('input', (e) => {
            document.getElementById('supply-fan-value').textContent = `${e.target.value}%`;
        });
        supplyFan.addEventListener('change', async (e) => {
            const ok = await saveControlSettings({
                control_enabled: true,
                fan_mode: 'manual',
                fan1_speed: parseInt(e.target.value, 10)
            });
            showToast(ok ? `Fläkt 1: ${e.target.value}%` : 'Kunde inte spara fläkthastighet',
                ok ? 'success' : 'error');
        });
    }

    if (exhaustFan) {
        exhaustFan.addEventListener('input', (e) => {
            document.getElementById('exhaust-fan-value').textContent = `${e.target.value}%`;
        });
        exhaustFan.addEventListener('change', async (e) => {
            const ok = await saveControlSettings({
                control_enabled: true,
                fan_mode: 'manual',
                fan2_speed: parseInt(e.target.value, 10)
            });
            showToast(ok ? `Fläkt 2: ${e.target.value}%` : 'Kunde inte spara fläkthastighet',
                ok ? 'success' : 'error');
        });
    }

    if (cyclePeriod) {
        cyclePeriod.addEventListener('input', (e) => {
            document.getElementById('cycle-period-value').textContent = `${e.target.value} s`;
        });
        cyclePeriod.addEventListener('change', async (e) => {
            const ok = await saveControlSettings({ cycle_period_s: parseInt(e.target.value, 10) });
            showToast(ok ? `Cykel: ${e.target.value} s` : 'Kunde inte spara cykel', ok ? 'success' : 'error');
        });
    }

    controlEnabled?.addEventListener('change', async (e) => {
        const ok = await saveControlSettings({ control_enabled: e.target.checked });
        showToast(ok
            ? (e.target.checked ? 'Styrning på' : 'Styrning av — endast mätning')
            : 'Kunde inte ändra styrning', ok ? 'success' : 'error');
    });

    controlMethod?.addEventListener('change', async (e) => {
        const ok = await saveControlSettings({
            control_method: e.target.value,
            control_enabled: e.target.value !== 'none'
        });
        showToast(ok ? `Styrsätt: ${e.target.value}` : 'Kunde inte spara styrsätt', ok ? 'success' : 'error');
    });

    document.querySelectorAll('#fan-mode-toggle .btn-toggle').forEach(btn => {
        btn.addEventListener('click', async () => {
            const mode = btn.dataset.mode;
            const ok = await saveControlSettings({
                fan_mode: mode,
                control_enabled: mode !== 'off'
            });
            if (ok) {
                document.querySelectorAll('#fan-mode-toggle .btn-toggle').forEach(b => b.classList.remove('active'));
                btn.classList.add('active');
                showToast(`Fläktläge: ${btn.textContent}`, 'success');
            } else {
                showToast('Kunde inte byta fläktläge', 'error');
            }
        });
    });

    const sendAcCmd = async (command, value = 1) => {
        const response = await fetchAPI('/ftx/control', {
            method: 'POST',
            body: JSON.stringify({ command, value })
        });
        if (response?.control) {
            applyControlState(response.control);
        }
        showToast(response ? `Kommando: ${command}` : 'Kunde inte skicka', response ? 'info' : 'error');
    };
    document.getElementById('ac-cmd-power')?.addEventListener('click', () => sendAcCmd('power', 1));
    document.getElementById('ac-cmd-cool')?.addEventListener('click', () => sendAcCmd('cool', 1));
    document.getElementById('ac-cmd-fan')?.addEventListener('click', () => sendAcCmd('fan_only', 1));

    const bindAssist = (id, valueId, key) => {
        const el = document.getElementById(id);
        el?.addEventListener('input', (e) => {
            const v = document.getElementById(valueId);
            if (v) v.textContent = `${e.target.value}%`;
        });
        el?.addEventListener('change', async (e) => {
            const body = { fan_mode: 'manual' };
            body[key] = parseInt(e.target.value, 10);
            const ok = await saveControlSettings(body);
            showToast(ok ? 'Manuellt börvärde sparat' : 'Kunde inte spara', ok ? 'success' : 'error');
        });
    };
    bindAssist('ac-assist-fan1', 'ac-assist-fan1-value', 'fan1_speed');
    bindAssist('ac-assist-fan2', 'ac-assist-fan2-value', 'fan2_speed');

    document.querySelectorAll('#ac-run-mode-toggle .btn-toggle').forEach(btn => {
        btn.addEventListener('click', async () => {
            const mode = btn.dataset.acRun;
            const ok = await saveControlSettings({ fan_mode: mode });
            if (ok) {
                document.querySelectorAll('#ac-run-mode-toggle .btn-toggle').forEach(b => b.classList.remove('active'));
                btn.classList.add('active');
                showToast(mode === 'auto' ? 'Automatisk fläktreglering' : 'Manuell fläktreglering', 'success');
                fetchDeviceInfo();
            } else {
                showToast('Kunde inte byta reglerläge', 'error');
            }
        });
    });

    document.getElementById('ac-cond-action')?.addEventListener('change', async (e) => {
        const ok = await saveControlSettings({ ac_cond_action: e.target.value });
        showToast(ok ? 'Kondenspolicy sparad' : 'Kunde inte spara policy', ok ? 'success' : 'error');
        if (ok) fetchDeviceInfo();
    });

    document.querySelectorAll('#ac-subtabs .subtab').forEach(btn => {
        btn.addEventListener('click', () => switchAcTab(btn.dataset.acTab));
    });

    document.getElementById('save-ac-modules')?.addEventListener('click', async () => {
        const ac_modules = {
            sensing: true,
            ir_remote: !!document.getElementById('ac-mod-ir')?.checked,
            line_control: !!document.getElementById('ac-mod-line')?.checked,
            assist_fans: !!document.getElementById('ac-mod-assist')?.checked
        };
        const ok = await saveControlSettings({ ac_modules });
        showToast(ok ? 'AC-tillval sparade' : 'Kunde inte spara tillval (kräver läge Mobil AC)', ok ? 'success' : 'error');
        if (ok) fetchDeviceInfo();
    });
}

// Settings
function setupSettings() {
    applyApplicationProfile(state.applicationProfile, { switchIfNeeded: false });

    const profileSelect = document.getElementById('application-profile');
    profileSelect?.addEventListener('change', async (e) => {
        const previous = state.applicationProfile;
        const next = e.target.value;
        if (next === previous) return;

        const saved = await saveApplicationProfile(next);
        if (!saved) {
            e.target.value = previous;
        }
    });

    // Segmented control for theme
    document.querySelectorAll('.segment').forEach(segment => {
        segment.addEventListener('click', () => {
            document.querySelectorAll('.segment').forEach(s => s.classList.remove('active'));
            segment.classList.add('active');
            state.theme = segment.dataset.theme;
            localStorage.setItem('theme', state.theme);
            applyTheme(state.theme);
        });
    });
    
    // Update interval slider
    const intervalSlider = document.getElementById('update-interval');
    const intervalValue = document.getElementById('update-interval-value');
    
    if (intervalSlider) {
        intervalSlider.value = state.updateInterval / 1000;
        intervalValue.textContent = `${intervalSlider.value}s`;
        
        intervalSlider.addEventListener('input', (e) => {
            intervalValue.textContent = `${e.target.value}s`;
        });
        
        intervalSlider.addEventListener('change', (e) => {
            state.updateInterval = parseInt(e.target.value) * 1000;
            localStorage.setItem('updateInterval', state.updateInterval);
            startPolling();
            showToast(`Uppdateringsintervall: ${e.target.value}s`, 'success');
        });
    }
    
    // Sensor data source
    document.querySelectorAll('#data-source-control .segment').forEach(segment => {
        segment.addEventListener('click', async () => {
            const source = segment.dataset.source;
            const response = await fetchAPI('/hardware/mode', {
                method: 'POST',
                body: JSON.stringify({ data_source: source })
            });

            if (response?.success) {
                document.querySelectorAll('#data-source-control .segment').forEach(s => {
                    s.classList.toggle('active', s.dataset.source === source);
                });
                await fetchHardwareMode();
                showToast(`Datakälla: ${dataSourceLabels[source] || source}`, 'success');
            } else {
                showToast('Kunde inte ändra datakälla', 'error');
            }
        });
    });

    // Device name editing
    const nameForm = document.getElementById('device-name-form');
    const nameInput = document.getElementById('device-name-input');
    const editBtn = document.getElementById('edit-device-name');
    const saveBtn = document.getElementById('save-device-name');
    const cancelBtn = document.getElementById('cancel-device-name');

    editBtn?.addEventListener('click', () => {
        const deviceId = document.getElementById('device-id')?.textContent;
        const currentName = document.getElementById('device-name').textContent;
        const sameAsId = currentName === deviceId || currentName === '--';
        nameInput.value = sameAsId ? '' : currentName;
        nameForm.classList.remove('hidden');
        nameInput.focus();
    });

    cancelBtn?.addEventListener('click', () => {
        nameForm.classList.add('hidden');
        nameInput.value = '';
    });

    saveBtn?.addEventListener('click', async () => {
        const newName = nameInput.value.trim();
        if (!newName) {
            showToast('Ange ett visningsnamn', 'warning');
            return;
        }
        if (/^ThermoFlow-[0-9A-Fa-f]{4}$/i.test(newName)) {
            showToast('ThermoFlow-XXXX är reserverat för enhets-ID', 'warning');
            return;
        }

        const response = await fetchAPI('/device/name', {
            method: 'POST',
            body: JSON.stringify({ device_name: newName })
        });

        if (response?.success) {
            document.getElementById('device-name').textContent = response.device_name || newName;
            nameForm.classList.add('hidden');
            fetchDeviceInfo();
            showToast('Visningsnamn uppdaterat', 'success');
        } else {
            showToast('Kunde inte spara enhetsnamn', 'error');
        }
    });

    // Danger buttons
    document.getElementById('reset-wifi')?.addEventListener('click', async () => {
        if (confirm('Är du säker på att du vill återställa WiFi-konfigurationen?')) {
            const response = await fetchAPI('/wifi/config', { method: 'DELETE' });
            if (response) {
                showToast('WiFi återställt. Enheten startar om...', 'success');
            }
        }
    });
    
    document.getElementById('restart-device')?.addEventListener('click', async () => {
        if (confirm('Är du säker på att du vill starta om enheten?')) {
            showToast('Startar om...', 'info');
            await fetchAPI('/device/restart', { method: 'POST' });
        }
    });
}

// Chart range buttons
document.querySelectorAll('.btn-sm').forEach(btn => {
    btn.addEventListener('click', () => {
        document.querySelectorAll('.btn-sm').forEach(b => b.classList.remove('active'));
        btn.classList.add('active');
    });
});

// Keyboard shortcuts
document.addEventListener('keydown', (e) => {
    if (e.ctrlKey || e.metaKey) {
        switch (e.key) {
            case '1':
                e.preventDefault();
                switchView('dashboard');
                break;
            case '2':
                e.preventDefault();
                switchView('sensors');
                break;
            case '3':
                e.preventDefault();
                switchView('ftx');
                break;
            case '4':
                e.preventDefault();
                switchView('settings');
                break;
            case 'r':
                e.preventDefault();
                fetchAll();
                showToast('Uppdaterar...', 'info');
                break;
        }
    }
});

// Service Worker registration for PWA
if ('serviceWorker' in navigator) {
    navigator.serviceWorker.register('/sw.js').catch(console.error);
}
