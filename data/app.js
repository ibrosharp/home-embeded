// AURA Hardware Dashboard — Full API Integration
// Integrates all endpoints from the ESP32 SmartWebServer

const DEVICE_IP = '';

function getApiUrl(path) {
  return DEVICE_IP + path;
}

let ws = null;
let cachedLoads = []; // Keep load list for the load-attach modal
let cachedHistory = null;
let currentUserRole = 'guest';

// ======================== Toast Notification System ========================

function showToast(type, title, message, duration = 3500) {
  const container = document.getElementById('toast-container');
  const toast = document.createElement('div');
  toast.className = `toast ${type}`;

  const icons = { success: '✓', error: '✕', info: 'ℹ' };
  toast.innerHTML = `
    <div class="toast-icon">${icons[type] || 'ℹ'}</div>
    <div class="toast-body">
      <div class="toast-title">${title}</div>
      <div class="toast-message">${message}</div>
    </div>
    <button class="toast-close" onclick="dismissToast(this.parentElement)">×</button>
  `;

  container.appendChild(toast);

  // Auto dismiss
  setTimeout(() => dismissToast(toast), duration);
}

function dismissToast(toastEl) {
  if (!toastEl || toastEl.classList.contains('removing')) return;
  toastEl.classList.add('removing');
  setTimeout(() => toastEl.remove(), 300);
}

// ======================== DOM Ready ========================

async function fetchUserRole() {
  try {
    const res = await fetch(getApiUrl('/api/auth/status'));
    if (res.ok) {
      const data = await res.json();
      currentUserRole = data.role || 'guest';
    }
  } catch (err) {
    console.error("Failed to fetch user role", err);
  }
}

function applyRoleRestrictions() {
  if (currentUserRole !== 'admin') {
    document.querySelectorAll('.admin-only').forEach(el => el.style.display = 'none');
  } else {
    document.querySelectorAll('.admin-only').forEach(el => el.style.display = '');
  }
}

