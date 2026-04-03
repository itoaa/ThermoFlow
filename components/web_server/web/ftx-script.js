/**
 * @file ftx-script.js
 * @brief Mini-FTX Web UI JavaScript with MQTT Integration
 */

// MQTT Configuration
const MQTT_CONFIG = {
    broker: window.location.hostname, // Same host as web server
    port: 9001,                      // WebSocket port (commonly 9001)
    clientId: 'thermoflow_web_' + Math.random().toString(16).substr(2, 8),
    topics: {
        sensors: 'thermoflow/ftx/sensors',
        efficiency: 'thermoflow/ftx/efficiency',
        status: 'thermoflow/ftx/status',
        state: 'thermoflow/ftx/state',
        daily: 'thermoflow/ftx/stats/daily'
    },
    useMqtt: true,    // Enable MQTT
    fallbackToApi: true  // Fallback to REST API if MQTT fails
};

// MQTT Client
let mqttClient = null;
let mqttConnected = false;

// FTX Data State
let ftxData = {
    sensors: {
        outdoor: { temp: 0.5, rh: 65 },
        supply: { temp: 18.7, rh: 42 },
        exhaust: { temp: 22.0, rh: 48 },
        extract: { temp: 21.5, rh: 50 }
    },
    efficiency: 84.7,
    powerRecovered: 942,
    dailySaving: 22.6,
    costSaving: 45.2,
    airflow: 150,
    mode: 'auto',
    fanSpeed: { supply: 50, exhaust: 50 },
    status: {
        frostRisk: false,
        frostProtection: false,
        bypass: false,
        filterWarning: false,
        highHumidity: false
    },
    filterHoursRemaining: 2160
};

// Energy history for chart (7 days)
const energyHistory = [
    { day: 'Mån', kwh: 21.2, cost: 42 },
    { day: 'Tis', kwh: 20.8, cost: 42 },
    { day: 'Ons', kwh: 22.1, cost: 44 },
    { day: 'Tor', kwh: 23.5, cost: 47 },
    { day: 'Fre', kwh: 22.3, cost: 45 },
    { day: 'Lör', kwh: 21.9, cost: 44 },
    { day: 'Sön', kwh: 22.6, cost: 45 }
];

// Initialize on page load
document.addEventListener('DOMContentLoaded', function() {
    initGauge();
    updateUI();
    initEventListeners();
    initChart();
    
    // Try to connect via MQTT first
    if (MQTT_CONFIG.useMqtt) {
        initMqttConnection();
    }
    
    // Fallback: fetch from REST API periodically
    setInterval(fetchFromApi, 5000);
});

// ==================== MQTT INTEGRATION ====================

// Initialize MQTT Connection via WebSocket
function initMqttConnection() {
    if (typeof mqtt === 'undefined') {
        console.warn('MQTT library not loaded, using REST API fallback');
        MQTT_CONFIG.useMqtt = false;
        return;
    }
    
    const wsUrl = `ws://${MQTT_CONFIG.broker}:${MQTT_CONFIG.port}/mqtt`;
    
    mqttClient = mqtt.connect(wsUrl, {
        clientId: MQTT_CONFIG.clientId,
        reconnectPeriod: 5000,
        connectTimeout: 30000
    });
    
    mqttClient.on('connect', function() {
        console.log('MQTT Connected');
        mqttConnected = true;
        updateConnectionStatus('mqtt', true);
        
        // Subscribe to FTX topics
        Object.values(MQTT_CONFIG.topics).forEach(topic => {
            mqttClient.subscribe(topic, function(err) {
                if (err) {
                    console.error('Failed to subscribe to', topic, err);
                } else {
                    console.log('Subscribed to', topic);
                }
            });
        });
    });
    
    mqttClient.on('message', function(topic, message) {
        try {
            const data = JSON.parse(message.toString());
            handleMqttMessage(topic, data);
        } catch (e) {
            console.error('Failed to parse MQTT message:', e);
        }
    });
    
    mqttClient.on('error', function(err) {
        console.error('MQTT Error:', err);
        mqttConnected = false;
        updateConnectionStatus('mqtt', false);
    });
    
    mqttClient.on('close', function() {
        console.log('MQTT Disconnected');
        mqttConnected = false;
        updateConnectionStatus('mqtt', false);
    });
}

