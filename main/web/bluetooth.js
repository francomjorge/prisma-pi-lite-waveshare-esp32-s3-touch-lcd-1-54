(() => {
  'use strict';

  let btTimer = null;
  let installed = false;

  const el = id => document.getElementById(id);
  const yesno = v => v ? 'YES' : 'NO';
  const stateClass = (node, kind) => {
    if (!node) return;
    node.className = 'bigState ' + kind;
  };

  function installCss() {
    if (document.querySelector('link[href="/bluetooth.css"]')) return;
    const link = document.createElement('link');
    link.rel = 'stylesheet';
    link.href = '/bluetooth.css';
    document.head.appendChild(link);
  }

  function installBluetoothPane() {
    if (el('tab-bluetooth')) return;
    const pane = document.createElement('section');
    pane.id = 'tab-bluetooth';
    pane.className = 'tabPane';
    pane.innerHTML = `
      <div class="grid">
        <section class="card">
          <div class="eyebrow">BLE RADIO</div>
          <h2>Bluetooth Low Energy</h2>
          <div id="btState" class="bigState pending">CHECKING</div>
          <div class="kv">
            <label>Stack</label><strong id="btStack">--</strong>
            <label>Mode</label><strong>BLE ONLY</strong>
            <label>Device name</label><strong id="btLocalName">--</strong>
            <label>BLE address</label><strong id="btLocalAddress">--</strong>
            <label>Classic Bluetooth</label><strong>NOT SUPPORTED</strong>
            <label>A2DP headphones/audio</label><strong>NOT SUPPORTED</strong>
          </div>
          <button onclick="loadBluetooth()">REFRESH</button>
        </section>

        <section class="card">
          <div class="eyebrow">DISCOVERABILITY</div>
          <h2>Peripheral / advertising</h2>
          <div id="btAdvertisingState" class="bigState pending">CHECKING</div>
          <p class="lead">Hace visible PRISMA-PI LITE para que otro dispositivo BLE pueda encontrarlo y conectarse.</p>
          <div class="kv">
            <label>Requested</label><strong id="btAdvertisingRequested">--</strong>
            <label>Radio advertising</label><strong id="btAdvertising">--</strong>
          </div>
          <div class="buttonRow">
            <button onclick="btSetAdvertising(true)">ADVERTISE ON</button>
            <button class="ghost" onclick="btSetAdvertising(false)">ADVERTISE OFF</button>
          </div>
          <div id="btAdvertisingAction" class="status"></div>
        </section>

        <section class="card wide">
          <div class="filesHead">
            <div>
              <div class="eyebrow">BLE SCANNER</div>
              <h2>Nearby devices</h2>
              <p>Escaneo activo BLE. Muestra nombre, dirección, RSSI y si el anuncio permite conexión.</p>
            </div>
            <div><strong id="btScanCount">0 DEVICES</strong><small id="btScanState">IDLE</small></div>
          </div>
          <div class="buttonRow">
            <button id="btScanBtn" onclick="btStartScan()">SCAN 6 SEC</button>
            <button class="ghost" onclick="btStopScan()">STOP SCAN</button>
          </div>
          <div id="btScanAction" class="status"></div>
          <div id="btDevices" class="btDeviceList"><div class="empty">Press SCAN 6 SEC.</div></div>
        </section>

        <section class="card">
          <div class="eyebrow">CONNECTION</div>
          <h2>Current BLE peer</h2>
          <div id="btConnectionState" class="bigState disconnected">IDLE</div>
          <div class="kv">
            <label>Name</label><strong id="btPeerName">NONE</strong>
            <label>Address</label><strong id="btPeerAddress">--</strong>
            <label>RSSI</label><strong id="btPeerRssi">--</strong>
            <label>ATT MTU</label><strong id="btMtu">--</strong>
            <label>Encrypted</label><strong id="btEncrypted">NO</strong>
            <label>Authenticated</label><strong id="btAuthenticated">NO</strong>
            <label>Bonded</label><strong id="btBonded">NO</strong>
          </div>
          <div class="buttonRow">
            <button id="btPairBtn" onclick="btPair()">PAIR / BOND</button>
            <button id="btDisconnectBtn" class="danger" onclick="btDisconnect()">DISCONNECT</button>
          </div>
          <div id="btConnectionAction" class="status"></div>
        </section>

        <section class="card">
          <div class="eyebrow">CAPABILITIES</div>
          <h2>BLE roles</h2>
          <div class="btCapabilities">
            <span id="btCapCentral">CENTRAL</span>
            <span id="btCapPeripheral">PERIPHERAL</span>
            <span id="btCapObserver">OBSERVER</span>
            <span id="btCapGattClient">GATT CLIENT</span>
            <span id="btCapGattServer">GATT SERVER</span>
            <span id="btCapBonding">BONDING</span>
          </div>
          <div class="kv btCapabilityKv">
            <label>BLE MIDI profile</label><strong id="btMidiProfile">NOT LOADED YET</strong>
            <label>Classic profiles</label><strong>UNAVAILABLE ON ESP32-S3</strong>
            <label>Wi-Fi coexistence</label><strong>2.4 GHz SHARED RADIO</strong>
            <label>Last BLE error</label><strong id="btError">NONE</strong>
          </div>
          <p class="hint">Esta etapa incorpora BLE de propósito general: descubrimiento, conexión, advertising y pairing/bonding. Los perfiles de aplicación se agregan encima de esta base.</p>
        </section>
      </div>`;
    document.querySelector('main').appendChild(pane);
  }

  function installCoreCard() {
    const grid = document.querySelector('#tab-core .grid');
    if (!grid || el('coreBtState')) return;
    const card = document.createElement('section');
    card.className = 'card';
    card.innerHTML = `
      <div class="eyebrow">BLUETOOTH</div>
      <h2>BLE connectivity</h2>
      <div id="coreBtState" class="bigState pending">CHECKING</div>
      <div class="kv">
        <label>Radio</label><strong id="coreBtRadio">--</strong>
        <label>Advertising</label><strong id="coreBtAdvertising">--</strong>
        <label>Peer</label><strong id="coreBtPeer">NONE</strong>
        <label>RSSI</label><strong id="coreBtRssi">--</strong>
      </div>
      <button onclick="showTab('bluetooth')">OPEN BLUETOOTH</button>`;
    const roadmap = grid.querySelector('.roadmap');
    grid.insertBefore(card, roadmap || null);
  }

  function enableTab() {
    const tab = [...document.querySelectorAll('.tabs button')]
      .find(b => b.textContent.trim() === 'BLUETOOTH');
    if (!tab) return;
    tab.disabled = false;
    tab.classList.remove('disabled');
    tab.dataset.tab = 'bluetooth';
    tab.onclick = () => window.showTab('bluetooth');
  }

  function markRoadmap() {
    document.querySelectorAll('.roadmapGrid span').forEach(s => {
      if (s.textContent.trim() === 'A5 BLE') s.classList.add('activeStep');
    });
    const header = document.querySelector('header .brand small');
    if (header) header.textContent = 'LITE / FACTORY AUDIO V1 + A5 BLE';
  }

  function buildDeviceRow(d) {
    const row = document.createElement('div');
    row.className = 'btDeviceRow';

    const identity = document.createElement('div');
    identity.className = 'btIdentity';
    const title = document.createElement('b');
    title.textContent = d.name || 'UNNAMED';
    const sub = document.createElement('small');
    sub.textContent = `${d.address || '--'} · type ${d.addr_type ?? '--'}`;
    identity.append(title, sub);

    const info = document.createElement('div');
    info.className = 'btRowInfo';
    const rssi = document.createElement('span');
    rssi.className = 'signal';
    rssi.textContent = Number.isFinite(Number(d.rssi)) ? `${d.rssi} dBm` : '--';
    const badge = document.createElement('span');
    badge.className = 'statusPill ' + (d.connectable ? 'on' : 'off');
    badge.textContent = d.connectable ? 'CONNECTABLE' : 'BEACON';
    const button = document.createElement('button');
    button.textContent = 'CONNECT';
    button.disabled = !d.connectable;
    button.onclick = () => btConnect(d.address, d.addr_type);
    info.append(rssi, badge, button);

    row.append(identity, info);
    return row;
  }

  function paintCapabilities(c = {}) {
    const toggle = (id, on) => {
      const n = el(id);
      if (!n) return;
      n.classList.toggle('off', !on);
    };
    toggle('btCapCentral', !!c.central);
    toggle('btCapPeripheral', !!c.peripheral);
    toggle('btCapObserver', !!c.observer);
    toggle('btCapGattClient', !!c.gatt_client);
    toggle('btCapGattServer', !!c.gatt_server);
    toggle('btCapBonding', !!c.bonding);
    if (el('btMidiProfile')) el('btMidiProfile').textContent = c.ble_midi_profile ? 'LOADED' : 'NOT LOADED YET';
  }

  function paintBluetooth(s) {
    const ready = !!(s.initialized && s.synced);
    const btState = el('btState');
    btState.textContent = ready ? 'READY' : (s.initialized ? 'SYNCING' : 'NOT READY');
    stateClass(btState, ready ? 'connected' : 'pending');
    el('btStack').textContent = s.synced ? 'NIMBLE READY' : (s.initialized ? 'STARTING' : 'OFFLINE');
    el('btLocalName').textContent = s.device_name || '--';
    el('btLocalAddress').textContent = s.local_address || '--';

    const advOn = !!s.advertising;
    el('btAdvertisingState').textContent = advOn ? 'VISIBLE' : 'IDLE';
    stateClass(el('btAdvertisingState'), advOn ? 'connected' : 'disconnected');
    el('btAdvertisingRequested').textContent = yesno(!!s.advertise_requested);
    el('btAdvertising').textContent = advOn ? 'ACTIVE' : 'STOPPED';

    const connText = s.connected ? 'CONNECTED' : (s.connecting ? 'CONNECTING' : 'IDLE');
    el('btConnectionState').textContent = connText;
    stateClass(el('btConnectionState'), s.connected ? 'connected' : (s.connecting ? 'pending' : 'disconnected'));
    el('btPeerName').textContent = s.peer_name || 'NONE';
    el('btPeerAddress').textContent = s.peer_address || '--';
    el('btPeerRssi').textContent = s.connected ? `${s.peer_rssi} dBm` : '--';
    el('btMtu').textContent = s.connected ? String(s.mtu || 23) : '--';
    el('btEncrypted').textContent = yesno(!!s.encrypted);
    el('btAuthenticated').textContent = yesno(!!s.authenticated);
    el('btBonded').textContent = yesno(!!s.bonded);
    el('btPairBtn').disabled = !s.connected || !!s.bonded;
    el('btDisconnectBtn').disabled = !s.connected;
    el('btError').textContent = s.last_error || 'NONE';

    const devices = Array.isArray(s.devices) ? s.devices : [];
    el('btScanCount').textContent = `${devices.length} DEVICE${devices.length === 1 ? '' : 'S'}`;
    el('btScanState').textContent = s.scanning ? 'SCANNING' : 'IDLE';
    el('btScanBtn').disabled = !!s.scanning || !!s.connecting;
    const box = el('btDevices');
    box.innerHTML = '';
    if (!devices.length) {
      box.innerHTML = `<div class="empty">${s.scanning ? 'Scanning…' : 'No BLE devices in the current scan list.'}</div>`;
    } else {
      devices.sort((a, b) => (Number(b.rssi) || -127) - (Number(a.rssi) || -127));
      devices.forEach(d => box.appendChild(buildDeviceRow(d)));
    }

    paintCapabilities(s.capabilities || {});

    if (el('coreBtState')) {
      el('coreBtState').textContent = s.connected ? 'CONNECTED' : (ready ? 'READY' : 'CHECK');
      stateClass(el('coreBtState'), s.connected ? 'connected' : (ready ? 'disconnected' : 'pending'));
      el('coreBtRadio').textContent = ready ? 'BLE READY' : 'STARTING';
      el('coreBtAdvertising').textContent = advOn ? 'VISIBLE' : 'OFF';
      el('coreBtPeer').textContent = s.connected ? (s.peer_name || s.peer_address || 'CONNECTED') : 'NONE';
      el('coreBtRssi').textContent = s.connected ? `${s.peer_rssi} dBm` : '--';
    }
  }

  window.loadBluetooth = async function loadBluetooth() {
    try {
      const s = await json('/api/bluetooth/status?x=' + Date.now());
      paintBluetooth(s);
      return s;
    } catch (e) {
      if (el('btError')) el('btError').textContent = e.message;
      if (el('btState')) {
        el('btState').textContent = 'ERROR';
        stateClass(el('btState'), 'pending');
      }
      return null;
    }
  };

  window.btStartScan = async function btStartScan() {
    try {
      el('btScanAction').textContent = 'Starting 6 second BLE scan…';
      await postForm('/api/bluetooth/scan', {seconds: 6});
      await loadBluetooth();
      startBtPoll();
    } catch (e) {
      el('btScanAction').textContent = 'Scan failed: ' + e.message;
    }
  };

  window.btStopScan = async function btStopScan() {
    try {
      await postForm('/api/bluetooth/scan-stop');
      el('btScanAction').textContent = 'Scan stopped.';
      await loadBluetooth();
    } catch (e) {
      el('btScanAction').textContent = 'Stop failed: ' + e.message;
    }
  };

  window.btConnect = async function btConnect(address, addrType) {
    try {
      el('btConnectionAction').textContent = 'Connecting to ' + address + '…';
      await postForm('/api/bluetooth/connect', {address, addr_type: addrType});
      await loadBluetooth();
      startBtPoll();
    } catch (e) {
      el('btConnectionAction').textContent = 'Connect failed: ' + e.message;
    }
  };

  window.btDisconnect = async function btDisconnect() {
    try {
      await postForm('/api/bluetooth/disconnect');
      el('btConnectionAction').textContent = 'Disconnect requested.';
      setTimeout(loadBluetooth, 250);
    } catch (e) {
      el('btConnectionAction').textContent = 'Disconnect failed: ' + e.message;
    }
  };

  window.btPair = async function btPair() {
    try {
      await postForm('/api/bluetooth/pair');
      el('btConnectionAction').textContent = 'Pairing / bonding started.';
      startBtPoll();
    } catch (e) {
      el('btConnectionAction').textContent = 'Pairing failed: ' + e.message;
    }
  };

  window.btSetAdvertising = async function btSetAdvertising(enabled) {
    try {
      await postForm('/api/bluetooth/advertise', {enabled: enabled ? 1 : 0});
      el('btAdvertisingAction').textContent = enabled ? 'Advertising enabled.' : 'Advertising disabled.';
      await loadBluetooth();
    } catch (e) {
      el('btAdvertisingAction').textContent = 'Advertising change failed: ' + e.message;
    }
  };

  function startBtPoll() {
    if (!btTimer) btTimer = setInterval(loadBluetooth, 1000);
  }

  function stopBtPoll() {
    if (btTimer) {
      clearInterval(btTimer);
      btTimer = null;
    }
  }

  function installNavigationHook() {
    if (installed) return;
    installed = true;
    const previousShowTab = window.showTab;
    window.showTab = function(name) {
      previousShowTab(name);
      if (name === 'bluetooth') {
        location.hash = 'bluetooth';
        loadBluetooth();
        startBtPoll();
      } else {
        if (location.hash === '#bluetooth') history.replaceState(null, '', location.pathname);
        stopBtPoll();
      }
    };
  }

  function initBluetoothAdmin() {
    installCss();
    installBluetoothPane();
    installCoreCard();
    enableTab();
    markRoadmap();
    installNavigationHook();
    loadBluetooth();
    if (location.hash === '#bluetooth') setTimeout(() => window.showTab('bluetooth'), 0);
  }

  if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', initBluetoothAdmin);
  else initBluetoothAdmin();
})();
