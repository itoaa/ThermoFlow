// ThermoFlow Web Interface JavaScript

const API_BASE = '/api';

let systemStatus = {
    connected: false,
    sensors: [],
    fans: [],
    network: {}
};

// Initialize on page load
document.addEventListener('DOMContentLoaded', function() {
    init();
    startPolling();
});

function init() {
    // Create sensor placeholders
    const sensorGrid = document.getElementById('sensor-grid');
    for (let i = 0; i < 4; i++) {
        const sensorEl = document.createElement('div');
        sensorEl.className = 'sensor-item';
        sensorEl.id = `sensor-${i}`;
        sensorEl.innerHTML = `
            <h3>Sensor ${i + 1}</h3>
            <div class="sensor-value temp">--</div>
            <div class="sensor-value humidity">--</div>
        `;
        sensorGrid.appendChild(sensorEl);
    }

    // Create fan placeholders
    const fanList = document.getElementById('fan-list');
    for (let i = 0; i < 2; i++) {
        const fanEl = document.createElement('div');
        fanEl.className = 'fan-item';
        fanEl.id = `fan-${i}`;
        fanEl.innerHTML = `
            <div class="fan-info">
                <h3>Fan ${i + 1}</h3>
                <p>Mode: <span class="fan-mode">manual</span></p>
            </div>
            <div class="fan-controls">
                <input type="range" class="fan-slider" min="0" max="100" value="0" 
                       onchange="setFanSpeed(${i}, this.value)">
                <span class="fan-value">0%</span>
            </div>
        `;
        fanList.appendChild(fanEl);
    }
}

async function fetchAPI(endpoint) {
    try {
        const response = await fetch(`${API_BASE}${endpoint}`);
        if (!response.ok) throw new Error(`HTTP ${response.status}`);
        return await response.json();
    } catch (error) {
        console.error(`API error: ${error}`);
        return null;
    }
}

async function updateStatus() {
    const status = await fetchAPI('/status');
    if (status) {
        systemStatus.connected = true;
        updateSystemStatus(true);
    } else {
        systemStatus.connected = false;
        updateSystemStatus(false);
    }
}

function updateSystemStatus(connected) {
    const statusEl = document.getElementById('system-status');
    if (connected) {
        statusEl.textContent = 'Connected';
        statusEl.className = 'status connected';
    } else {
        statusEl.textContent = 'Disconnected';
        statusEl.className = 'status disconnected';
    }
}

async function updateSensors() {
    const data = await fetchAPI('/sensors');
    if (!data || !data.sensors) return;

    data.sensors.forEach((sensor, index) => {
        const el = document.getElementById(`sensor-${index}`);
        if (!el) return;

        const tempEl = el.querySelector('.temp');
        const humEl = el.querySelector('.humidity');

        if (sensor.valid) {
            tempEl.textContent = sensor.temperature.toFixed(1) + '°C';
            humEl.textContent = sensor.humidity.toFixed(1) + '%';
            el.classList.remove('invalid');
        } else {
            tempEl.textContent = '--';
            humEl.textContent = '--';
            el.classList.add('invalid');
        }
    });
}

async function updateFans() {
    const data = await fetchAPI('/fans');
    if (!data || !data.fans) return;

    data.fans.forEach((fan, index) => {
        const el = document.getElementById(`fan-${index}`);
        if (!el) return;

        const modeEl = el.querySelector('.fan-mode');
        const slider = el.querySelector('.fan-slider');
        const valueEl = el.querySelector('.fan-value');

        modeEl.textContent = fan.mode || 'manual';
        slider.value = fan.speed || 0;
        valueEl.textContent = fan.speed + '%';
    });
}

async function setFanSpeed(fanId, speed) {
    try {
        const response = await fetch(`${API_BASE}/fans`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ fan_id: fanId, speed: parseInt(speed) })
        });
        
        if (!response.ok) {
            console.error('Failed to set fan speed');
        }
        
        // Update display immediately
        updateFans();
    } catch (error) {
        console.error('Error setting fan speed:', error);
    }
}

async function updateNetwork() {
    const data = await fetchAPI('/config');
    if (!data) return;

    // Mock network data for now
    document.getElementById('wifi-status').textContent = 'Connected';
    document.getElementById('mqtt-status').textContent = 'Connected';
    document.getElementById('rssi').textContent = '-45';
}

async function pollAll() {
    await updateStatus();
    await updateSensors();
    await updateFans();
    await updateNetwork();
}

function startPolling() {
    // Poll immediately
    pollAll();
    
    // Then every 5 seconds
    setInterval(pollAll, 5000);
}

// Handle visibility change to pause/resume polling
document.addEventListener('visibilitychange', function() {
    // Could implement pause/resume here if needed
});