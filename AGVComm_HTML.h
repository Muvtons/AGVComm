/**
 * AGVComm_HTML.h - HTML Pages for AGVComm Library
 * Separated for better organization
 */

#ifndef AGV_COMM_HTML_H
#define AGV_COMM_HTML_H

const char WIFI_SETUP_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>WiFi Setup</title>
    <style>
        body { font-family: Arial, sans-serif; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); display: flex; justify-content: center; align-items: center; min-height: 100vh; margin: 0; padding: 20px; }
        .setup-container { background: white; padding: 40px; border-radius: 10px; box-shadow: 0 10px 25px rgba(0,0,0,0.2); width: 100%; max-width: 500px; }
        h1 { text-align: center; color: #333; margin-bottom: 30px; }
        .form-group { margin-bottom: 20px; }
        label { display: block; margin-bottom: 5px; color: #555; font-weight: bold; }
        input, select { width: 100%; padding: 12px; border: 1px solid #ddd; border-radius: 5px; box-sizing: border-box; font-size: 16px; }
        button { width: 100%; padding: 12px; background: #2ecc71; color: white; border: none; border-radius: 5px; font-size: 16px; font-weight: bold; cursor: pointer; margin-top: 10px; transition: background 0.3s; }
        button:hover { background: #27ae60; }
        .scan-btn { background: #3498db; }
        .scan-btn:hover { background: #2980b9; }
        .message { text-align: center; padding: 10px; margin-top: 10px; border-radius: 5px; display: none; }
        .success { background: #d4edda; color: #155724; }
        .error { background: #f8d7da; color: #721c24; }
        .loading { text-align: center; margin: 10px 0; display: none; }
    </style>
</head>
<body>
    <div class="setup-container">
        <h1>📡 WiFi Setup</h1>
        <form id="wifiForm">
            <div class="form-group">
                <label for="ssid">WiFi Network</label>
                <select id="ssid" required><option value="">-- Select or type below --</option></select>
            </div>
            <div class="form-group">
                <label for="ssid_manual">Or Enter Manually</label>
                <input type="text" id="ssid_manual" placeholder="Enter WiFi name">
            </div>
            <button type="button" class="scan-btn" onclick="scanNetworks()">🔍 Scan Networks</button>
            <div class="loading" id="loading">Scanning...</div>
            <div class="form-group">
                <label for="password">WiFi Password</label>
                <input type="password" id="password" required>
            </div>
            <button type="submit">💾 Save & Connect</button>
            <div class="message" id="message"></div>
        </form>
    </div>
    <script>
        async function scanNetworks() {
            document.getElementById('loading').style.display = 'block';
            const response = await fetch('/scan');
            const networks = await response.json();
            document.getElementById('loading').style.display = 'none';
            const select = document.getElementById('ssid');
            select.innerHTML = '<option value="">-- Select WiFi Network --</option>';
            networks.forEach(network => {
                const option = document.createElement('option');
                option.value = network.ssid;
                option.textContent = `${network.ssid} (${network.rssi} dBm) ${network.secured ? '🔒' : ''}`;
                select.appendChild(option);
            });
        }
        document.getElementById('wifiForm').addEventListener('submit', async function(e) {
            e.preventDefault();
            const ssid = document.getElementById('ssid_manual').value || document.getElementById('ssid').value;
            const password = document.getElementById('password').value;
            const response = await fetch('/savewifi', { method: 'POST', headers: {'Content-Type': 'application/json'}, body: JSON.stringify({ssid, password}) });
            const result = await response.json();
            const msg = document.getElementById('message');
            msg.style.display = 'block';
            if (result.success) { msg.className = 'message success'; msg.textContent = '✅ WiFi saved! Restarting...'; setTimeout(() => location.reload(), 3000); } 
            else { msg.className = 'message error'; msg.textContent = '❌ Failed to save WiFi settings'; }
        });
        window.onload = scanNetworks;
    </script>
</body>
</html>
)rawliteral";

const char MAIN_CONTROL_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>AGV Navigation Controller</title>
    <style>
        body { font-family: Arial, sans-serif; max-width: 800px; margin: 0 auto; padding: 20px; background-color: #f5f5f5; }
        .container { background: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }
        h1 { color: #2c3e50; text-align: center; margin-bottom: 30px; }
        .header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 20px; }
        .logout-btn { background: #e74c3c; color: white; padding: 8px 15px; border: none; border-radius: 5px; cursor: pointer; font-weight: bold; }
        .logout-btn:hover { background: #c0392b; }
        .control-section { margin-bottom: 25px; padding: 15px; border: 1px solid #ddd; border-radius: 5px; }
        .section-title { font-weight: bold; margin-bottom: 10px; color: #34495e; }
        .input-group { margin-bottom: 10px; }
        label { display: inline-block; width: 120px; font-weight: bold; }
        input[type="number"] { width: 80px; padding: 5px; margin: 0 5px; }
        .button-group { margin: 15px 0; }
        button { padding: 10px 15px; margin: 5px; border: none; border-radius: 5px; cursor: pointer; font-weight: bold; transition: background-color 0.3s; }
        .primary-btn { background-color: #3498db; color: white; }
        .primary-btn:hover { background-color: #2980b9; }
        .success-btn { background-color: #2ecc71; color: white; }
        .success-btn:hover { background-color: #27ae60; }
        .warning-btn { background-color: #f39c12; color: white; }
        .warning-btn:hover { background-color: #d35400; }
        .danger-btn { background-color: #e74c3c; color: white; }
        .danger-btn:hover { background-color: #c0392b; }
        .status-section { background-color: #ecf0f1; padding: 15px; border-radius: 5px; margin-top: 20px; }
        .log-entry { font-family: monospace; font-size: 12px; margin: 2px 0; padding: 2px 5px; background-color: #2c3e50; color: white; border-radius: 3px; }
        .connection-status { display: inline-block; width: 10px; height: 10px; border-radius: 50%; margin-right: 5px; }
        .connected { background-color: #2ecc71; }
        .disconnected { background-color: #e74c3c; }
        .radio-group { margin: 10px 0; }
        .radio-group label { width: auto; margin-right: 15px; font-weight: normal; }
        .agv-status { background-color: #34495e; color: white; padding: 10px; border-radius: 5px; margin: 10px 0; font-weight: bold; }
        .loop-count { margin-left: 20px; display: none; }
        .loop-count input { width: 60px; }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>🚗 AGV Navigation Controller</h1>
            <button class="logout-btn" onclick="logout()">Logout</button>
        </div>
        <div class="control-section">
            <div class="section-title">Connection Status</div>
            <div id="connectionStatus">
                <span class="connection-status disconnected" id="statusIndicator"></span>
                <span id="statusText">Disconnected from ESP32</span>
            </div>
        </div>
        <div class="control-section">
            <div class="section-title">Path Configuration</div>
            <div class="input-group">
                <label for="sourceX">Source X:</label><input type="number" id="sourceX" value="1" min="1" max="10">
                <label for="sourceY">Source Y:</label><input type="number" id="sourceY" value="1" min="1" max="10">
            </div>
            <div class="input-group">
                <label for="destX">Destination X:</label><input type="number" id="destX" value="3" min="1" max="10">
                <label for="destY">Destination Y:</label><input type="number" id="destY" value="2" min="1" max="10">
            </div>
            <div class="radio-group">
                <label>Execution Mode:</label>
                <input type="radio" id="once" name="mode" value="once" checked onchange="toggleLoopCount()">
                <label for="once">Run Once</label>
                <input type="radio" id="loop" name="mode" value="loop" onchange="toggleLoopCount()">
                <label for="loop">Loop Continuously</label>
                <span class="loop-count" id="loopCountContainer">
                    <label for="loopCount">Times:</label>
                    <input type="number" id="loopCount" value="2" min="2" max="1000">
                </span>
            </div>
            <div class="button-group">
                <button class="success-btn" onclick="sendPath()">Update Path</button>
                <button class="primary-btn" onclick="sendDefault()">Load Default Path</button>
            </div>
        </div>
        <div class="control-section">
            <div class="section-title">AGV Control</div>
            <div class="button-group">
                <button class="success-btn" onclick="sendStart()">▶️ START</button>
                <button class="warning-btn" onclick="sendStop()">⏹️ STOP</button>
                <button class="danger-btn" onclick="sendAbort()">🛑 ABORT</button>
            </div>
        </div>
        <div class="control-section">
            <div class="section-title">AGV Status</div>
            <div id="agvStatusDisplay" class="agv-status">AGV: Waiting for ESP32 connection...</div>
        </div>
        <div class="status-section">
            <div class="section-title">System Logs</div>
            <div id="logDisplay" style="max-height: 200px; overflow-y: auto; margin-top: 10px;"></div>
        </div>
    </div>
    <script>
        let ws; let isConnected = false;
        function checkAuth() { const token = localStorage.getItem('token'); if (!token) window.location.href = '/'; }
        function logout() { localStorage.removeItem('token'); window.location.href = '/'; }
        function connect() {
            ws = new WebSocket('ws://' + window.location.hostname + ':81');
            ws.onopen = function() { isConnected = true; updateConnectionStatus(true, 'Connected to ESP32'); addLog('✅ Connected to ESP32'); updateAGVStatus('Connected - Ready for commands'); };
            ws.onclose = function() { isConnected = false; updateConnectionStatus(false, 'Disconnected from ESP32'); addLog('🔌 Disconnected. Reconnecting...'); updateAGVStatus('Disconnected'); setTimeout(connect, 2000); };
            ws.onerror = function() { addLog('❌ Connection error'); };
            ws.onmessage = function(event) { addLog('📥 ESP32: ' + event.data); updateAGVStatus(event.data); };
        }
        function updateConnectionStatus(connected, message) {
            const indicator = document.getElementById('statusIndicator');
            const statusText = document.getElementById('statusText');
            if (connected) { indicator.className = 'connection-status connected'; statusText.textContent = message; } 
            else { indicator.className = 'connection-status disconnected'; statusText.textContent = message; }
        }
        function updateAGVStatus(message) {
            const statusDisplay = document.getElementById('agvStatusDisplay');
            statusDisplay.textContent = 'AGV: ' + message;
            if (message.includes('START') || message.includes('Moving')) statusDisplay.style.backgroundColor = '#27ae60';
            else if (message.includes('STOP') || message.includes('Stopped')) statusDisplay.style.backgroundColor = '#f39c12';
            else if (message.includes('ABORT') || message.includes('Emergency')) statusDisplay.style.backgroundColor = '#e74c3c';
            else if (message.includes('Ready') || message.includes('Connected')) statusDisplay.style.backgroundColor = '#3498db';
            else statusDisplay.style.backgroundColor = '#34495e';
        }
        function addLog(message) {
            const logDisplay = document.getElementById('logDisplay');
            const logEntry = document.createElement('div');
            logEntry.className = 'log-entry';
            const timestamp = new Date().toLocaleTimeString();
            logEntry.textContent = `[${timestamp}] ${message}`;
            logDisplay.appendChild(logEntry);
            logDisplay.scrollTop = logDisplay.scrollHeight;
        }
        function sendCommand(command) {
            if (!isConnected || !ws || ws.readyState !== WebSocket.OPEN) { addLog('❌ Not connected to ESP32'); return; }
            ws.send(command); addLog('📤 Sent: ' + command);
        }
        function toggleLoopCount() {
            const loopRadio = document.getElementById('loop');
            const loopCountContainer = document.getElementById('loopCountContainer');
            if (loopRadio.checked) loopCountContainer.style.display = 'inline-block';
            else loopCountContainer.style.display = 'none';
        }
        function sendPath() {
            const sourceX = parseInt(document.getElementById('sourceX').value);
            const sourceY = parseInt(document.getElementById('sourceY').value);
            const destX = parseInt(document.getElementById('destX').value);
            const destY = parseInt(document.getElementById('destY').value);
            const mode = document.querySelector('input[name="mode"]:checked').value;
            const loopCount = document.getElementById('loopCount').value;
            let command = mode === 'loop' ? `PATH:${sourceX},${sourceY},${destX},${destY}:LOOP:${loopCount}` : `PATH:${sourceX},${sourceY},${destX},${destY}:ONCE`;
            sendCommand(command);
        }
        function sendDefault() { sendCommand('DEFAULT'); }
        function sendStart() { sendCommand('START'); }
        function sendStop() { sendCommand('STOP'); }
        function sendAbort() { sendCommand('ABORT'); }
        window.onload = function() { checkAuth(); addLog('🔧 AGV Control Interface Initialized'); addLog('📍 Connecting to ESP32...'); updateAGVStatus('Web interface ready - Connecting...'); toggleLoopCount(); connect(); };
    </script>
</body>
</html>
)rawliteral";

#endif // AGV_COMM_HTML_H