document.addEventListener('DOMContentLoaded', async () => {
  const loadingOverlay = document.getElementById('loading-overlay');
  const loadingMessage = document.getElementById('loading-message');
  const mainDashboard = document.getElementById('main-dashboard');
  const wifiBadge = document.getElementById('wifi-badge');
  const statusDot = document.getElementById('status-dot');
  const statusText = document.getElementById('status-text');

  const sensorsContainer = document.getElementById('sensors-container');
  const switchesContainer = document.getElementById('switches-container');
  const loadsContainer = document.getElementById('loads-container');
  const irContainer = document.getElementById('ir-container');
  const settingsContainer = document.getElementById('settings-container');
  const switchCountBadge = document.getElementById('switch-count-badge');
  const loadCountBadge = document.getElementById('load-count-badge');
  const settingsStatusBadge = document.getElementById('settings-status-badge');

  const scenesContainer = document.getElementById('scenes-container');
  const btnAddScene = document.getElementById('btn-add-scene');
  const sceneModal = document.getElementById('scene-modal');
  const closeSceneModalBtn = document.getElementById('btn-close-scene-modal');
  const sceneForm = document.getElementById('scene-form');
  const inputSceneName = document.getElementById('input-scene-name');
  const sceneLoadsContainer = document.getElementById('scene-loads-container');

  const btnOpenRemote = document.getElementById('btn-open-remote');
  const vrModal = document.getElementById('virtual-remote-modal');
  const closeVrModalBtn = document.getElementById('btn-close-virtual-remote');
  const vrDeviceSelect = document.getElementById('virtual-remote-device-select');
  const btnAddVrDevice = document.getElementById('btn-add-virtual-device');
  const vrGrid = document.getElementById('virtual-remote-grid');
  let currentSelectedVrDevice = '';
  let globalIrCommands = [];

  if (btnOpenRemote) {
    btnOpenRemote.addEventListener('click', () => {
      vrModal.classList.add('active');
      renderVirtualRemote();
    });
  }

  if (closeVrModalBtn) {
    closeVrModalBtn.addEventListener('click', () => {
      vrModal.classList.remove('active');
    });
  }

  if (vrDeviceSelect) {
    vrDeviceSelect.addEventListener('change', (e) => {
      currentSelectedVrDevice = e.target.value;
      renderVirtualRemote();
    });
  }

    if (btnAddVrDevice) {
    btnAddVrDevice.addEventListener('click', async () => {
      const newName = prompt('Enter a name for the new Device (e.g., AC, TV):');
      if (!newName || newName.trim() === '') return;
      
      try {
        const res = await fetch(getApiUrl(`/api/ir/devices?name=${encodeURIComponent(newName.trim())}`), { method: 'POST' });
        const data = await res.json();
        
        if (res.ok) {
           showToast('success', 'Device Created', 'Device added successfully.');
           currentSelectedVrDevice = String(data.deviceId);
           pollDeviceData(); // Will trigger re-render
        } else {
           showToast('error', 'Limit Reached', data.error || 'Cannot create device.');
        }
      } catch(err) {
         showToast('error', 'Network Error', 'Could not reach the device.');
      }
    });
  }

  const configBtn = document.getElementById('btn-config');
  const configModal = document.getElementById('config-modal');
  const closeConfigBtn = document.getElementById('btn-close-modal');
  const configForm = document.getElementById('config-form');

  const btnAddIr = document.getElementById('btn-add-ir');
  const irModal = document.getElementById('ir-modal');
  const closeIrModalBtn = document.getElementById('btn-close-ir-modal');
  const irForm = document.getElementById('ir-form');
  const inputIrSlot = document.getElementById('input-ir-slot');
  const inputIrDevice = document.getElementById('input-ir-device');
  const inputIrName = document.getElementById('input-ir-name');
  const irModalTitle = document.getElementById('ir-modal-title');

  const loadModal = document.getElementById('load-modal');
  const closeLoadModalBtn = document.getElementById('btn-close-load-modal');
  const loadForm = document.getElementById('load-form');
  const inputLoadSwitchPin = document.getElementById('input-load-switch-pin');
  const inputLoadPin = document.getElementById('input-load-pin');

  // ======================== Theme Logic ========================
  
  function applyAccentColor(hex) {
    document.documentElement.style.setProperty('--accent-primary', hex);
    
    // Create a darker variant for gradients if not pure hex
    // Using simple transparency for the glow and secondary colors
    document.documentElement.style.setProperty('--active-glow', hex + '4D'); // 30% opacity
    
    // Mark the correct swatch as active
    document.querySelectorAll('.color-swatch').forEach(sw => {
      if (sw.dataset.color === hex) {
        sw.classList.add('active');
      } else {
        sw.classList.remove('active');
      }
    });
  }

  const savedColor = localStorage.getItem('smartHomeAccentColor');
  if (savedColor) {
    applyAccentColor(savedColor);
  } else {
    // Default to the first swatch if none saved
    const firstSwatch = document.querySelector('.color-swatch');
    if (firstSwatch) {
      firstSwatch.classList.add('active');
    }
  }

  document.querySelectorAll('.color-swatch').forEach(swatch => {
    swatch.addEventListener('click', () => {
      const color = swatch.dataset.color;
      applyAccentColor(color);
      localStorage.setItem('smartHomeAccentColor', color);
    });
  });

  // ======================== Initial Health Check ========================

  await fetchUserRole();
  applyRoleRestrictions();

  // Initialize WebSocket connection
  function initWebSocket() {
    const wsUrl = `ws://${DEVICE_IP ? DEVICE_IP : window.location.host}/ws`;
    ws = new WebSocket(wsUrl);

    ws.onopen = () => {
      console.log('WebSocket Connected');
      statusText.textContent = 'Connected (Real-time)';
      statusDot.className = 'status-dot connected';
      wifiBadge.textContent = 'Connected';
      wifiBadge.className = 'badge success';
      loadingOverlay.style.opacity = '0';
      setTimeout(() => {
        loadingOverlay.style.display = 'none';
        mainDashboard.style.display = 'block';
      }, 500);
      
      // Fetch initial full state
      pollDeviceData();
    };

    ws.onmessage = (event) => {
      try {
        const data = JSON.parse(event.data);
        renderSensors(data.sensors);
        cachedLoads = data.loads || []; // Cache loads for UI lookup
        renderSwitches(data.switches);
        renderLoads(data.loads);
        renderScenes(data.scenes);
        renderAutomations(data);
        renderSettings(data.settings);
        
        // Cache globals for other interactions
        globalIrDevices = data.ir.devices || [];
        globalIrCommands = data.ir.commands || [];
      } catch (err) {
        console.error('WS Parse Error:', err);
      }
    };

    ws.onclose = () => {
      console.log('WebSocket Disconnected');
      statusText.textContent = 'Reconnecting...';
      statusDot.className = 'status-dot error';
      setTimeout(initWebSocket, 2000);
    };
  }

  async function initialHealthCheck() {
    loadingMessage.textContent = 'Connecting to real-time service...';
    try {
      const res = await fetch(getApiUrl('/api/status'), { signal: AbortSignal.timeout(5000) });
      if (!res.ok) throw new Error('Status check failed');
      
      initWebSocket();
    } catch (error) {
      console.error(error);
      loadingMessage.textContent = 'Connection failed. Retrying...';
      setTimeout(initialHealthCheck, 3000);
    }
  }

  function showDashboard() {
    loadingOverlay.style.display = 'none';
    mainDashboard.style.display = 'flex';
  }

  // ======================== Room Data Fetching ========================

  async function fetchDeviceData() {
    const res = await fetch(getApiUrl('/api/room'));
    if (!res.ok) throw new Error('Room fetch failed');
    const data = await res.json();

    // On first successful load, show the dashboard
    if (loadingOverlay.style.display !== 'none') {
      showDashboard();
      fetchHistory(); // initial fetch
      setInterval(fetchHistory, 60000); // refresh history every 60s
    }

    renderSensors(data.sensors);
    renderSwitches(data.switches);
    renderLoads(data.loads);
    if (data.controllers && data.controllers.ir) {
      renderIRCommands(data.controllers.ir);
    }
    
    // Scenes
    if (data.scenes) {
      renderScenes(data.scenes);
    }

    // Cache loads for the load-attach modal
    cachedLoads = data.loads || [];
    
    // Also re-render the automation checkboxes if we haven't already
    // but preserve their checked state if they are currently rendered
    const container = document.getElementById('auto-light-loads');
    if (container && container.innerHTML.trim() === '') {
      fetchAutomations(); // Will render them properly with selected state
    }

    return data;
  }

  async function fetchHistory() {
    try {
      const res = await fetch(getApiUrl('/api/history'));
      if (res.ok) {
        cachedHistory = await res.json();
        // Force re-render of sensors to show new charts if we have cached sensors
        // We will just wait for the next poll to pick it up to avoid double rendering
      }
    } catch (err) {
      console.warn('History fetch failed', err);
    }
  }

  async function pollDeviceData() {
    try {
      await fetchDeviceData();
    } catch (err) {
      console.warn('Manual fetch failed:', err);
    }
  }

  // ======================== Settings API ========================

  async function fetchSettings() {
    try {
      const res = await fetch(getApiUrl('/api/settings'));
      if (!res.ok) throw new Error('Settings fetch failed');
      const data = await res.json();
      renderSettings(data);
      if (settingsStatusBadge) settingsStatusBadge.textContent = 'Loaded';

      // Pre-fill WiFi config modal with current SSID
      const ssidInput = document.getElementById('input-ssid');
      if (data.wifiSSID && ssidInput) {
        ssidInput.value = data.wifiSSID;
      }
    } catch (err) {
      console.warn('Settings fetch failed:', err);
      if (settingsStatusBadge) settingsStatusBadge.textContent = 'Unavailable';
      renderSettingsFallback();
    }
  }

  function renderSettings(settings) {
    if (!settings) {
      if (settingsStatusBadge) settingsStatusBadge.textContent = 'Unavailable';
      return;
    }
    if (settingsStatusBadge) settingsStatusBadge.textContent = 'Loaded';
    
    // Additional settings rendering logic goes here if needed
  }

  async function updateSetting(params) {
    try {
      const queryString = Object.entries(params)
        .map(([k, v]) => `${encodeURIComponent(k)}=${encodeURIComponent(v)}`)
        .join('&');

      const res = await fetch(getApiUrl(`/api/settings?${queryString}`), { method: 'POST' });
      if (!res.ok) throw new Error('Settings update failed');
      const data = await res.json();

      if (data.success) {
        showToast('success', 'Settings Updated', data.message || 'Changes saved to device.');
        // Re-fetch settings to ensure UI is in sync
        await fetchSettings();
      } else {
        showToast('error', 'Update Failed', data.error || 'Unknown error.');
      }
    } catch (err) {
      console.error('Settings update error:', err);
      showToast('error', 'Network Error', 'Could not reach the device.');
    }
  }

  // ======================== Automations API ========================

  async function fetchAutomations() {
    try {
      const res = await fetch(getApiUrl('/api/automation/light'));
      if (!res.ok) throw new Error('Automation fetch failed');
      const data = await res.json();
      
      document.getElementById('auto-light-enabled').checked = data.enabled;
      document.getElementById('auto-light-threshold').value = data.threshold;
      
      const autoOffToggle = document.getElementById('auto-off-enabled');
      if (autoOffToggle) autoOffToggle.checked = data.autoOffEnabled || false;
      
      const autoOffTimeout = document.getElementById('auto-off-timeout');
      if (autoOffTimeout) autoOffTimeout.value = data.autoOffTimeoutSeconds || 300;
      
      // We need to render the loads checkboxes dynamically based on cachedLoads
      renderAutomationLoads(data.loadPins || []);
    } catch (err) {
      console.warn('Failed to fetch automations', err);
    }
  }

  async function fetchClimateAutomation() {
    try {
      const res = await fetch(getApiUrl('/api/automation/climate'));
      if (!res.ok) throw new Error('Climate automation fetch failed');
      const data = await res.json();
      
      document.getElementById('auto-climate-enabled').checked = data.enabled;
      document.getElementById('auto-climate-target').value = data.targetTemp;
      
      const onSelect = document.getElementById('auto-climate-on-slot');
      if (onSelect && Array.from(onSelect.options).some(o => o.value == data.irSlotPowerOn)) {
        onSelect.value = data.irSlotPowerOn;
      }
      
      const offSelect = document.getElementById('auto-climate-off-slot');
      if (offSelect && Array.from(offSelect.options).some(o => o.value == data.irSlotPowerOff)) {
        offSelect.value = data.irSlotPowerOff;
      }
    } catch (err) {
      console.warn('Failed to fetch climate automation', err);
    }
  }

  function renderAutomationLoads(selectedPins) {
    const container = document.getElementById('auto-light-loads');
    if (!container) return;
    
    container.innerHTML = '';
    
    if (!cachedLoads || cachedLoads.length === 0) {
      container.innerHTML = '<span style="font-size:12px; color:var(--text-secondary);">No loads available</span>';
      return;
    }
    
    cachedLoads.forEach((load, index) => {
      const isChecked = selectedPins.includes(load.pin) ? 'checked' : '';
      container.innerHTML += `
        <label style="display:flex; align-items:center; gap:8px; cursor:pointer; padding:4px 0;">
          <input type="checkbox" class="auto-load-checkbox" value="${load.pin}" ${isChecked}>
          <span style="font-size:14px; color:var(--text-primary);">Load ${index + 1}</span>
        </label>
      `;
    });
  }

  document.getElementById('btn-save-auto-light')?.addEventListener('click', async () => {
    const btn = document.getElementById('btn-save-auto-light');
    btn.textContent = 'Saving...';
    
    const enabled = document.getElementById('auto-light-enabled').checked;
    const threshold = document.getElementById('auto-light-threshold').value;
    
    const autoOffEnabled = document.getElementById('auto-off-enabled') ? document.getElementById('auto-off-enabled').checked : false;
    const autoOffTimeout = document.getElementById('auto-off-timeout') ? document.getElementById('auto-off-timeout').value : 300;
    
    const checkboxes = document.querySelectorAll('.auto-load-checkbox:checked');
    const selectedPins = Array.from(checkboxes).map(cb => cb.value).join(',');
    
    const formData = new URLSearchParams();
    formData.append('enabled', enabled);
    formData.append('threshold', threshold);
    formData.append('autoOffEnabled', autoOffEnabled);
    formData.append('autoOffTimeoutSeconds', autoOffTimeout);
    formData.append('loadPins', selectedPins);
    
    try {
      const res = await fetch(getApiUrl('/api/automation/light'), {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: formData.toString()
      });
      
      if (res.ok) {
        showToast('success', 'Automation Saved', 'Light automation settings applied.');
      } else {
        showToast('error', 'Save Failed', 'Device returned an error.');
      }
    } catch (err) {
      console.error('Automation save failed', err);
      showToast('error', 'Network Error', 'Could not reach the device.');
    }
    
    setTimeout(() => btn.textContent = 'Save Automation', 1000);
  });

  document.getElementById('btn-save-auto-climate')?.addEventListener('click', async (e) => {
    const btn = e.target;
    btn.textContent = 'Saving...';
    
    const enabled = document.getElementById('auto-climate-enabled').checked;
    const targetTemp = document.getElementById('auto-climate-target').value;
    const irSlotPowerOn = document.getElementById('auto-climate-on-slot').value;
    const irSlotPowerOff = document.getElementById('auto-climate-off-slot').value;

    const params = new URLSearchParams();
    params.append('enabled', enabled);
    params.append('targetTemp', targetTemp);
    params.append('irSlotPowerOn', irSlotPowerOn);
    params.append('irSlotPowerOff', irSlotPowerOff);

    try {
      const res = await fetch(getApiUrl('/api/automation/climate'), {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: params.toString()
      });
      
      if (res.ok) {
        showToast('success', 'Thermostat Saved', 'Climate automation settings applied.');
      } else {
        showToast('error', 'Save Failed', 'Device returned an error.');
      }
    } catch (err) {
      console.error('Automation save failed', err);
      showToast('error', 'Network Error', 'Could not reach the device.');
    }
    
    setTimeout(() => btn.textContent = 'Save Thermostat', 1000);
  });

  // ======================== Rendering Components ========================

  function createSparkline(dataArr, color) {
    if (!dataArr || dataArr.length < 2) return '';
    const min = Math.min(...dataArr) * 0.9;
    const max = Math.max(...dataArr) * 1.1;
    const range = max - min || 1;
    const width = 100;
    const height = 30;
    
    let path = `M 0 ${height - ((dataArr[0] - min) / range) * height}`;
    for (let i = 1; i < dataArr.length; i++) {
      const x = (i / (dataArr.length - 1)) * width;
      const y = height - ((dataArr[i] - min) / range) * height;
      path += ` L ${x} ${y}`;
    }
    
    return `<svg width="100%" height="30" viewBox="0 0 100 30" preserveAspectRatio="none" style="margin-top:8px;">
      <path d="${path}" fill="none" stroke="${color}" stroke-width="2" vector-effect="non-scaling-stroke" stroke-linejoin="round"/>
    </svg>`;
  }

  function renderSensors(sensors) {
    if (!sensors) return;

    let html = '';

    // Extract history arrays
    let tempHist = [], humHist = [], lightHist = [];
    if (cachedHistory && cachedHistory.points) {
      tempHist = cachedHistory.points.map(p => p.t);
      humHist = cachedHistory.points.map(p => p.h);
      lightHist = cachedHistory.points.map(p => p.l);
    }

    // Climate
    if (sensors.climate) {
      const tempColor = sensors.climate.temperature > 30 ? 'var(--danger)' : sensors.climate.temperature > 25 ? 'var(--warning)' : '#fff';
      html += `
        <div class="v2-glass-card">
          <div class="sensor-header">
            <span class="sensor-icon" style="color: var(--warning);">🌡️</span>
            <span class="sensor-label">Temperature</span>
          </div>
          <span class="sensor-value" style="color: ${tempColor}">${sensors.climate.temperature.toFixed(1)}°C</span>
          ${createSparkline(tempHist, tempColor !== '#fff' ? tempColor : 'var(--accent-primary)')}
        </div>
        <div class="v2-glass-card">
          <div class="sensor-header">
            <span class="sensor-icon" style="color: var(--info);">💧</span>
            <span class="sensor-label">Humidity</span>
          </div>
          <span class="sensor-value">${sensors.climate.humidity.toFixed(1)}%</span>
          ${createSparkline(humHist, 'var(--info)')}
        </div>
      `;
    }
    // Light
    if (sensors.light) {
      const lightIcon = sensors.light.percentage > 50 ? '☀️' : '🌙';
      html += `
        <div class="v2-glass-card">
          <div class="sensor-header">
            <span class="sensor-icon" style="color: var(--warning);">${lightIcon}</span>
            <span class="sensor-label">Ambient Light</span>
          </div>
          <span class="sensor-value">${sensors.light.percentage.toFixed(1)}%</span>
          ${createSparkline(lightHist, 'var(--warning)')}
        </div>
      `;
    }
    // Presence
    if (sensors.presence) {
      const motionText = sensors.presence.motion ? 'DETECTED' : 'CLEAR';
      const color = sensors.presence.motion ? 'var(--accent-primary)' : 'var(--text-secondary)';
      const icon = sensors.presence.motion ? '🚶‍♂️' : '🔒';
      const glowStyle = sensors.presence.motion ? 'box-shadow: 0 0 20px rgba(12,255,184,0.15); border-color: var(--accent-primary);' : '';
      html += `
        <div class="v2-glass-card" style="${glowStyle}">
          <div class="sensor-header">
            <span class="sensor-icon" style="color: ${color};">${icon}</span>
            <span class="sensor-label" style="color: ${color};">Presence</span>
          </div>
          <span class="sensor-value" style="color: ${color};">${motionText}</span>
        </div>
      `;
    }

    sensorsContainer.innerHTML = html;
  }

  function renderSwitches(switches) {
    if (!switches) return;
    if (switchCountBadge) switchCountBadge.textContent = `${switches.length} Configured`;
    const zonesContainer = document.getElementById('zones-container');
    if (!zonesContainer) return;

    if (zonesContainer.children.length === 0 || window.lastSwitchCount !== switches.length) {
      window.lastSwitchCount = switches.length;
      let html = '';
      for (let i = 0; i < switches.length; i += 4) {
        const zoneSwitches = switches.slice(i, i + 4);
        const zoneNumber = Math.floor(i / 4) + 1;
        html += `<div class="zone-panel">`;
        html += `<div class="zone-header"><span>Zone ${zoneNumber}</span><span style="color:var(--text-muted)">...</span></div>`;
        html += `<div style="display:grid; grid-template-columns:1fr 1fr; gap:12px;">`;
        zoneSwitches.forEach((sw, idx) => {
          const globalIndex = i + idx;
          const loadState = sw.hasLoad && sw.load ? sw.load.state : false;
          const isActive = sw.hasLoad ? loadState : (sw.state === 1);
          const loadIdx = sw.hasLoad && sw.load && cachedLoads ? cachedLoads.findIndex(l => l.pin === sw.load.pin) : -1;
          const loadBadgeText = loadIdx >= 0 ? `[L${loadIdx + 1}]` : `[--]`;
          const displayName = sw.name || `Switch ${globalIndex + 1}`;
          
          html += `
            <div class="v2-switch-item" id="card-switch-${sw.pin}">
              <div class="v2-switch-row">
                <span class="v2-switch-name">
                  ${displayName}
                  ${currentUserRole === 'admin' ? `<button class="icon-btn edit-name-btn admin-only" data-type="switch" data-pin="${sw.pin}" data-name="${sw.name || ''}" style="padding: 0px; margin-left: 4px; font-size: 10px; border:none; background:none;">✎</button>` : ''}
                </span>
                <span class="v2-switch-badge" style="color: ${sw.hasLoad ? 'var(--accent-primary)' : 'var(--text-muted)'}; border-color: ${sw.hasLoad ? 'var(--accent-primary)' : 'var(--text-muted)'};">${loadBadgeText}</span>
              </div>
              <div class="v2-switch-row" style="margin-top: 4px;">
                <div style="display:flex; align-items:center; gap:8px;">
                  <label class="custom-toggle small">
                    <input type="checkbox" class="btn-toggle-new" data-pin="${sw.pin}" ${isActive ? 'checked' : ''}>
                    <span class="custom-slider"></span>
                  </label>
                  <span class="v2-switch-state-text ${isActive ? 'on' : 'off'}">${isActive ? 'ON' : 'OFF'}</span>
                </div>
                ${currentUserRole === 'admin' ? `<button class="btn-manage-v2 admin-only" data-pin="${sw.pin}" data-index="${globalIndex + 1}" data-has-load="${sw.hasLoad}" data-load-pin="${sw.hasLoad && sw.load ? sw.load.pin : -1}">Manage Load <span style="font-size:10px">☰</span></button>` : ''}
              </div>
            </div>
          `;
        });
        html += `</div></div>`;
      }
      zonesContainer.innerHTML = html;

      // Bind rename events
      document.querySelectorAll('.edit-name-btn').forEach(el => {
        el.addEventListener('click', async (e) => {
          const type = e.currentTarget.dataset.type;
          const pin = e.currentTarget.dataset.pin;
          const currentName = e.currentTarget.dataset.name;
          const newName = prompt(`Enter a new name for this ${type}:`, currentName);
          if (newName !== null && newName.trim() !== '') {
            try {
              const formData = new URLSearchParams();
              formData.append('type', type);
              formData.append('pin', pin);
              formData.append('name', newName.trim());
              const res = await fetch(getApiUrl('/api/hardware/rename'), {
                method: 'POST',
                headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
                body: formData.toString()
              });
              if (res.ok) {
                showToast('success', 'Renamed successfully', `${type} renamed to ${newName.trim()}`);
                zonesContainer.innerHTML = ''; // force rebuild
                pollDeviceData();
              } else {
                showToast('error', 'Rename Failed', 'Device error.');
              }
            } catch (err) {
              showToast('error', 'Network Error', 'Could not reach device.');
            }
          }
        });
      });

      // Bind toggle events (using change instead of click for checkbox)
      document.querySelectorAll('.btn-toggle-new').forEach(el => {
        el.addEventListener('change', async (e) => {
          const pin = e.target.dataset.pin;
          try {
            const res = await fetch(getApiUrl(`/api/switch/state?pin=${pin}`), { method: 'POST' });
            if (!res.ok) {
              e.target.checked = !e.target.checked; // revert
              showToast('error', 'Toggle Failed', 'Device returned an error.');
            } else {
               pollDeviceData();
            }
          } catch (err) {
            e.target.checked = !e.target.checked; // revert
            showToast('error', 'Network Error', 'Could not reach the device.');
          }
        });
      });

      // Bind load-attach events
      document.querySelectorAll('.btn-manage-v2').forEach(el => {
        el.addEventListener('click', (e) => {
          const pin = e.currentTarget.dataset.pin;
          const index = e.currentTarget.dataset.index;
          const currentLoadPin = parseInt(e.currentTarget.dataset.loadPin);
          openLoadModal(pin, `Switch ${index}`, currentLoadPin);
        });
      });
    } else {
      // Just update states smoothly without rebuilding DOM
      switches.forEach((sw, index) => {
        const card = document.getElementById(`card-switch-${sw.pin}`);
        if (card) {
          const loadState = sw.hasLoad && sw.load ? sw.load.state : (sw.state === 1);
          
          const checkbox = card.querySelector('.btn-toggle-new');
          if (checkbox) checkbox.checked = loadState;

          const stateText = card.querySelector('.v2-switch-state-text');
          if (stateText) {
             stateText.className = `v2-switch-state-text ${loadState ? 'on' : 'off'}`;
             stateText.textContent = loadState ? 'ON' : 'OFF';
          }

          const h3 = card.querySelector('.v2-switch-name');
          if (h3) {
             const displayName = sw.name || `Switch ${index + 1}`;
             // h3 has childnodes for button, update only text
             if(h3.childNodes[0].nodeType === 3) h3.childNodes[0].textContent = displayName + " ";
          }

          const badge = card.querySelector('.v2-switch-badge');
          if (badge) {
            const loadIdx = sw.hasLoad && sw.load && cachedLoads ? cachedLoads.findIndex(l => l.pin === sw.load.pin) : -1;
            const loadBadgeText = loadIdx >= 0 ? `[L${loadIdx + 1}]` : `[--]`;
            badge.textContent = loadBadgeText;
            badge.style.color = sw.hasLoad ? 'var(--accent-primary)' : 'var(--text-muted)';
            badge.style.borderColor = sw.hasLoad ? 'var(--accent-primary)' : 'var(--text-muted)';
          }
          
          const manageBtn = card.querySelector('.btn-manage-v2');
          if(manageBtn) {
             manageBtn.dataset.hasLoad = sw.hasLoad;
             manageBtn.dataset.loadPin = sw.hasLoad && sw.load ? sw.load.pin : -1;
          }
        }
      });
    }
  }

  function renderLoads(loads) {
    if (!loads || loads.length === 0) {
      if (loadCountBadge) loadCountBadge.textContent = '0 Registered';
      loadsContainer.innerHTML = '<div style="color:var(--text-muted); font-size:12px;">No loads registered.</div>';
      return;
    }

    if (loadCountBadge) loadCountBadge.textContent = `${loads.length} Registered`;

    // Check if data changed to avoid rebuilding
    const loadsJson = JSON.stringify(loads);
    if (loadsContainer.dataset.lastJson === loadsJson) return;
    loadsContainer.dataset.lastJson = loadsJson;

    loadsContainer.innerHTML = '';
    loads.forEach((load, index) => {
      const isOn = load.state === true;
      const div = document.createElement('div');
      div.className = `v2-glass-card load-card-v2 ${isOn ? 'active' : ''}`;
      div.id = `card-load-${load.pin}`;

      const displayName = load.name || `Load ${index + 1}`;
      div.innerHTML = `
        <div style="display:flex; justify-content:space-between; align-items:center;">
           <span class="v2-switch-name" style="display:flex; align-items:center; gap:8px;">
             <span style="color:var(--warning);">💡</span>
             ${displayName}
             <button class="icon-btn edit-name-btn" data-type="load" data-pin="${load.pin}" data-name="${load.name || ''}" style="padding:0; margin-left:4px; font-size:10px; border:none; background:none; color:var(--text-muted);">✎</button>
           </span>
           <label class="custom-toggle small">
              <input type="checkbox" class="load-toggle-v2" data-pin="${load.pin}" ${isOn ? 'checked' : ''}>
              <span class="custom-slider"></span>
           </label>
        </div>
        <div style="display:flex; align-items:center; gap:6px; margin-top:8px;">
           <span style="color:${isOn ? 'var(--accent-primary)' : 'var(--text-muted)'}; font-size:10px; font-weight:700;">⚡ ${isOn ? 'ON' : 'OFF'}</span>
           <span style="color:var(--text-secondary); font-size:9px; margin-left:auto;">${load.activeLow ? 'Active-Low' : 'Active-High'}</span>
        </div>
      `;
      loadsContainer.appendChild(div);
    });

    // Bind rename events
    document.querySelectorAll('#loads-container .edit-name-btn').forEach(el => {
      el.addEventListener('click', async (e) => {
        const type = e.currentTarget.dataset.type;
        const pin = e.currentTarget.dataset.pin;
        const currentName = e.currentTarget.dataset.name;
        const newName = prompt(`Enter a new name for this ${type}:`, currentName);
        if (newName !== null && newName.trim() !== '') {
          try {
            const formData = new URLSearchParams();
            formData.append('type', type);
            formData.append('pin', pin);
            formData.append('name', newName.trim());
            const res = await fetch(getApiUrl('/api/hardware/rename'), {
              method: 'POST',
              headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
              body: formData.toString()
            });
            if (res.ok) {
              showToast('success', 'Renamed successfully', `${type} renamed to ${newName.trim()}`);
              loadsContainer.dataset.lastJson = ''; // force rebuild
              pollDeviceData();
            } else {
              showToast('error', 'Rename Failed', 'Device error.');
            }
          } catch (err) {
            showToast('error', 'Network Error', 'Could not reach device.');
          }
        }
      });
    });

    document.querySelectorAll('.load-toggle-v2').forEach(el => {
      el.addEventListener('change', async (e) => {
        const pin = e.target.dataset.pin;
        try {
          const res = await fetch(getApiUrl(`/api/load/toggle?pin=${pin}`), { method: 'POST' });
          if (!res.ok) {
            e.target.checked = !e.target.checked;
            showToast('error', 'Toggle Failed', 'Device returned an error.');
          } else {
             loadsContainer.dataset.lastJson = ''; // force update 
             pollDeviceData();
          }
        } catch (err) {
          e.target.checked = !e.target.checked;
          showToast('error', 'Network Error', 'Could not reach the device.');
        }
      });
    });
  }

  function renderScenes(scenes) {
    if (!scenes || scenes.length === 0) {
      scenesContainer.innerHTML = '<div style="color:var(--text-muted); font-size:12px;">No scenes created yet.</div>';
      return;
    }

    const scenesJson = JSON.stringify(scenes);
    if (scenesContainer.dataset.lastJson === scenesJson) return;
    scenesContainer.dataset.lastJson = scenesJson;

    scenesContainer.innerHTML = '';
    scenes.forEach(scene => {
      const div = document.createElement('div');
      div.className = 'v2-glass-card scene-card-v2';
      div.style.cursor = 'pointer';

      // Assign a random-looking icon based on name
      let icon = '🎬';
      if (scene.name.toLowerCase().includes('light')) icon = '💡';
      else if (scene.name.toLowerCase().includes('sleep') || scene.name.toLowerCase().includes('night')) icon = '🌙';

      div.innerHTML = `
        <div style="display:flex; justify-content:space-between; align-items:flex-start;">
           <span class="v2-switch-name">${scene.name}</span>
           <div style="display:flex; gap:6px; align-items:center;">
             <div style="width:6px; height:6px; border-radius:50%; background:rgba(255,255,255,0.1);"></div>
             <button class="icon-btn scene-del-btn" data-id="${scene.id}" style="padding:0; font-size:10px; border:none; background:none; color:var(--text-muted);" title="Delete Scene">✕</button>
           </div>
        </div>
        <div style="display:flex; justify-content:space-between; align-items:flex-end;">
           <span class="scene-icon-v2">${icon}</span>
        </div>
      `;

      div.addEventListener('click', async (e) => {
        if(e.target.classList.contains('scene-del-btn')) return; // handled separately
        try {
          const formData = new URLSearchParams();
          formData.append('id', scene.id);
          const res = await fetch(getApiUrl('/api/scenes/execute'), {
            method: 'POST',
            headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
            body: formData.toString()
          });
          if (res.ok) {
            showToast('success', 'Scene Executed', `Fired scene ID ${scene.id}.`);
            // Flash the dot green
            const dot = div.querySelector('div[style*="width:6px"]');
            if(dot) { dot.style.background = 'var(--accent-primary)'; dot.style.boxShadow = '0 0 8px var(--accent-primary)'; }
            setTimeout(() => { if(dot) { dot.style.background = 'rgba(255,255,255,0.1)'; dot.style.boxShadow = 'none'; } }, 1000);
            pollDeviceData();
          } else {
            showToast('error', 'Execution Failed', 'Device returned an error.');
          }
        } catch (err) {
          showToast('error', 'Network Error', 'Could not reach the device.');
        }
      });
      scenesContainer.appendChild(div);
    });

    // Add Global Activate Button card
    const btnDiv = document.createElement('div');
    btnDiv.className = 'v2-glass-card';
    btnDiv.style.justifyContent = 'center';
    btnDiv.style.padding = '16px';
    btnDiv.style.minWidth = '140px';
    btnDiv.innerHTML = `<button class="btn-primary v2-full-btn" id="btn-activate-scene-global">ACTIVATE SCENE</button>`;
    scenesContainer.appendChild(btnDiv);

    btnDiv.querySelector('button').addEventListener('click', () => {
       showToast('info', 'Activate Scene', 'Click on any scene card to activate it.');
    });

    document.querySelectorAll('.scene-del-btn').forEach(el => {
      el.addEventListener('click', async (e) => {
        e.stopPropagation();
        const id = e.currentTarget.dataset.id;
        if (!confirm(`Are you sure you want to delete Scene ID ${id}?`)) return;

        try {
          const res = await fetch(getApiUrl(`/api/scenes?id=${id}`), { method: 'DELETE' });
          if (res.ok) {
            showToast('success', 'Scene Deleted', `Removed scene ID ${id}.`);
            loadsContainer.dataset.lastJson = ''; // force update
            pollDeviceData();
          } else {
            showToast('error', 'Deletion Failed', 'Device returned an error.');
          }
        } catch (err) {
          showToast('error', 'Network Error', 'Could not reach the device.');
        }
      });
    });
  }

  function renderVirtualRemote() {
    if (!currentSelectedVrDevice || currentSelectedVrDevice === '-1') {
      vrGrid.innerHTML = '<div style="grid-column: span 3; text-align:center; color:var(--text-muted); padding: 20px;">Please select or add a device</div>';
      return;
    }



    const deviceIdInt = parseInt(currentSelectedVrDevice);
    const device = globalIrDevices.find(d => d.id === deviceIdInt);
    
    if (!device) {
       vrGrid.innerHTML = '<div style="grid-column: span 3; text-align:center; color:var(--text-muted); padding: 20px;">Device not found</div>';
       return;
    }

    vrGrid.innerHTML = '';

    const startSlot = deviceIdInt * 15;

    // Render exactly 15 buttons per device
    for (let i = 0; i < 15; i++) {
      const btn = document.createElement('div');
      const slotIndex = startSlot + i;
      const cmd = globalIrCommands.find(c => parseInt(c.slot) === slotIndex);
      
      if (cmd && cmd.name) {
        btn.className = 'vr-btn';
        btn.innerHTML = `
          <span>${cmd.name}</span>
          <div class="vr-edit-icon" data-slot="${slotIndex}" data-device="${device.name}" data-name="${cmd.name}" title="Edit/Re-record">✎</div>
        `;
        
        btn.addEventListener('click', async (e) => {
          if (e.target.classList.contains('vr-edit-icon')) return; 
          
          btn.style.opacity = '0.5';
          try {
            const res = await fetch(getApiUrl(`/api/ir/emit?slot=${slotIndex}`), { method: 'POST' });
            if (res.ok) showToast('success', 'IR Emitted', `Sent ${cmd.name}`);
            else showToast('error', 'Emit Failed', 'Device error');
          } catch (err) {
            showToast('error', 'Network Error', 'Could not reach device');
          }
          setTimeout(() => btn.style.opacity = '1', 200);
        });

      } else {
        btn.className = 'vr-btn vr-empty';
        btn.innerHTML = 'Empty';
        btn.addEventListener('click', () => {
          irModalTitle.textContent = 'Record Command';
          inputIrSlot.value = slotIndex;
          inputIrDevice.value = device.name;
          inputIrName.value = '';
          irModal.classList.add('active');
        });
      }
      
      vrGrid.appendChild(btn);
    }

    // Add a delete button for the device
    const delBtn = document.createElement('button');
    delBtn.className = 'btn btn-danger';
    delBtn.style.gridColumn = 'span 3';
    delBtn.style.marginTop = '16px';
    delBtn.textContent = 'Delete Device';
    delBtn.addEventListener('click', async () => {
       if(!confirm(`Delete device ${device.name} and all its commands?`)) return;
       try {
           const res = await fetch(getApiUrl(`/api/ir/devices?id=${deviceIdInt}`), { method: 'DELETE' });
           if (res.ok) {
              showToast('success', 'Device Deleted', 'Device removed.');
              currentSelectedVrDevice = '-1';
              pollDeviceData();
           }
       } catch(err) {
           showToast('error', 'Network Error', 'Could not delete device.');
       }
    });
    vrGrid.appendChild(delBtn);

    vrGrid.querySelectorAll('.vr-edit-icon').forEach(icon => {
      icon.addEventListener('click', (e) => {
        e.stopPropagation();
        irModalTitle.textContent = 'Edit Command';
        inputIrSlot.value = icon.dataset.slot;
        inputIrDevice.value = icon.dataset.device;
        inputIrName.value = icon.dataset.name;
        irModal.classList.add('active');
      });
    });
  }

  function renderIRCommands(irData) {
    globalIrCommands = irData.commands || [];
    globalIrDevices = irData.devices || [];

    if (vrDeviceSelect) {
      const oldVal = vrDeviceSelect.value;
      let optionsHtml = '<option value="-1">-- Select Device --</option>';
      globalIrDevices.forEach(dev => {
        optionsHtml += `<option value="${dev.id}">${dev.name}</option>`;
      });
      vrDeviceSelect.innerHTML = optionsHtml;
      
      if (globalIrDevices.find(d => String(d.id) === oldVal)) {
        vrDeviceSelect.value = oldVal;
        currentSelectedVrDevice = oldVal;
      } else if (globalIrDevices.length > 0 && (!currentSelectedVrDevice || currentSelectedVrDevice === '-1')) {
        const first = String(globalIrDevices[0].id);
        vrDeviceSelect.value = first;
        currentSelectedVrDevice = first;
      } else {
        currentSelectedVrDevice = vrDeviceSelect.value;
      }
    }

    if (vrModal && vrModal.classList.contains('active')) {
      renderVirtualRemote();
    }

    const datalist = document.getElementById('ir-device-list');
    if (datalist) {
      datalist.innerHTML = globalIrDevices
        .map(dev => `<option value="${dev.id}">${dev.name}</option>`)
        .join('');
    }

    const onSelect = document.getElementById('auto-climate-on-slot');
    const offSelect = document.getElementById('auto-climate-off-slot');
    
    if (onSelect && offSelect) {
      const onVal = onSelect.value;
      const offVal = offSelect.value;
      
      const optionsHtml = '<option value="-1">-- None --</option>' + 
        globalIrCommands.map(cmd => `<option value="${cmd.slot}">${cmd.deviceId} - ${cmd.name} (Slot ${cmd.slot})</option>`).join('');
      
      onSelect.innerHTML = optionsHtml;
      offSelect.innerHTML = optionsHtml;
      
      if (Array.from(onSelect.options).some(o => o.value == onVal)) onSelect.value = onVal;
      if (Array.from(offSelect.options).some(o => o.value == offVal)) offSelect.value = offVal;
      
      if (!onSelect.dataset.fetched) {
         onSelect.dataset.fetched = "true";
         fetchClimateAutomation();
      }
    }
  }

  function renderSettings(settings) {
    if (!settings) return;

    settingsContainer.innerHTML = `
      <!-- WiFi SSID -->
      <div class="setting-card">
        <span class="setting-label">WiFi Network</span>
        <div class="setting-card-header">
          <span class="setting-value mono" id="display-wifi-ssid">${settings.wifiSSID || '—'}</span>
        </div>
        <div class="setting-actions">
          <button class="action-btn small" id="btn-open-wifi-config" title="Change WiFi credentials">Change</button>
        </div>
      </div>

      <!-- Target Temperature -->
      <div class="setting-card">
        <span class="setting-label">Target Temperature</span>
        <div class="setting-card-header">
          <input type="number" class="setting-inline-input" id="input-target-temp" value="${settings.defaultTargetTemp || 24}" step="0.5" min="16" max="35">
          <span style="font-size: 14px; color: var(--text-secondary);">°C</span>
        </div>
        <div class="setting-actions">
          <button class="action-btn small" id="btn-save-temp">Save</button>
        </div>
      </div>

      <!-- LED Feedback -->
      <div class="setting-card">
        <span class="setting-label">LED Feedback</span>
        <div class="setting-card-header">
          <span class="setting-value" id="display-led-state">${settings.ledFeedbackEnabled ? 'Enabled' : 'Disabled'}</span>
          <label class="toggle-switch">
            <input type="checkbox" id="input-led-toggle" ${settings.ledFeedbackEnabled ? 'checked' : ''}>
            <span class="toggle-slider"></span>
          </label>
        </div>
      </div>

      <!-- Authentication -->
      <div class="setting-card">
        <span class="setting-label">Authentication</span>
        <div class="setting-card-header" style="flex-direction: column; gap: 8px;">
          <input type="text" class="setting-inline-input" id="input-auth-user" value="${settings.authUsername || 'admin'}" placeholder="Admin Username">
          <input type="password" class="setting-inline-input" id="input-auth-pass" value="${settings.authPassword || 'admin'}" placeholder="Admin Password">
          <input type="password" class="setting-inline-input" id="input-guest-pass" value="${settings.guestPassword || 'guest'}" placeholder="Guest Password">
        </div>
        <div class="setting-actions" style="margin-top: 8px;">
          <button class="action-btn small btn-primary" id="btn-save-auth">Save Credentials</button>
        </div>
      </div>

      <!-- Namespace -->
      <div class="setting-card">
        <span class="setting-label">Storage Namespace</span>
        <div class="setting-card-header">
          <span class="setting-value mono">${settings.namespace || '—'}</span>
        </div>
      </div>
    `;

    // Bind settings interactions
    document.getElementById('btn-open-wifi-config')?.addEventListener('click', () => {
      configModal.classList.add('active');
    });

    document.getElementById('btn-save-temp')?.addEventListener('click', () => {
      const temp = document.getElementById('input-target-temp').value;
      updateSetting({ temp });
    });

    document.getElementById('input-led-toggle')?.addEventListener('change', (e) => {
      const led = e.target.checked ? 'true' : 'false';
      updateSetting({ led });
      // Optimistic UI update
      const display = document.getElementById('display-led-state');
      if (display) display.textContent = e.target.checked ? 'Enabled' : 'Disabled';
    });

    document.getElementById('btn-save-auth')?.addEventListener('click', () => {
      const authUser = document.getElementById('input-auth-user').value;
      const authPass = document.getElementById('input-auth-pass').value;
      const guestPass = document.getElementById('input-guest-pass').value;
      if (!authUser || !authPass || !guestPass) {
        showToast('error', 'Validation Error', 'All credential fields are required');
        return;
      }
      updateSetting({ authUser, authPass, guestPass });
    });
  }

  function renderSettingsFallback() {
    settingsContainer.innerHTML = `
      <div class="setting-card" style="grid-column: 1 / -1;">
        <span class="setting-label" style="color: var(--text-muted);">Settings endpoint not available</span>
        <span class="setting-value" style="font-size: 13px; color: var(--text-secondary);">
          The /api/settings endpoint is not responding. Settings may not be configured on this device.
        </span>
      </div>
    `;
  }

  // ======================== Load Attach Modal ========================

  function openLoadModal(switchPin, switchName, currentLoadPin) {
    // We store the pin in a data attribute to use for the API call, but show the friendly name
    inputLoadSwitchPin.dataset.pin = switchPin;
    inputLoadSwitchPin.value = switchName;

    // Populate load dropdown from cached loads
    inputLoadPin.innerHTML = '<option value="-1">— Detach (No Load) —</option>';
    cachedLoads.forEach((load, index) => {
      const selected = load.pin === currentLoadPin ? 'selected' : '';
      inputLoadPin.innerHTML += `<option value="${load.pin}" ${selected}>Load ${index + 1} (${load.state ? 'ON' : 'OFF'})</option>`;
    });

    loadModal.classList.add('active');
  }

  // ======================== Modal Events ========================

  // IR Modal
  closeIrModalBtn.addEventListener('click', () => {
    irModal.classList.remove('active');
  });

  irForm.addEventListener('submit', async (e) => {
    e.preventDefault();
    const slot = inputIrSlot.value;
    const deviceId = inputIrDevice.value;
    const name = inputIrName.value;

    const submitBtn = document.getElementById('btn-submit-ir');
    submitBtn.textContent = 'Recording...';
    submitBtn.disabled = true;

    try {
      const res = await fetch(getApiUrl(`/api/ir/record?slot=${slot}&name=${encodeURIComponent(name)}`), { method: 'POST' });
      const data = await res.json().catch(() => ({}));
      if (res.ok) {
        showToast('success', 'IR Recorded', data.message || `Command "${name}" saved to slot ${slot}.`);
        irModal.classList.remove('active');
        pollDeviceData(); // Refresh UI
      } else {
        showToast('error', 'Record Failed', data.error || 'Device returned an error.');
      }
    } catch (err) {
      console.error(err);
      showToast('error', 'Network Error', 'Could not reach the device while recording.');
    } finally {
      submitBtn.textContent = 'Start Recording';
      submitBtn.disabled = false;
    }
  });

  // Config Modal (WiFi Save)
  configBtn.addEventListener('click', () => {
    configModal.classList.add('active');
  });

  closeConfigBtn.addEventListener('click', () => {
    configModal.classList.remove('active');
  });

  configForm.addEventListener('submit', async (e) => {
    e.preventDefault();
    const ssid = document.getElementById('input-ssid').value;
    const pass = document.getElementById('input-pass').value;

    const saveBtn = document.getElementById('btn-save-wifi');
    saveBtn.textContent = 'Saving...';
    saveBtn.disabled = true;

    try {
      const res = await fetch(getApiUrl(`/api/save?ssid=${encodeURIComponent(ssid)}&pass=${encodeURIComponent(pass)}`), {
        method: 'POST'
      });
      const data = await res.json().catch(() => ({}));
      if (res.ok) {
        showToast('success', 'WiFi Saved', data.message || 'Credentials saved. Device will reboot.');
        configModal.classList.remove('active');
        // Device will reboot — show reconnecting state
        wifiBadge.textContent = 'Rebooting...';
        statusDot.className = 'status-dot connecting';
        statusText.textContent = 'Device is rebooting with new credentials...';
      } else {
        showToast('error', 'Save Failed', data.error || 'Device returned an error.');
      }
    } catch (err) {
      console.error(err);
      showToast('error', 'Network Error', 'Could not reach the device.');
    } finally {
      saveBtn.textContent = 'Save to Node';
      saveBtn.disabled = false;
    }
  });

  // Load Modal
  closeLoadModalBtn.addEventListener('click', () => {
    loadModal.classList.remove('active');
  });

  loadForm.addEventListener('submit', async (e) => {
    e.preventDefault();
    const switchPin = inputLoadSwitchPin.dataset.pin;
    const loadPin = inputLoadPin.value;

    const submitBtn = document.getElementById('btn-submit-load');
    submitBtn.textContent = 'Applying...';
    submitBtn.disabled = true;

    try {
      const res = await fetch(getApiUrl(`/api/switch/load?pin=${switchPin}&loadPin=${loadPin}`), {
        method: 'POST'
      });
      const data = await res.json().catch(() => ({}));
      if (res.ok && data.success) {
        const action = parseInt(loadPin) < 0 ? 'detached from' : `attached to`;
        showToast('success', 'Load Updated', `Load ${action} switch pin ${switchPin}.`);
        loadModal.classList.remove('active');
        pollDeviceData(); // Refresh
      } else {
        showToast('error', 'Load Update Failed', data.error || 'Device returned an error.');
      }
    } catch (err) {
      console.error(err);
      showToast('error', 'Network Error', 'Could not reach the device.');
    } finally {
      submitBtn.textContent = 'Apply';
      submitBtn.disabled = false;
    }
  });

  // Scene Modal
  btnAddScene?.addEventListener('click', () => {
    inputSceneName.value = '';
    
    if (!cachedLoads || cachedLoads.length === 0) {
      sceneLoadsContainer.innerHTML = '<span style="font-size:12px; color:var(--text-secondary);">No loads available to control.</span>';
    } else {
      let html = '';
      cachedLoads.forEach((load, i) => {
        html += `
          <div class="scene-load-item">
            <span style="font-size:13px; font-weight:600;">Load ${i+1} <span style="font-size:10px; color:var(--text-secondary); font-weight:normal;">(Pin ${load.pin})</span></span>
            <select class="scene-load-select" data-pin="${load.pin}">
              <option value="ignore" selected>Ignore</option>
              <option value="1">Turn ON</option>
              <option value="0">Turn OFF</option>
            </select>
          </div>
        `;
      });
      sceneLoadsContainer.innerHTML = html;
    }
    
    sceneModal.classList.add('active');
  });

  closeSceneModalBtn?.addEventListener('click', () => {
    sceneModal.classList.remove('active');
  });

  sceneForm?.addEventListener('submit', async (e) => {
    e.preventDefault();
    const name = inputSceneName.value.trim();
    
    // Gather actions
    const selects = sceneLoadsContainer.querySelectorAll('.scene-load-select');
    const actions = [];
    selects.forEach(sel => {
      if (sel.value !== 'ignore') {
        actions.push(`${sel.dataset.pin}:${sel.value}`);
      }
    });
    
    const actionsStr = actions.join(',');
    
    const submitBtn = document.getElementById('btn-submit-scene');
    submitBtn.textContent = 'Saving...';
    submitBtn.disabled = true;

    try {
      const res = await fetch(getApiUrl(`/api/scenes?name=${encodeURIComponent(name)}&actions=${encodeURIComponent(actionsStr)}`), {
        method: 'POST'
      });
      const data = await res.json().catch(() => ({}));
      if (res.ok) {
        showToast('success', 'Scene Created', data.message || 'Scene saved.');
        sceneModal.classList.remove('active');
        pollDeviceData(); // Refresh UI
      } else {
        showToast('error', 'Save Failed', data.error || 'Server error');
      }
    } catch (err) {
      console.error(err);
      showToast('error', 'Network Error', 'Could not save scene.');
    } finally {
      submitBtn.textContent = 'Save Scene';
      submitBtn.disabled = false;
    }
  });

  // Close modals on overlay click
  [configModal, irModal, loadModal, sceneModal].forEach(modal => {
    if (!modal) return;
    modal.addEventListener('click', (e) => {
      if (e.target === modal) modal.classList.remove('active');
    });
  });

  // Close modals on Escape key
  document.addEventListener('keydown', (e) => {
    if (e.key === 'Escape') {
      configModal?.classList.remove('active');
      irModal?.classList.remove('active');
      loadModal?.classList.remove('active');
      sceneModal?.classList.remove('active');
    }
  });

  // ======================== OTA Update ========================

  const inputOtaFile = document.getElementById('input-ota-file');
  const btnSelectOta = document.getElementById('btn-select-ota');
  const btnUploadOta = document.getElementById('btn-upload-ota');
  const otaFileName = document.getElementById('ota-file-name');

  btnSelectOta?.addEventListener('click', () => {
    inputOtaFile.click();
  });

  inputOtaFile?.addEventListener('change', (e) => {
    const file = e.target.files[0];
    if (file) {
      otaFileName.textContent = file.name;
      btnUploadOta.disabled = false;
    } else {
      otaFileName.textContent = '';
      btnUploadOta.disabled = true;
    }
  });

  btnUploadOta?.addEventListener('click', async () => {
    const file = inputOtaFile.files[0];
    if (!file) return;

    btnUploadOta.textContent = 'Uploading...';
    btnUploadOta.disabled = true;
    btnSelectOta.disabled = true;

    const formData = new FormData();
    formData.append('update', file);

    try {
      showToast('info', 'Uploading Firmware', 'Please wait. Do not close this page.', 5000);
      
      const res = await fetch(getApiUrl('/update'), {
        method: 'POST',
        body: formData
      });
      
      const data = await res.json().catch(() => ({}));
      
      if (res.ok) {
        showToast('success', 'Update Successful', data.message || 'Device is rebooting. It will be back online shortly.', 10000);
        
        // Show rebooting state
        wifiBadge.textContent = 'Rebooting...';
        statusDot.className = 'status-dot connecting';
        statusText.textContent = 'Device is flashing and rebooting...';
        
        // Reset form
        inputOtaFile.value = '';
        otaFileName.textContent = '';
        btnUploadOta.textContent = 'Upload & Flash';
        btnSelectOta.disabled = false;
        
      } else {
        showToast('error', 'Update Failed', data.error || 'Server returned an error.');
        btnUploadOta.textContent = 'Upload & Flash';
        btnUploadOta.disabled = false;
        btnSelectOta.disabled = false;
      }
    } catch (err) {
      console.error(err);
      showToast('error', 'Network Error', 'Connection lost. The device might be rebooting.');
      wifiBadge.textContent = 'Rebooting...';
      statusDot.className = 'status-dot connecting';
      statusText.textContent = 'Device might be rebooting...';
      btnUploadOta.textContent = 'Upload & Flash';
      btnUploadOta.disabled = false;
      btnSelectOta.disabled = false;
    }
  });

  // START THE APPLICATION
  initialHealthCheck();
});