// Handle incoming MQTT messages
function handleMqttMessage(topic, data) {
    let updated = false;
    
    if (topic === MQTT_CONFIG.topics.sensors) {
        // Update sensor data
        if (data.outdoor_temp !== undefined) {
            ftxData.sensors.outdoor.temp = data.outdoor_temp;
            ftxData.sensors.outdoor.rh = data.outdoor_rh;
            ftxData.sensors.supply.temp = data.supply_temp;
            ftxData.sensors.supply.rh = data.supply_rh;
            ftxData.sensors.exhaust.temp = data.exhaust_temp;
            ftxData.sensors.exhaust.rh = data.exhaust_rh;
            updated = true;
        }
    } else if (topic === MQTT_CONFIG.topics.efficiency) {
        // Update efficiency
        if (data.efficiency_percent !== undefined) {
            ftxData.efficiency = data.efficiency_percent;
            ftxData.powerRecovered = data.power_recovered_w || ftxData.powerRecovered;
            ftxData.airflow = data.airflow_m3h || ftxData.airflow;
            updated = true;
        }
    } else if (topic === MQTT_CONFIG.topics.status) {
        // Update status
        if (data.frost_risk !== undefined) {
            ftxData.status.frostRisk = data.frost_risk;
            ftxData.status.frostProtection = data.frost_protection_active;
            ftxData.status.bypass = data.bypass_active;
            ftxData.status.filterWarning = data.filter_warning;
            ftxData.status.highHumidity = data.high_humidity_alert;
            updated = true;
        }
    } else if (topic === MQTT_CONFIG.topics.state) {
        // Full state update
        if (data.sensors) {
            ftxData.sensors.outdoor.temp = data.sensors.outdoor_temp;
            ftxData.sensors.outdoor.rh = data.sensors.outdoor_rh;
            ftxData.sensors.supply.temp = data.sensors.supply_temp;
            ftxData.sensors.supply.rh = data.sensors.supply_rh;
            ftxData.sensors.exhaust.temp = data.sensors.exhaust_temp;
            ftxData.sensors.exhaust.rh = data.sensors.exhaust_rh;
        }
        if (data.efficiency) {
            ftxData.efficiency = data.efficiency.percent;
            ftxData.powerRecovered = data.efficiency.power_w;
            ftxData.airflow = data.efficiency.airflow_m3h;
        }
        if (data.status) {
            ftxData.status.frostRisk = data.status.frost_risk;
            ftxData.status.bypass = data.status.bypass;
            ftxData.status.filterWarning = data.status.filter_warning;
        }
        updated = true;
    } else if (topic === MQTT_CONFIG.topics.daily) {
        // Update daily stats
        if (data.energy_kwh_day !== undefined) {
            ftxData.dailySaving = data.energy_kwh_day;
            ftxData.costSaving = data.cost_saving_sek;
            updated = true;
        }
    }
    
    if (updated) {
        updateUI();
    }
}

// Update connection status indicator
function updateConnectionStatus(type, connected) {
    const statusEl = document.getElementById('system-status');
    if (!statusEl) return;
    
    if (connected) {
        statusEl.className = 'status connected';
        statusEl.textContent = 'Ansluten (MQTT)';
    } else if (MQTT_CONFIG.fallbackToApi) {
        statusEl.className = 'status warning';
        statusEl.textContent = 'Ansluten (HTTP)';
    } else {
        statusEl.className = 'status disconnected';
        statusEl.textContent = 'Frånkopplad';
    }
}

// ==================== REST API FALLBACK ====================

// Fetch data from REST API
async function fetchFromApi() {
    if (mqttConnected) return; // Skip if MQTT is working
    
    try {
        const response = await fetch('/api/ftx');
        if (!response.ok) throw new Error('API Error');
        
        const data = await response.json();
        if (data.valid) {
            updateDataFromApi(data);
            updateUI();
            updateConnectionStatus('http', true);
        }
    } catch (err) {
        console.error('Failed to fetch from API:', err);
        updateConnectionStatus('http', false);
    }
}

