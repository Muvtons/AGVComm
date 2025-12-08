#ifndef AGVCORENETWORK_RESOURCES_H
#define AGVCORENETWORK_RESOURCES_H

#include <Arduino.h>

// Login page HTML - MINIFIED for memory efficiency
const char loginPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>AGV Controller Login</title>
    <style>
        body{font-family:Arial,sans-serif;background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);display:flex;justify-content:center;align-items:center;height:100vh;margin:0}
        .login-container{background:white;padding:40px;border-radius:10px;box-shadow:0 10px 25px rgba(0,0,0,0.2);width:100%;max-width:400px}
        h1{text-align:center;color:#333;margin-bottom:30px}
        .form-group{margin-bottom:20px}
        label{display:block;margin-bottom:5px;color:#555;font-weight:bold}
        input{width:100%;padding:12px;border:1px solid #ddd;border-radius:5px;box-sizing:border-box;font-size:16px}
        button{width:100%;padding:12px;background:#667eea;color:white;border:none;border-radius:5px;font-size:16px;font-weight:bold;cursor:pointer;transition:background 0.3s}
        button:hover{background:#5568d3}
        .error{color:#e74c3c;text-align:center;margin-top:10px;display:none}
        .robot-icon{text-align:center;font-size:48px;margin-bottom:20px}
    </style>
</head>
<body>
    <div class="login-container">
        <div class="robot-icon">🤖</div>
        <h1>AGV Controller</h1>
        <form id="loginForm">
            <div class="form-group">
                <label for="username">Username</label>
                <input type="text" id="username" required>
            </div>
            <div class="form-group">
                <label for="password">Password</label>
                <input type="password" id="password" required>
            </div>
            <button type="submit">Login</button>
            <div class="error" id="error">Invalid credentials!</div>
        </form>
    </div>
    <script>
        document.getElementById('loginForm').addEventListener('submit', async function(e) {
            e.preventDefault();
            const username = document.getElementById('username').value;
            const password = document.getElementById('password').value;
            
            try {
                const response = await fetch('/login', {
                    method: 'POST',
                    headers: {'Content-Type': 'application/json'},
                    body: JSON.stringify({username, password})
                });
                
                const result = await response.json();
                if (result.success) {
                    localStorage.setItem('token', result.token);
                    window.location.href = '/dashboard';
                } else {
                    document.getElementById('error').style.display = 'block';
                }
            } catch (error) {
                console.error('Login failed:', error);
                document.getElementById('error').textContent = 'Connection error!';
                document.getElementById('error').style.display = 'block';
            }
        });
    </script>
</body>
</html>
)rawliteral";

// WiFi Setup page HTML - MINIFIED
const char wifiSetupPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>AGV WiFi Setup</title>
    <style>
        body{font-family:Arial,sans-serif;background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);display:flex;justify-content:center;align-items:center;min-height:100vh;margin:0;padding:20px}
        .setup-container{background:white;padding:40px;border-radius:10px;box-shadow:0 10px 25px rgba(0,0,0,0.2);width:100%;max-width:500px}
        h1{text-align:center;color:#333;margin-bottom:30px}
        .form-group{margin-bottom:20px}
        label{display:block;margin-bottom:5px;color:#555;font-weight:bold}
        input,select{width:100%;padding:12px;border:1px solid #ddd;border-radius:5px;box-sizing:border-box;font-size:16px}
        button{width:100%;padding:12px;margin-top:10px;border:none;border-radius:5px;font-size:16px;font-weight:bold;cursor:pointer;transition:background 0.3s}
        .scan-btn{background:#3498db;color:white}
        .scan-btn:hover{background:#2980b9}
        .save-btn{background:#2ecc71;color:white}
        .save-btn:hover{background:#27ae60}
        .message{text-align:center;padding:10px;margin-top:10px;border-radius:5px;display:none}
        .success{background:#d4edda;color:#155724}
        .error{background:#f8d7da;color:#721c24}
        .loading{text-align:center;margin:10px 0;display:none}
        .back-btn{background:#95a5a6;color:white;width:auto;padding:8px 16px;margin-top:20px}
    </style>
</head>
<body>
    <div class="setup-container">
        <h1>📡 AGV WiFi Setup</h1>
        <form id="wifiForm">
            <div class="form-group">
                <label for="ssid">WiFi Network</label>
                <select id="ssid" required>
                    <option value="">-- Select or scan below --</option>
                </select>
            </div>
            <button type="button" class="scan-btn" onclick="scanNetworks()">🔍 Scan Networks</button>
            <div class="loading" id="loading">Scanning networks...</div>
            
            <div class="form-group">
                <label for="password">WiFi Password</label>
                <input type="password" id="password" required>
            </div>
            
            <button type="submit" class="save-btn">💾 Save & Connect</button>
            <div class="message" id="message"></div>
        </form>
        <button class="back-btn" onclick="location.href='/'">🏠 Back to Main</button>
    </div>
    <script>
        let scanAttempts = 0;
        
        async function scanNetworks() {
            if (scanAttempts >= 3) {
                alert('Maximum scan attempts reached. Please try again later.');
                return;
            }
            
            const loading = document.getElementById('loading');
            const message = document.getElementById('message');
            const ssidSelect = document.getElementById('ssid');
            
            loading.style.display = 'block';
            message.style.display = 'none';
            
            try {
                const response = await fetch('/scan', {timeout: 10000});
                const networks = await response.json();
                
                ssidSelect.innerHTML = '<option value="">-- Select WiFi Network --</option>';
                networks.forEach(network => {
                    const option = document.createElement('option');
                    option.value = network.ssid;
                    option.textContent = `${network.ssid} (${network.rssi} dBm) ${network.secured ? '🔒' : ''}`;
                    ssidSelect.appendChild(option);
                });
                
                if (networks.length === 0) {
                    message.className = 'message error';
                    message.textContent = 'No networks found. Try again.';
                    message.style.display = 'block';
                }
                
                scanAttempts++;
            } catch (error) {
                console.error('Scan failed:', error);
                message.className = 'message error';
                message.textContent = 'Scan failed: ' + (error.message || 'Network error');
                message.style.display = 'block';
            } finally {
                loading.style.display = 'none';
            }
        }
        
        document.getElementById('wifiForm').addEventListener('submit', async function(e) {
            e.preventDefault();
            const ssid = document.getElementById('ssid').value;
            const password = document.getElementById('password').value;
            const message = document.getElementById('message');
            
            if (!ssid) {
                message.className = 'message error';
                message.textContent = 'Please select a WiFi network';
                message.style.display = 'block';
                return;
            }
            
            message.style.display = 'block';
            message.className = 'message';
            message.textContent = 'Saving configuration...';
            
            try {
                const response = await fetch('/savewifi', {
                    method: 'POST',
                    headers: {'Content-Type': 'application/json'},
                    body: JSON.stringify({ssid, password})
                });
                
                const result = await response.json();
                if (result.success) {
                    message.className = 'message success';
                    message.textContent = '✅ Configuration saved! Restarting AGV...';
                    setTimeout(() => {
                        alert('AGV is restarting. Please wait 30 seconds, then reconnect to the new WiFi network.');
                        location.href = '/';
                    }, 3000);
                } else {
                    message.className = 'message error';
                    message.textContent = '❌ Failed to save configuration';
                }
            } catch (error) {
                console.error('Save failed:', error);
                message.className = 'message error';
                message.textContent = '❌ Save failed: ' + (error.message || 'Connection error');
            }
        });
        
        // Auto-scan on page load
        window.addEventListener('load', scanNetworks);
    </script>
</body>
</html>
)rawliteral";

// Main Dashboard page HTML - SAFETY ENHANCED
const char mainPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>AGV Control Dashboard</title>
    <style>
        *{box-sizing:border-box;margin:0;padding:0}
        body{font-family:'Segoe UI',Arial,sans-serif;background:#f0f2f5;color:#333;line-height:1.6}
        .container{max-width:1200px;margin:0 auto;padding:20px}
        header{background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);color:white;text-align:center;padding:20px;border-radius:10px;margin-bottom:20px;box-shadow:0 4px 6px rgba(0,0,0,0.1)}
        h1{font-size:2.2em;margin-bottom:10px;display:flex;align-items:center;justify-content:center;gap:15px}
        .robot-icon{font-size:2.5em}
        
        .status-bar{display:flex;justify-content:space-between;align-items:center;background:#2c3e50;color:white;padding:15px;border-radius:8px;margin-bottom:25px;box-shadow:0 2px 5px rgba(0,0,0,0.2)}
        .connection-status{display:flex;align-items:center;gap:8px;font-weight:bold}
        .status-indicator{width:12px;height:12px;border-radius:50%;background:#e74c3c}
        .status-indicator.connected{background:#2ecc71}
        .emergency-status{background:#e74c3c;padding:5px 15px;border-radius:20px;font-weight:bold;animation:pulse 1.5s infinite}
        
        .grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(300px,1fr));gap:25px;margin-bottom:25px}
        .card{background:white;border-radius:10px;box-shadow:0 4px 6px rgba(0,0,0,0.1);padding:25px;transition:transform 0.3s ease}
        .card:hover{transform:translateY(-5px)}
        .card-header{display:flex;justify-content:space-between;align-items:center;margin-bottom:20px;border-bottom:2px solid #667eea;padding-bottom:10px}
        .card-title{font-size:1.4em;font-weight:bold;color:#2c3e50;display:flex;align-items:center;gap:10px}
        .card-icon{font-size:1.8em}
        
        .controls{display:grid;grid-template-columns:repeat(2,1fr);gap:15px;margin-top:15px}
        .control-group{margin-bottom:15px}
        label{display:block;margin-bottom:5px;font-weight:600;color:#2c3e50}
        input,select{width:100%;padding:10px;border:2px solid #ddd;border-radius:5px;font-size:16px;transition:border 0.3s}
        input:focus,select:focus{border-color:#667eea;outline:none}
        
        .btn-group{display:grid;grid-template-columns:repeat(3,1fr);gap:12px;margin-top:20px}
        .btn{padding:14px 10px;border:none;border-radius:8px;font-weight:bold;font-size:1.1em;cursor:pointer;transition:all 0.3s ease;display:flex;flex-direction:column;align-items:center;justify-content:center;gap:5px}
        .btn i{font-size:1.5em}
        
        .btn-primary{background:#3498db;color:white}
        .btn-primary:hover{background:#2980b9}
        .btn-success{background:#2ecc71;color:white}
        .btn-success:hover{background:#27ae60}
        .btn-warning{background:#f39c12;color:white}
        .btn-warning:hover{background:#d35400}
        .btn-danger{background:#e74c3c;color:white}
        .btn-danger:hover{background:#c0392b}
        
        .logs{background:white;border-radius:10px;box-shadow:0 4px 6px rgba(0,0,0,0.1);padding:25px;margin-bottom:25px}
        .logs-header{display:flex;justify-content:space-between;align-items:center;margin-bottom:15px;padding-bottom:10px;border-bottom:2px solid #667eea}
        .logs-title{font-size:1.4em;font-weight:bold;color:#2c3e50}
        .clear-logs{background:#95a5a6;color:white;border:none;padding:5px 15px;border-radius:5px;cursor:pointer;transition:background 0.3s}
        .clear-logs:hover{background:#7f8c8d}
        
        .log-container{max-height:300px;overflow-y:auto;background:#2c3e50;color:#ecf0f1;font-family:monospace;padding:15px;border-radius:8px;font-size:0.95em}
        .log-entry{margin-bottom:5px;padding:5px 0;border-bottom:1px solid #34495e}
        .log-timestamp{color:#3498db;font-weight:bold;margin-right:10px}
        .log-serial{color:#2ecc71}
        .log-web{color:#3498db}
        .log-emergency{color:#e74c3c;font-weight:bold}
        
        footer{text-align:center;margin-top:30px;color:#7f8c8d;font-size:0.9em}
        
        @keyframes pulse{
            0%{opacity:1;box-shadow:0 0 0 0 rgba(231,76,60,0.7)}
            70%{opacity:0.7;box-shadow:0 0 0 10px rgba(231,76,60,0)}
            100%{opacity:1;box-shadow:0 0 0 0 rgba(231,76,60,0)}
        }
        
        @media (max-width:768px){
            .btn-group{grid-template-columns:1fr}
            .controls{grid-template-columns:1fr}
            .grid{grid-template-columns:1fr}
        }
    </style>
</head>
<body>
    <div class="container">
        <header>
            <h1><span class="robot-icon">🤖</span> AGV Control Dashboard</h1>
            <p>Real-time monitoring and control interface</p>
        </header>
        
        <div class="status-bar">
            <div class="connection-status">
                <span class="status-indicator" id="connectionIndicator"></span>
                <span id="connectionText">Connecting...</span>
            </div>
            <div class="emergency-status" id="emergencyStatus" style="display:none;">
                ⚠️ EMERGENCY STOP ACTIVE
            </div>
            <button onclick="logout()" class="btn-danger" style="padding:8px 15px;margin:0;font-size:0.9em;">Logout</button>
        </div>
        
        <div class="grid">
            <div class="card">
                <div class="card-header">
                    <div class="card-title"><span class="card-icon">🔄</span> Movement Controls</div>
                </div>
                <div class="btn-group">
                    <button class="btn btn-success" onclick="sendCommand('move forward')">
                        <i>↑</i>
                        <span>Forward</span>
                    </button>
                    <button class="btn btn-warning" onclick="sendCommand('turn_left 90')">
                        <i>←</i>
                        <span>Left 90°</span>
                    </button>
                    <button class="btn btn-warning" onclick="sendCommand('turn_right 90')">
                        <i>→</i>
                        <span>Right 90°</span>
                    </button>
                    <button class="btn btn-primary" onclick="sendCommand('turnaround')">
                        <i>🔄</i>
                        <span>Turn Around</span>
                    </button>
                </div>
            </div>
            
            <div class="card">
                <div class="card-header">
                    <div class="card-title"><span class="card-icon">⚠️</span> Emergency Controls</div>
                </div>
                <div class="btn-group">
                    <button class="btn btn-danger" onclick="confirmEmergency('STOP')" style="grid-column: span 2;">
                        <i>🛑</i>
                        <span>EMERGENCY STOP</span>
                    </button>
                    <button class="btn btn-primary" onclick="confirmEmergency('CLEAR_EMERGENCY')" style="grid-column: span 2;">
                        <i>✅</i>
                        <span>CLEAR EMERGENCY</span>
                    </button>
                </div>
                <div style="margin-top:15px;padding:10px;background:#fff8e1;border-radius:8px;font-size:0.9em;">
                    <strong>⚠️ Safety Notice:</strong> Emergency Stop will immediately halt all movement and require manual reset.
                </div>
            </div>
        </div>
        
        <div class="logs">
            <div class="logs-header">
                <div class="logs-title">System Logs</div>
                <button class="clear-logs" onclick="clearLogs()">Clear Logs</button>
            </div>
            <div class="log-container" id="logContainer">
                <div class="log-entry"><span class="log-timestamp">[00:00:00]</span> <span class="log-web">System initialized - Ready for commands</span></div>
            </div>
        </div>
        
        <footer>
            <p>AGV Control System v2.0 | Serial commands take priority over web interface</p>
            <p style="margin-top:5px;color:#e74c3c;font-weight:bold;">⚠️ Safety First: Always maintain physical supervision during operation</p>
        </footer>
    </div>

    <script>
        let ws;
        let isConnected = false;
        let systemEmergency = false;
        let logEntries = [];
        
        function checkAuth() {
            const token = localStorage.getItem('token');
            if (!token) {
                window.location.href = '/';
            }
        }
        
        function logout() {
            localStorage.removeItem('token');
            window.location.href = '/';
        }
        
        function connectWebSocket() {
            const host = window.location.hostname;
            ws = new WebSocket(`ws://${host}:81`);
            
            ws.onopen = function() {
                isConnected = true;
                updateConnectionStatus(true);
                addLog('✅ Connected to AGV', 'system');
                requestSystemStatus();
            };
            
            ws.onclose = function() {
                isConnected = false;
                updateConnectionStatus(false);
                addLog('🔌 Disconnected from AGV', 'system');
                setTimeout(connectWebSocket, 3000);
            };
            
            ws.onerror = function(error) {
                console.error('WebSocket error:', error);
                addLog('❌ WebSocket error - reconnecting', 'system');
            };
            
            ws.onmessage = function(event) {
                const message = event.data.trim();
                processMessage(message);
            };
        }
        
        function updateConnectionStatus(connected) {
            const indicator = document.getElementById('connectionIndicator');
            const text = document.getElementById('connectionText');
            
            if (connected) {
                indicator.className = 'status-indicator connected';
                text.textContent = 'Connected to AGV';
                // Request status update on reconnection
                setTimeout(requestSystemStatus, 1000);
            } else {
                indicator.className = 'status-indicator';
                text.textContent = 'Disconnected - Reconnecting...';
            }
        }
        
        function processMessage(message) {
            addLog(message, 'web');
            
            // Check for emergency status
            if (message.includes('EMERGENCY') || message.includes('STOP ACTIVATED')) {
                systemEmergency = true;
                updateEmergencyStatus(true);
            } else if (message.includes('SYSTEM_NORMAL') || message.includes('Emergency cleared')) {
                systemEmergency = false;
                updateEmergencyStatus(false);
            }
            
            // Update connection status based on heartbeat
            if (message.includes('heartbeat')) {
                updateConnectionStatus(true);
            }
        }
        
        function updateEmergencyStatus(active) {
            const emergencyStatus = document.getElementById('emergencyStatus');
            if (active) {
                emergencyStatus.style.display = 'block';
                document.body.style.backgroundColor = '#fff0f0';
            } else {
                emergencyStatus.style.display = 'none';
                document.body.style.backgroundColor = '#f0f2f5';
            }
        }
        
        function addLog(message, source = 'system') {
            const now = new Date();
            const timeStr = now.toLocaleTimeString();
            const timestamp = `[${timeStr}]`;
            
            const logContainer = document.getElementById('logContainer');
            const logEntry = document.createElement('div');
            logEntry.className = 'log-entry';
            
            let sourceClass = 'log-system';
            if (source === 'serial') sourceClass = 'log-serial';
            if (source === 'web') sourceClass = 'log-web';
            if (message.toLowerCase().includes('emergency')) sourceClass = 'log-emergency';
            
            logEntry.innerHTML = `<span class="log-timestamp">${timestamp}</span> <span class="${sourceClass}">${message}</span>`;
            logContainer.appendChild(logEntry);
            logContainer.scrollTop = logContainer.scrollHeight;
            
            // Store in array for clearing
            logEntries.push({
                timestamp: timeStr,
                message: message,
                source: source
            });
            
            // Limit log entries to 100
            if (logEntries.length > 100) {
                logEntries.shift();
                logContainer.removeChild(logContainer.firstChild);
            }
        }
        
        function clearLogs() {
            document.getElementById('logContainer').innerHTML = '';
            logEntries = [];
            addLog('Logs cleared by user', 'system');
        }
        
        function sendCommand(command) {
            if (systemEmergency && !command.includes('CLEAR_EMERGENCY')) {
                addLog('❌ Command blocked: System emergency active', 'web');
                alert('System is in emergency state! Clear emergency first.');
                return;
            }
            
            if (!isConnected || !ws || ws.readyState !== WebSocket.OPEN) {
                addLog('❌ Not connected to AGV - command queued', 'web');
                alert('Not connected to AGV. Command will be sent when connection is restored.');
                return;
            }
            
            ws.send(command);
            addLog(`📤 Sent: ${command}`, 'web');
        }
        
        function confirmEmergency(command) {
            if (command === 'STOP') {
                if (!confirm('⚠️ EMERGENCY STOP: This will immediately halt all movement and require manual reset. Are you sure?')) {
                    return;
                }
            } else if (command === 'CLEAR_EMERGENCY') {
                if (!confirm('⚠️ Clear Emergency: This will restore normal operation. Ensure it is safe to proceed. Are you sure?')) {
                    return;
                }
            }
            
            sendCommand(command);
        }
        
        function requestSystemStatus() {
            if (isConnected && ws && ws.readyState === WebSocket.OPEN) {
                ws.send('STATUS_REQUEST');
            }
        }
        
        // Initialize
        window.onload = function() {
            checkAuth();
            connectWebSocket();
            setInterval(requestSystemStatus, 5000); // Request status every 5 seconds
            
            // Add initial log
            addLog('Intialized AGV Control Dashboard', 'system');
            addLog('Awaiting AGV connection...', 'system');
        };
    </script>
</body>
</html>
)rawliteral";

#endif
