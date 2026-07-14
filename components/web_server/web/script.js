/**
 * ThermoFlow Modern Web Interface
 * SPA with real-time updates, charts, and theme support
 */

// Configuration
const API_BASE = '/api';
const DEFAULT_UPDATE_INTERVAL = 5000;
const DEMO_MODE = new URLSearchParams(window.location.search).has('demo') ||
    localStorage.getItem('thermoflowDemo') === '1';

// State
const state = {
    connected: false,
    currentView: 'dashboard',
    theme: localStorage.getItem('theme') || 'dark',
    updateInterval: parseInt(localStorage.getItem('updateInterval')) || DEFAULT_UPDATE_INTERVAL,
    historyData: {
        labels: [],
        outdoor: [],
        indoor: []
    },
    sensors: [],
    fans: [],
    ftx: null
};

// Chart instances
let tempChart = null;

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
    
    // Setup chart
    initChart();
    
    // Setup sliders
    setupSliders();
    
    // Setup settings
    setupSettings();
    setupLogs();
    
    // Start data fetching
    fetchAll();
    setInterval(fetchAll, state.updateInterval);
    
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

function switchView(viewName) {
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
    } else if (viewName === 'ftx') {
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

    const outdoorTemp = 2 + 9 * Math.sin(dayPhase - 1.2) + hourWobble + noise;
    const extractTemp = 20.5 + 1.2 * Math.sin(dayPhase + 0.4) + noise * 0.5;
    const exhaustTemp = extractTemp + 1.8;
    const supplyTemp = outdoorTemp + (exhaustTemp - outdoorTemp) * 0.82;

    const outdoorRh = 78 - 18 * Math.sin(dayPhase);
    const extractRh = 42 + 6 * Math.sin(dayPhase + 0.8);
    const exhaustRh = extractRh - 4;
    const supplyRh = outdoorRh - 12;
    const efficiency = Math.max(0, Math.min(100,
        ((supplyTemp - outdoorTemp) / (exhaustTemp - outdoorTemp)) * 100));
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
            exhaust_rh: exhaustRh
        },
        efficiency: {
            percent: efficiency,
            power_recovered_w: Math.max(0, (supplyTemp - outdoorTemp) * 18),
            airflow_m3h: 120
        },
        fans: {
            supply: fanSpeed,
            exhaust: fanSpeed
        }
    };
}