// Update internal data from API response
function updateDataFromApi(data) {
    if (data.sensors) {
        ftxData.sensors.outdoor.temp = data.sensors.outdoor_temp;
        ftxData.sensors.outdoor.rh = data.sensors.outdoor_rh;
        ftxData.sensors.supply.temp = data.sensors.supply_temp;
        ftxData.sensors.supply.rh = data.sensors.supply_rh;
        ftxData.sensors.exhaust.temp = data.sensors.exhaust_temp;
        ftxData.sensors.exhaust.rh = data.sensors.exhaust_rh;
    }
    
    if (data.efficiency) {
        ftxData.efficiency = data.efficiency.percent;
        ftxData.airflow = data.efficiency.airflow;
    }
    
    if (data.fans) {
        ftxData.fanSpeed.supply = data.fans.supply;
        ftxData.fanSpeed.exhaust = data.fans.exhaust;
    }
    
    if (data.mode) {
        ftxData.mode = data.mode;
    }
    
    if (data.status) {
        ftxData.status.frostRisk = data.status.frost_risk;
        ftxData.status.bypass = data.status.bypass;
        ftxData.status.filterWarning = data.status.filter_warning;
    }
}

// ==================== UI FUNCTIONS ====================

// Initialize SVG Gauge
function initGauge() {
    const svg = document.querySelector('.gauge');
    if (!svg) return;
    
    const defs = document.createElementNS('http://www.w3.org/2000/svg', 'defs');
    defs.innerHTML = `
        <linearGradient id="gaugeGradient" x1="0%" y1="0%" x2="100%" y2="0%">
            <stop offset="0%" style="stop-color:#dc3545" />
            <stop offset="50%" style="stop-color:#ffc107" />
            <stop offset="100%" style="stop-color:#28a745" />
        </linearGradient>
    `;
    svg.insertBefore(defs, svg.firstChild);
}

// Update all UI elements
function updateUI() {
    updateGauge();
    updateEfficiencyStats();
    updateTemperatureDisplay();
    updateFanControls();
    updateStatusIndicators();
    updateFilterInfo();
}

// Update Gauge
function updateGauge() {
    const efficiency = ftxData.efficiency;
    const gaugeFill = document.getElementById('efficiency-gauge');
    const gaugeValue = document.getElementById('efficiency-percent');
    
    if (gaugeFill && gaugeValue) {
        const percentage = Math.min(Math.max(efficiency, 0), 100);
        const angle = (percentage / 100) * 180;
        const rad = (angle * Math.PI) / 180;
        const endX = 100 - 80 * Math.cos(rad);
        const endY = 100 - 80 * Math.sin(rad);
        
        const largeArc = angle > 90 ? 1 : 0;
        const path = `M 20 100 A 80 80 0 ${largeArc} 1 ${endX} ${endY}`;
        
        gaugeFill.setAttribute('d', path);
        gaugeValue.textContent = efficiency.toFixed(1);
    }
}

// Update Efficiency Statistics
function updateEfficiencyStats() {
    const powerEl = document.getElementById('power-recovered');
    const savingEl = document.getElementById('daily-saving');
    const costEl = document.getElementById('cost-saving');
    
    if (powerEl) powerEl.textContent = Math.round(ftxData.powerRecovered);
    if (savingEl) savingEl.textContent = ftxData.dailySaving.toFixed(1);
    if (costEl) costEl.textContent = ftxData.costSaving.toFixed(0);
}

// Update Temperature Display
function updateTemperatureDisplay() {
    updateTempBox('outdoor-temp', ftxData.sensors.outdoor);
    updateTempBox('supply-temp', ftxData.sensors.supply);
    updateTempBox('exhaust-temp', ftxData.sensors.exhaust);
    updateTempBox('extract-temp', ftxData.sensors.extract);
    
    const badge = document.getElementById('hx-efficiency');
    if (badge) badge.textContent = ftxData.efficiency.toFixed(0) + '%';
    
    const airflowEl = document.getElementById('airflow-rate');
    if (airflowEl) airflowEl.textContent = ftxData.airflow;
}

