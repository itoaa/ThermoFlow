/**
 * ThermoFlow Modern Web Interface
 * SPA with real-time updates, charts, and theme support
 */

// Configuration
const API_BASE = '/api';
const DEFAULT_UPDATE_INTERVAL = 5000;

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

// API Functions
async function fetchAPI(endpoint, options = {}) {
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
        fetchDeviceInfo()
    ]);
    
    state.connected = true;
    updateConnectionStatus();
}

async function fetchDashboard() {
    const data = await fetchAPI('/ftx');
    if (!data || !data.valid) return;
    
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
    
    if (data.fans) {
        document.getElementById('power-saved').textContent = 
            Math.round(data.fans.supply * 0.5); // Mock calculation
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
    const data = await fetchAPI('/ftx');
    const status = await fetchAPI('/ftx/status');
    
    if (data && data.valid) {
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

async function fetchDeviceInfo() {
    const data = await fetchAPI('/device/info');
    if (!data) return;
    
    document.getElementById('device-name').textContent = data.device_name || 'ThermoFlow';
    document.getElementById('device-mac').textContent = data.mac_address || '--:--:--:--:--:--';
    document.getElementById('firmware-version').textContent = data.firmware_version || '--';
    document.getElementById('device-ip').textContent = data.ip_address || '--';
    document.getElementById('wifi-state').textContent = 
        data.wifi_state === 'connected' ? 'Ansluten' : 
        data.wifi_state === 'ap_mode' ? 'AP-läge' : 'Frånkopplad';
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
            // Device restart would happen here
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