function mockDemoApi(endpoint, options = {}) {
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
                ip_address: '127.0.0.1',
                wifi_state: 'connected',
                simulation_mode: true
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
    if (data.valid && data.sensors) return data;

    if (data.outdoor_temp !== undefined) {
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
                exhaust_rh: data.exhaust_rh
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

async function fetchAll() {
    await Promise.all([
        fetchDashboard(),
        fetchSensors(),
        fetchFTX(),
        fetchDeviceInfo(),
        fetchOtaStatus(),
        fetchHardwareMode()
    ]);
    
    state.connected = true;
    updateConnectionStatus();
}

async function fetchDashboard() {
    const raw = await fetchAPI('/ftx');
    const data = normalizeFtxData(raw);
    if (!data) return;
    
    state.ftx = data;
    
    // Update stats
    if (data.sensors) {
        updateGauge('outdoor-temp', data.sensors.outdoor_temp, '°C');
        updateGauge('indoor-temp', data.sensors.supply_temp, '°C');
        updateGauge('humidity', data.sensors.supply_rh, '%');
        
        document.getElementById('avg-temp').textContent = 
            ((data.sensors.outdoor_temp + data.sensors.supply_temp) / 2).toFixed(1);
        document.getElementById('avg-humidity').textContent = data.sensors.supply_rh.toFixed(1);
    }
    
    if (data.efficiency) {
        document.getElementById('ftx-efficiency').textContent = data.efficiency.percent.toFixed(1);
    }
    
    if (data.efficiency && data.efficiency.power_recovered_w) {
        document.getElementById('power-saved').textContent =
            Math.round(data.efficiency.power_recovered_w);
    } else if (data.fans) {
        document.getElementById('power-saved').textContent =
            Math.round(data.fans.supply * 0.5);
    }
    
    // Update chart
    updateChart(data);
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
        // Update flow diagram
        if (data.sensors) {
            document.getElementById('ftx-outdoor-temp').textContent = 
                `${data.sensors.outdoor_temp.toFixed(1)}°C`;
            document.getElementById('ftx-supply-temp').textContent = 
                `${data.sensors.supply_temp.toFixed(1)}°C`;
            document.getElementById('ftx-extract-temp').textContent = 
                `${data.sensors.extract_temp.toFixed(1)}°C`;
            document.getElementById('ftx-exhaust-temp').textContent = 
                `${data.sensors.exhaust_temp.toFixed(1)}°C`;
        }
        
        if (data.efficiency) {
            document.getElementById('ftx-badge').textContent = 
                `${data.efficiency.percent.toFixed(0)}%`;
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
    
    const sensors = [
        { name: 'Utomhus', icon: 'cloud-sun', temp: data.outdoor_temp, rh: data.outdoor_rh },
        { name: 'Tilluft', icon: 'wind', temp: data.supply_temp, rh: data.supply_rh },
        { name: 'Frånluft', icon: 'home', temp: data.extract_temp, rh: data.extract_rh },
        { name: 'Avluft', icon: 'sign-out-alt', temp: data.exhaust_temp, rh: data.exhaust_rh }
    ];
    
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
                    <div class="sensor-data-value">${sensor.temp.toFixed(1)}°C</div>
                </div>
                <div class="sensor-data-item">
                    <div class="sensor-data-label">Fuktighet</div>
                    <div class="sensor-data-value">${sensor.rh.toFixed(1)}%</div>
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

async function fetchDeviceInfo() {
    const data = await fetchAPI('/device/info');
    if (!data) return;
    
    const deviceId = data.device_id || data.default_name || '--';
    const displayName = data.device_name || data.name || deviceId;
    document.getElementById('device-id').textContent = deviceId;
    document.getElementById('device-name').textContent = displayName;
    document.getElementById('device-mac').textContent = data.mac_address || '--:--:--:--:--:--';
    document.getElementById('firmware-version').textContent = formatFirmwareVersion(data);
    document.getElementById('device-ip').textContent = data.ip_address || '--';
    document.getElementById('wifi-state').textContent =
        wifiStateLabels[data.wifi_state] || wifiStateLabels.unknown;

    const nameInput = document.getElementById('device-name-input');
    if (nameInput && document.activeElement !== nameInput) {
        nameInput.placeholder = data.has_custom_name ? 'Visningsnamn' : 'Samma som enhets-ID';
    }
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
            hintEl.textContent = 'Simulerad sensordata används oavsett om sensorer är anslutna.';
        } else if (data.data_source === 'hardware') {
            hintEl.textContent = 'Endast riktiga sensorer används. Utan sensorer blir avläsningarna ogiltiga.';
        } else {
            hintEl.textContent = 'Auto använder riktiga sensorer om de finns, annars simulerad data.';
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

async function fetchLogs() {
    if (DEMO_MODE) {
        renderLogs(getDemoLogs());
        return;
    }

    const data = await fetchAPI('/logs');
    if (!data) return;
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
        min_level: document.getElementById('log-min-level')?.value || 'INFO',
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
    if (element) {
        element.textContent = `${value.toFixed(1)}${unit}`;
    }
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
function initChart() {
    const ctx = document.getElementById('temp-chart');
    if (!ctx) return;
    
    const isDark = document.documentElement.getAttribute('data-theme') === 'dark';
    const gridColor = isDark ? 'rgba(255,255,255,0.1)' : 'rgba(0,0,0,0.1)';
    const textColor = isDark ? '#a0a0b0' : '#666';
    
    tempChart = new Chart(ctx, {
        type: 'line',
        data: {
            labels: [],
            datasets: [
                {
                    label: 'Utomhus',
                    data: [],
                    borderColor: '#4ecdc4',
                    backgroundColor: 'rgba(78, 205, 196, 0.1)',
                    tension: 0.4,
                    fill: true
                },
                {
                    label: 'Inomhus',
                    data: [],
                    borderColor: '#ff6b6b',
                    backgroundColor: 'rgba(255, 107, 107, 0.1)',
                    tension: 0.4,
                    fill: true
                }
            ]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            interaction: {
                intersect: false,
                mode: 'index'
            },
            plugins: {
                legend: {
                    labels: {
                        color: textColor
                    }
                }
            },
            scales: {
                x: {
                    grid: {
                        color: gridColor
                    },
                    ticks: {
                        color: textColor
                    }
                },
                y: {
                    grid: {
                        color: gridColor
                    },
                    ticks: {
                        color: textColor
                    }
                }
            }
        }
    });
}

function updateChart(data) {
    if (!tempChart || !data.sensors) return;
    
    // Add new data point
    const now = new Date().toLocaleTimeString('sv-SE', { hour: '2-digit', minute: '2-digit' });
    
    state.historyData.labels.push(now);
    state.historyData.outdoor.push(data.sensors.outdoor_temp);
    state.historyData.indoor.push(data.sensors.supply_temp);
    
    // Keep only last 20 points
    if (state.historyData.labels.length > 20) {
        state.historyData.labels.shift();
        state.historyData.outdoor.shift();
        state.historyData.indoor.shift();
    }
    
    tempChart.data.labels = state.historyData.labels;
    tempChart.data.datasets[0].data = state.historyData.outdoor;
    tempChart.data.datasets[1].data = state.historyData.indoor;
    tempChart.update('none');
}

// Sliders
function setupSliders() {
    const supplyFan = document.getElementById('supply-fan');
    const exhaustFan = document.getElementById('exhaust-fan');
    
    if (supplyFan) {
        supplyFan.addEventListener('input', (e) => {
            document.getElementById('supply-fan-value').textContent = `${e.target.value}%`;
        });
        supplyFan.addEventListener('change', (e) => {
            setFanSpeed('supply', e.target.value);
        });
    }
    
    if (exhaustFan) {
        exhaustFan.addEventListener('input', (e) => {
            document.getElementById('exhaust-fan-value').textContent = `${e.target.value}%`;
        });
        exhaustFan.addEventListener('change', (e) => {
            setFanSpeed('exhaust', e.target.value);
        });
    }
    
    // Mode toggle
    document.querySelectorAll('.btn-toggle').forEach(btn => {
        btn.addEventListener('click', () => {
            document.querySelectorAll('.btn-toggle').forEach(b => b.classList.remove('active'));
            btn.classList.add('active');
            showToast(`Läge: ${btn.textContent}`, 'info');
        });
    });
}

async function setFanSpeed(fan, speed) {
    try {
        const response = await fetchAPI('/ftx/control', {
            method: 'POST',
            body: JSON.stringify({
                command: 'set_fan',
                fan: fan,
                value: parseInt(speed)
            })
        });
        
        if (response) {
            showToast(`${fan === 'supply' ? 'Tillufts' : 'Frånlufts'}fläkt satt till ${speed}%`, 'success');
        }
    } catch (error) {
        showToast('Kunde inte ändra fläkthastighet', 'error');
    }
}

// Settings
function setupSettings() {
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