function updateTempBox(id, data) {
    const box = document.getElementById(id);
    if (!box) return;
    
    const tempValue = box.querySelector('.temp-value');
    const rhValue = box.querySelector('.rh-value');
    
    if (tempValue) tempValue.textContent = data.temp.toFixed(1) + '°C';
    if (rhValue) rhValue.textContent = data.rh.toFixed(0) + '%';
}

// Update Fan Controls
function updateFanControls() {
    const supplySlider = document.getElementById('fan-supply');
    const exhaustSlider = document.getElementById('fan-exhaust');
    const supplyVal = document.getElementById('fan-supply-val');
    const exhaustVal = document.getElementById('fan-exhaust-val');
    
    if (supplySlider) {
        supplySlider.value = ftxData.fanSpeed.supply;
        supplySlider.disabled = ftxData.mode !== 'manual';
    }
    if (exhaustSlider) {
        exhaustSlider.value = ftxData.fanSpeed.exhaust;
        exhaustSlider.disabled = ftxData.mode !== 'manual';
    }
    if (supplyVal) supplyVal.textContent = ftxData.fanSpeed.supply + '%';
    if (exhaustVal) exhaustVal.textContent = ftxData.fanSpeed.exhaust + '%';
    
    document.querySelectorAll('.mode-btn').forEach(btn => {
        btn.classList.toggle('active', btn.dataset.mode === ftxData.mode);
    });
}

// Update Status Indicators
function updateStatusIndicators() {
    updateStatusItem('status-frost', ftxData.status.frostRisk || ftxData.status.frostProtection);
    updateStatusItem('status-bypass', ftxData.status.bypass);
    updateStatusItem('status-filter', ftxData.status.filterWarning, true);
    updateStatusItem('status-humidity', ftxData.status.highHumidity);
}

function updateStatusItem(id, active, invert = false) {
    const item = document.getElementById(id);
    if (!item) return;
    
    const state = item.querySelector('.status-state');
    if (!state) return;
    
    const isActive = invert ? !active : active;
    
    item.classList.toggle('alert', active);
    item.classList.toggle('ok', invert && !active);
    
    if (isActive) {
        state.className = 'status-state active';
        state.textContent = 'På';
    } else {
        state.className = 'status-state inactive';
        state.textContent = 'Av';
    }
}

// Update Filter Info
function updateFilterInfo() {
    const progress = document.getElementById('filter-progress');
    const remaining = document.getElementById('filter-remaining');
    
    if (progress && remaining) {
        const totalHours = 4000;
        const usedHours = totalHours - ftxData.filterHoursRemaining;
        const percentage = (usedHours / totalHours) * 100;
        
        progress.style.width = percentage + '%';
        remaining.textContent = ftxData.filterHoursRemaining + 'h kvar';
        
        if (ftxData.filterHoursRemaining < 500) {
            progress.style.background = '#dc3545';
        } else if (ftxData.filterHoursRemaining < 1000) {
            progress.style.background = '#ffc107';
        } else {
            progress.style.background = '#28a745';
        }
    }
}

// Initialize Event Listeners
function initEventListeners() {
    document.querySelectorAll('.mode-btn').forEach(btn => {
        btn.addEventListener('click', function() {
            setMode(this.dataset.mode);
        });
    });
    
    const supplySlider = document.getElementById('fan-supply');
    const exhaustSlider = document.getElementById('fan-exhaust');
    const syncCheckbox = document.getElementById('fan-sync');
    
    if (supplySlider) {
        supplySlider.addEventListener('input', function() {
            const val = parseInt(this.value);
            ftxData.fanSpeed.supply = val;
            if (syncCheckbox && syncCheckbox.checked) {
                ftxData.fanSpeed.exhaust = val;
                if (exhaustSlider) exhaustSlider.value = val;
            }
            updateFanControls();
            sendFanCommand('supply', val);
        });
    }
    
    if (exhaustSlider) {
        exhaustSlider.addEventListener('input', function() {
            const val = parseInt(this.value);
            ftxData.fanSpeed.exhaust = val;
            if (syncCheckbox && syncCheckbox.checked) {
                ftxData.fanSpeed.supply = val;
                if (supplySlider) supplySlider.value = val;
            }
            updateFanControls();
            sendFanCommand('exhaust', val);
        });
    }
    
    const resetBtn = document.getElementById('btn-reset-filter');
    if (resetBtn) {
        resetBtn.addEventListener('click', function() {
            if (confirm('Nollställ filtertimer?')) {
                sendCommand('reset_filter', 1);
                showNotification('Filtertimer nollställd');
            }
        });
    }
}

