// AURA Hardware Dashboard — Full API Integration
// Integrates all endpoints from the ESP32 SmartWebServer

const DEVICE_IP = '';

function getApiUrl(path) {
  return DEVICE_IP + path;
}

let pollingInterval = null;
let cachedLoads = []; // Keep load list for the load-attach modal

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

document.addEventListener('DOMContentLoaded', () => {
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

  // ======================== Initial Health Check ========================

  async function initialHealthCheck() {
    loadingMessage.textContent = 'Checking device status...';
    try {
      const res = await fetch(getApiUrl('/api/status'), { signal: AbortSignal.timeout(5000) });
      if (!res.ok) throw new Error('Status check failed');
      const data = await res.json();

      if (data.status === 'operational') {
        loadingMessage.textContent = 'Device online. Loading dashboard...';
        // Transition to dashboard
        await fetchDeviceData();
        await fetchSettings();
      } else {
        loadingMessage.textContent = 'Device in provisioning mode. Opening config...';
        showDashboard();
        configModal.classList.add('active');
      }
    } catch (err) {
      console.warn('Health check failed, trying direct room fetch...', err);
      loadingMessage.textContent = 'Retrying connection...';
      // Fall back to fetching room data directly
      try {
        await fetchDeviceData();
        await fetchSettings();
      } catch (err2) {
        loadingMessage.innerHTML = '<span style="color:#ff5e62">Device Offline.</span><br><span style="color:#9ea4bb;font-size:12px">Check that ' + DEVICE_IP + ' is reachable</span>';
        // Retry every 5 seconds
        setTimeout(initialHealthCheck, 5000);
      }
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

    // On first successful load, show the dashboard and start polling
    if (loadingOverlay.style.display !== 'none') {
      showDashboard();
      startPolling();
    }

    // Update connection status
    wifiBadge.textContent = 'Node Online';
    wifiBadge.classList.remove('offline');
    statusDot.className = 'status-dot online';
    statusText.textContent = 'Connected — polling every 3s';

    renderSensors(data.sensors);
    renderSwitches(data.switches);
    renderLoads(data.loads);
    if (data.controllers && data.controllers.ir) {
      renderIRCommands(data.controllers.ir.commands);
    }

    // Cache loads for the load-attach modal
    cachedLoads = data.loads || [];

    return data;
  }

  async function pollDeviceData() {
    try {
      await fetchDeviceData();
    } catch (err) {
      console.warn('Poll failed:', err);
      wifiBadge.textContent = 'Offline';
      wifiBadge.classList.add('offline');
      statusDot.className = 'status-dot offline';
      statusText.textContent = 'Connection lost — retrying...';
    }
  }

  function startPolling() {
    if (pollingInterval) clearInterval(pollingInterval);
    pollingInterval = setInterval(pollDeviceData, 3000);
  }

  // ======================== Settings API ========================

  async function fetchSettings() {
    try {
      const res = await fetch(getApiUrl('/api/settings'));
      if (!res.ok) throw new Error('Settings fetch failed');
      const data = await res.json();
      renderSettings(data);
      settingsStatusBadge.textContent = 'Loaded';

      // Pre-fill WiFi config modal with current SSID
      const ssidInput = document.getElementById('input-ssid');
      if (data.wifiSSID && ssidInput) {
        ssidInput.value = data.wifiSSID;
      }
    } catch (err) {
      console.warn('Settings fetch failed:', err);
      settingsStatusBadge.textContent = 'Unavailable';
      renderSettingsFallback();
    }
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

  // ======================== UI Rendering ========================

  function renderSensors(sensors) {
    if (!sensors) return;

    let html = '';

    // Climate
    if (sensors.climate) {
      const tempColor = sensors.climate.temperature > 30 ? '#ff5e62' : sensors.climate.temperature > 25 ? '#ff9966' : '#38ef7d';
      html += `
        <div class="sensor-card">
          <span class="sensor-icon">🌡️</span>
          <span class="sensor-label">Temperature</span>
          <span class="sensor-value" style="color: ${tempColor}">${sensors.climate.temperature.toFixed(1)}°C</span>
        </div>
        <div class="sensor-card">
          <span class="sensor-icon">💧</span>
          <span class="sensor-label">Humidity</span>
          <span class="sensor-value">${sensors.climate.humidity.toFixed(1)}%</span>
        </div>
      `;
    }
    // Light
    if (sensors.light) {
      const lightIcon = sensors.light.percentage > 50 ? '☀️' : '🌙';
      html += `
        <div class="sensor-card">
          <span class="sensor-icon">${lightIcon}</span>
          <span class="sensor-label">Ambient Light</span>
          <span class="sensor-value">${sensors.light.percentage.toFixed(1)}%</span>
        </div>
      `;
    }
    // Presence
    if (sensors.presence) {
      const motionText = sensors.presence.motion ? 'Detected' : 'Clear';
      const color = sensors.presence.motion ? '#38ef7d' : '#9ea4bb';
      const icon = sensors.presence.motion ? '🚶' : '🔒';
      html += `
        <div class="sensor-card">
          <span class="sensor-icon">${icon}</span>
          <span class="sensor-label">Presence</span>
          <span class="sensor-value" style="color: ${color}">${motionText}</span>
        </div>
      `;
    }

    sensorsContainer.innerHTML = html;
  }

  function renderSwitches(switches) {
    if (!switches) return;
    switchCountBadge.textContent = `${switches.length} Configured`;

    // Only rebuild DOM if the number of switches changed to avoid focus loss
    if (switchesContainer.children.length !== switches.length) {
      switchesContainer.innerHTML = '';
      switches.forEach(sw => {
        const loadState = sw.hasLoad && sw.load ? sw.load.state : false;
        const isActive = sw.hasLoad ? loadState : (sw.state === 1);

        const div = document.createElement('div');
        div.className = `switch-card ${isActive ? 'active' : ''}`;
        div.id = `card-switch-${sw.pin}`;

        const loadBadge = sw.hasLoad
          ? `<span class="load-badge">⚡ Load ${sw.load ? sw.load.pin : '?'}</span>`
          : `<span class="load-badge" style="opacity:0.4">No Load</span>`;

        div.innerHTML = `
          <div class="switch-header">
            <h3>Switch ${sw.pin}</h3>
            ${loadBadge}
          </div>
          <div class="switch-actions">
            <span class="pin-label">PIN: ${sw.pin}</span>
            <button class="action-btn trigger-btn" data-pin="${sw.pin}" data-isactive="${isActive}">Toggle</button>
            <button class="action-btn small load-attach-btn" data-pin="${sw.pin}" data-has-load="${sw.hasLoad}" data-load-pin="${sw.hasLoad && sw.load ? sw.load.pin : -1}" title="Manage load">⚡</button>
          </div>
        `;
        switchesContainer.appendChild(div);
      });

      // Bind toggle events
      document.querySelectorAll('.trigger-btn').forEach(el => {
        el.addEventListener('click', async (e) => {
          const pin = e.target.dataset.pin;
          e.target.textContent = '...';

          try {
            const res = await fetch(getApiUrl(`/api/switch/state?pin=${pin}`), { method: 'POST' });
            if (res.ok) {
              showToast('success', 'Switch Toggled', `Switch pin ${pin} state changed.`);
            } else {
              showToast('error', 'Toggle Failed', 'Device returned an error.');
            }
            setTimeout(() => { if (e.target) e.target.textContent = 'Toggle'; }, 400);
            pollDeviceData(); // force fast update
          } catch (err) {
            console.error('Toggle failed', err);
            if (e.target) e.target.textContent = 'Toggle';
            showToast('error', 'Network Error', 'Could not reach the device.');
          }
        });
      });

      // Bind load-attach events
      document.querySelectorAll('.load-attach-btn').forEach(el => {
        el.addEventListener('click', (e) => {
          const pin = e.currentTarget.dataset.pin;
          const currentLoadPin = parseInt(e.currentTarget.dataset.loadPin);
          openLoadModal(pin, currentLoadPin);
        });
      });
    } else {
      // Just update states smoothly without rebuilding DOM
      switches.forEach(sw => {
        const card = document.getElementById(`card-switch-${sw.pin}`);
        if (card) {
          const loadState = sw.hasLoad && sw.load ? sw.load.state : (sw.state === 1);
          if (loadState) card.classList.add('active');
          else card.classList.remove('active');

          const btn = card.querySelector('.trigger-btn');
          if (btn) {
            const isActive = sw.hasLoad && sw.load ? sw.load.state : (sw.state === 1);
            btn.dataset.isactive = isActive;
          }

          // Update load badge
          const loadBadgeEl = card.querySelector('.load-badge');
          if (loadBadgeEl) {
            if (sw.hasLoad && sw.load) {
              loadBadgeEl.textContent = `⚡ Load ${sw.load.pin}`;
              loadBadgeEl.style.opacity = '1';
            } else {
              loadBadgeEl.textContent = 'No Load';
              loadBadgeEl.style.opacity = '0.4';
            }
          }

          // Update load-attach button data
          const attachBtn = card.querySelector('.load-attach-btn');
          if (attachBtn) {
            attachBtn.dataset.hasLoad = sw.hasLoad;
            attachBtn.dataset.loadPin = sw.hasLoad && sw.load ? sw.load.pin : -1;
          }
        }
      });
    }
  }

  function renderLoads(loads) {
    if (!loads || loads.length === 0) {
      loadCountBadge.textContent = '0 Registered';
      loadsContainer.innerHTML = '<div class="ir-empty-state">No loads registered on this node.</div>';
      return;
    }

    loadCountBadge.textContent = `${loads.length} Registered`;

    // Check if data changed to avoid rebuilding
    const loadsJson = JSON.stringify(loads);
    if (loadsContainer.dataset.lastJson === loadsJson) return;
    loadsContainer.dataset.lastJson = loadsJson;

    loadsContainer.innerHTML = '';
    loads.forEach(load => {
      const isOn = load.state === true;
      const div = document.createElement('div');
      div.className = `load-card ${isOn ? 'active' : ''}`;
      div.id = `card-load-${load.pin}`;

      div.innerHTML = `
        <div class="load-header">
          <h3>Load ${load.pin}</h3>
          <div style="display:flex; gap:10px; align-items:center;">
            <div class="load-state-dot ${isOn ? 'on' : 'off'}"></div>
            <button class="action-btn load-trigger-btn" data-pin="${load.pin}">Toggle</button>
          </div>
        </div>
        <div class="load-meta">
          <span class="load-meta-item"><span class="pin-label">PIN: ${load.pin}</span></span>
          <span class="load-meta-item" style="color: ${load.activeLow ? '#ff9966' : 'var(--text-muted)'}">
            ${load.activeLow ? '⚡ Active-Low' : 'Active-High'}
          </span>
          <span class="load-meta-item" style="color: ${isOn ? 'var(--accent-primary)' : 'var(--text-muted)'}">
            ${isOn ? '● ON' : '○ OFF'}
          </span>
        </div>
      `;

      loadsContainer.appendChild(div);
    });

    document.querySelectorAll('.load-trigger-btn').forEach(el => {
      el.addEventListener('click', async (e) => {
        const pin = e.target.dataset.pin;
        e.target.textContent = '...';

        try {
          const res = await fetch(getApiUrl(`/api/load/toggle?pin=${pin}`), { method: 'POST' });
          if (res.ok) {
            showToast('success', 'Load Toggled', `Load pin ${pin} toggled.`);
          } else {
            showToast('error', 'Toggle Failed', 'Device returned an error.');
          }
          setTimeout(() => { if (e.target) e.target.textContent = 'Toggle'; }, 400);
          pollDeviceData(); // force fast update
        } catch (err) {
          console.error('Toggle failed', err);
          if (e.target) e.target.textContent = 'Toggle';
          showToast('error', 'Network Error', 'Could not reach the device.');
        }
      });
    });
  }

  function renderIRCommands(commands) {
    if (!commands || commands.length === 0) {
      irContainer.innerHTML = '<div class="ir-empty-state">No IR commands recorded yet.</div>';
      irContainer.dataset.lastJson = '[]';

      // Update Add button for first command
      btnAddIr.onclick = () => {
        irModalTitle.textContent = 'Record New Command';
        inputIrSlot.value = 0;
        inputIrDevice.value = '';
        inputIrName.value = '';
        irModal.classList.add('active');
      };
      return;
    }

    // Sort commands by slot to keep predictable ordering
    commands.sort((a, b) => parseInt(a.slot) - parseInt(b.slot));

    // Check if the data has actually changed to prevent UI flickering on polling
    const commandsJson = JSON.stringify(commands);
    if (irContainer.dataset.lastJson === commandsJson) {
      return;
    }
    irContainer.dataset.lastJson = commandsJson;

    let maxSlot = 0;
    if (commands.length > 0) {
      maxSlot = Math.max(...commands.map(c => parseInt(c.slot)));
    }

    // Update Add button logic to auto-increment slot
    btnAddIr.onclick = () => {
      irModalTitle.textContent = 'Record New Command';
      inputIrSlot.value = maxSlot + 1;
      inputIrDevice.value = commands.length > 0 ? commands[0].deviceId : '';
      inputIrName.value = '';
      irModal.classList.add('active');
    };

    // Group commands by deviceId
    const grouped = commands.reduce((acc, cmd) => {
      if (!acc[cmd.deviceId]) acc[cmd.deviceId] = [];
      acc[cmd.deviceId].push(cmd);
      return acc;
    }, {});

    irContainer.innerHTML = '';

    Object.keys(grouped).forEach(deviceId => {
      const groupDiv = document.createElement('div');
      groupDiv.className = 'ir-device-group';

      const title = document.createElement('h3');
      title.className = 'ir-device-title';
      title.textContent = deviceId;
      groupDiv.appendChild(title);

      const listDiv = document.createElement('div');
      listDiv.className = 'remote-list';

      grouped[deviceId].forEach(cmd => {
        const div = document.createElement('div');
        div.className = 'ir-item';
        div.innerHTML = `
          <div class="ir-info">
            <strong>${cmd.name}</strong>
            <span>Slot ${cmd.slot} · ${cmd.protocol !== undefined ? 'Protocol ' + cmd.protocol : ''}</span>
          </div>
          <div class="ir-actions">
            <button class="ir-btn-emit" data-slot="${cmd.slot}">Emit</button>
            <button class="ir-btn-edit" data-slot="${cmd.slot}">Edit</button>
          </div>
        `;

        // Bind Emit
        div.querySelector('.ir-btn-emit').addEventListener('click', async (e) => {
          const btn = e.currentTarget;
          btn.textContent = '...';
          try {
            const res = await fetch(getApiUrl(`/api/ir/emit?slot=${cmd.slot}`), { method: 'POST' });
            if (res.ok) {
              showToast('success', 'IR Emitted', `Sent "${cmd.name}" from slot ${cmd.slot}.`);
            } else {
              showToast('error', 'Emit Failed', 'Device returned an error.');
            }
          } catch (err) {
            console.error('IR emit failed', err);
            showToast('error', 'Network Error', 'Could not reach the device.');
          }
          setTimeout(() => btn.textContent = 'Emit', 500);
        });

        // Bind Edit
        div.querySelector('.ir-btn-edit').addEventListener('click', () => {
          irModalTitle.textContent = 'Edit/Re-record Command';
          inputIrSlot.value = cmd.slot;
          inputIrDevice.value = cmd.deviceId;
          inputIrName.value = cmd.name;
          irModal.classList.add('active');
        });

        listDiv.appendChild(div);
      });

      groupDiv.appendChild(listDiv);
      irContainer.appendChild(groupDiv);
    });
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

  function openLoadModal(switchPin, currentLoadPin) {
    inputLoadSwitchPin.value = switchPin;

    // Populate load dropdown from cached loads
    inputLoadPin.innerHTML = '<option value="-1">— Detach (No Load) —</option>';
    cachedLoads.forEach(load => {
      const selected = load.pin === currentLoadPin ? 'selected' : '';
      inputLoadPin.innerHTML += `<option value="${load.pin}" ${selected}>Load Pin ${load.pin} (${load.state ? 'ON' : 'OFF'})</option>`;
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
      const res = await fetch(getApiUrl(`/api/ir/record?slot=${slot}&deviceId=${encodeURIComponent(deviceId)}&name=${encodeURIComponent(name)}`), { method: 'POST' });
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
    const switchPin = inputLoadSwitchPin.value;
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

  // Close modals on overlay click
  [configModal, irModal, loadModal].forEach(modal => {
    modal.addEventListener('click', (e) => {
      if (e.target === modal) modal.classList.remove('active');
    });
  });

  // Close modals on Escape key
  document.addEventListener('keydown', (e) => {
    if (e.key === 'Escape') {
      configModal.classList.remove('active');
      irModal.classList.remove('active');
      loadModal.classList.remove('active');
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