// Send fan command via MQTT or API
function sendFanCommand(fan, value) {
    sendCommand('fan_speed', value);
}

// Send command to device
async function sendCommand(command, value) {
    // Try MQTT first
    if (mqttConnected && mqttClient) {
        const topic = 'thermoflow/ftx/control/' + command;
        mqttClient.publish(topic, value.toString());
        return;
    }
    
    // Fallback to HTTP API
    try {
        const response = await fetch('/api/ftx/control', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ command: command, value: value })
        });
        if (!response.ok) throw new Error('Command failed');
    } catch (err) {
        console.error('Failed to send command:', err);
        showNotification('Kommandot misslyckades');
    }
}

// Set Operating Mode
function setMode(mode) {
    ftxData.mode = mode;
    updateFanControls();
    sendCommand('mode', mode);
    showNotification('Läge: ' + mode);
}

// Show Notification
function showNotification(message) {
    const notif = document.createElement('div');
    notif.className = 'notification';
    notif.textContent = message;
    notif.style.cssText = `
        position: fixed;
        bottom: 20px;
        right: 20px;
        background: #4ecdc4;
        color: #000;
        padding: 15px 25px;
        border-radius: 8px;
        font-weight: 600;
        z-index: 1000;
        animation: slideIn 0.3s ease;
    `;
    
    document.body.appendChild(notif);
    
    setTimeout(() => {
        notif.style.animation = 'slideOut 0.3s ease';
        setTimeout(() => notif.remove(), 300);
    }, 3000);
}

// Initialize Chart
function initChart() {
    const canvas = document.getElementById('energy-chart');
    if (!canvas) return;
    
    const ctx = canvas.getContext('2d');
    const width = canvas.width;
    const height = canvas.height;
    
    ctx.clearRect(0, 0, width, height);
    
    const padding = 40;
    const chartWidth = width - padding * 2;
    const chartHeight = height - padding * 2;
    const barWidth = chartWidth / energyHistory.length / 2 - 5;
    const maxKwh = Math.max(...energyHistory.map(d => d.kwh)) * 1.2;
    
    ctx.strokeStyle = 'rgba(255, 255, 255, 0.1)';
    ctx.lineWidth = 1;
    for (let i = 0; i <= 4; i++) {
        const y = padding + (chartHeight / 4) * i;
        ctx.beginPath();
        ctx.moveTo(padding, y);
        ctx.lineTo(width - padding, y);
        ctx.stroke();
    }
    
    energyHistory.forEach((data, index) => {
        const x = padding + (chartWidth / energyHistory.length) * index + (chartWidth / energyHistory.length) / 2;
        const barHeight1 = (data.kwh / maxKwh) * chartHeight;
        const barHeight2 = (data.cost / (maxKwh * 2)) * chartHeight;
        
        ctx.fillStyle = '#4ecdc4';
        ctx.fillRect(x - barWidth, height - padding - barHeight1, barWidth, barHeight1);
        
        ctx.fillStyle = '#e94560';
        ctx.fillRect(x, height - padding - barHeight2, barWidth, barHeight2);
        
        ctx.fillStyle = '#888';
        ctx.font = '12px sans-serif';
        ctx.textAlign = 'center';
        ctx.fillText(data.day, x, height - 15);
    });
}

// Add CSS animations
const style = document.createElement('style');
style.textContent = `
    @keyframes slideIn {
        from { transform: translateX(100%); opacity: 0; }
        to { transform: translateX(0); opacity: 1; }
    }
    @keyframes slideOut {
        from { transform: translateX(0); opacity: 1; }
        to { transform: translateX(100%); opacity: 0; }
    }
    .status.warning {
        background: #ffc107;
        color: #000;
    }
`;
document.head.appendChild(style);
