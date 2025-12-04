/**
 * AGVComm.cpp - AGV Communication Library Implementation
 * Core 0 dedicated communication layer
 */

#include "AGVComm.h"

// Static member initialization
AGVComm* AGVComm::instance = nullptr;

// HTML Pages (defined at the end of this file)
const char* AGVComm::loginPageHTML = nullptr;
const char* AGVComm::wifiSetupPageHTML = nullptr;
const char* AGVComm::mainPageHTML = nullptr;

// ============ Constructor & Destructor ============

AGVComm::AGVComm() {
    server = nullptr;
    webSocket = nullptr;
    dnsServer = nullptr;
    is_ap_mode = false;
    is_authenticated = false;
    is_initialized = false;
    command_queue = nullptr;
    command_mutex = nullptr;
    core0_task_handle = nullptr;
    cmd_callback = nullptr;
    conn_callback = nullptr;
    instance = this;
}

AGVComm::~AGVComm() {
    if (server) delete server;
    if (webSocket) delete webSocket;
    if (dnsServer) delete dnsServer;
    if (command_queue) vQueueDelete(command_queue);
    if (command_mutex) vSemaphoreDelete(command_mutex);
}

// ============ Initialization ============

bool AGVComm::begin(const char* apSSID, const char* apPassword, const char* mdnsName) {
    if (is_initialized) {
        Serial.println("[AGVComm] Already initialized");
        return false;
    }
    
    Serial.println("\n╔════════════════════════════════════════╗");
    Serial.println("║   AGVComm Library Initializing        ║");
    Serial.println("╚════════════════════════════════════════╝\n");
    
    // Store configuration
    ap_ssid = String(apSSID);
    ap_password = String(apPassword);
    mdns_name = String(mdnsName);
    
    // Default admin credentials
    admin_username = "admin";
    admin_password = "admin123";
    
    // Initialize communication structures
    command_queue = xQueueCreate(COMMAND_QUEUE_SIZE, sizeof(AGVCommand));
    command_mutex = xSemaphoreCreateMutex();
    
    if (!command_queue || !command_mutex) {
        Serial.println("[AGVComm] ERROR: Failed to create queue/mutex");
        return false;
    }
    
    // Create server objects
    server = new WebServer(WEB_SERVER_PORT);
    webSocket = new WebSocketsServer(WEBSOCKET_PORT);
    dnsServer = new DNSServer();
    
    // Load stored WiFi credentials
    preferences.begin("wifi", false);
    stored_ssid = preferences.getString("ssid", "");
    stored_password = preferences.getString("password", "");
    preferences.end();
    
    Serial.println("[AGVComm] Checking WiFi credentials...");
    
    // Start in appropriate mode
    if (stored_ssid.length() > 0) {
        Serial.println("[AGVComm] Found credentials, starting Station mode");
        startStationMode();
    } else {
        Serial.println("[AGVComm] No credentials, starting AP mode");
        startAPMode();
    }
    
    is_initialized = true;
    Serial.println("\n[AGVComm] ✅ Initialization complete\n");
    
    return true;
}

void AGVComm::setAdminCredentials(const char* username, const char* password) {
    admin_username = String(username);
    admin_password = String(password);
    Serial.printf("[AGVComm] Admin credentials updated: %s\n", username);
}

// ============ Callbacks ============

void AGVComm::onCommand(CommandCallback callback) {
    cmd_callback = callback;
}

void AGVComm::onConnection(ConnectionCallback callback) {
    conn_callback = callback;
}

// ============ Command Handling ============

void AGVComm::sendToClients(const char* message) {
    if (webSocket && !is_ap_mode) {
        webSocket->broadcastTXT(message);
        Serial.printf("[AGVComm] Broadcast: %s\n", message);
    }
}

void AGVComm::sendToClient(uint8_t clientNum, const char* message) {
    if (webSocket && !is_ap_mode) {
        webSocket->sendTXT(clientNum, message);
    }
}

QueueHandle_t AGVComm::getCommandQueue() {
    return command_queue;
}

// ============ Network Information ============

bool AGVComm::isInAPMode() {
    return is_ap_mode;
}

bool AGVComm::isConnected() {
    return WiFi.status() == WL_CONNECTED;
}

String AGVComm::getIPAddress() {
    if (is_ap_mode) {
        return WiFi.softAPIP().toString();
    } else {
        return WiFi.localIP().toString();
    }
}

String AGVComm::getMDNSUrl() {
    if (!is_ap_mode) {
        return "http://" + mdns_name + ".local";
    }
    return "";
}

uint8_t AGVComm::getClientCount() {
    // WebSocket library doesn't expose this directly, 
    // you'd need to track it manually in webSocketEvent
    return 0; // Placeholder
}

// ============ WiFi Management ============

bool AGVComm::hasStoredCredentials() {
    return stored_ssid.length() > 0;
}

void AGVComm::clearCredentials() {
    preferences.begin("wifi", false);
    preferences.clear();
    preferences.end();
    stored_ssid = "";
    stored_password = "";
    Serial.println("[AGVComm] Credentials cleared");
}

void AGVComm::restart() {
    Serial.println("[AGVComm] Restarting ESP32...");
    delay(1000);
    ESP.restart();
}

// ============ Private Methods - Mode Setup ============

void AGVComm::startAPMode() {
    Serial.println("\n[AGVComm] Starting Access Point Mode");
    
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ap_ssid.c_str(), ap_password.c_str());
    
    IPAddress IP = WiFi.softAPIP();
    Serial.printf("[AGVComm] AP IP: %s\n", IP.toString().c_str());
    Serial.printf("[AGVComm] SSID: %s\n", ap_ssid.c_str());
    Serial.printf("[AGVComm] Password: %s\n", ap_password.c_str());
    
    delay(100);
    
    // Start DNS for captive portal
    dnsServer->start(DNS_PORT, "*", WiFi.softAPIP());
    is_ap_mode = true;
    
    setupWebServer();
    server->begin();
    
    Serial.println("[AGVComm] ✅ AP Mode active with captive portal");
}

void AGVComm::startStationMode() {
    Serial.println("\n[AGVComm] Starting Station Mode");
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(stored_ssid.c_str(), stored_password.c_str());
    
    Serial.printf("[AGVComm] Connecting to: %s\n", stored_ssid.c_str());
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    Serial.println();
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("[AGVComm] ✅ WiFi Connected!");
        Serial.printf("[AGVComm] IP: %s\n", WiFi.localIP().toString().c_str());
        
        // Start mDNS
        if (MDNS.begin(mdns_name.c_str())) {
            Serial.printf("[AGVComm] ✅ mDNS: http://%s.local\n", mdns_name.c_str());
            MDNS.addService("http", "tcp", 80);
        }
        
        is_ap_mode = false;
        
        setupWebServer();
        setupWebSocket();
        
        server->begin();
        webSocket->begin();
        webSocket->onEvent(webSocketEventStatic);
        
        Serial.println("[AGVComm] ✅ Station Mode active");
        
        // Notify connection callback
        if (conn_callback) {
            conn_callback(true);
        }
        
    } else {
        Serial.println("[AGVComm] ❌ WiFi connection failed");
        Serial.println("[AGVComm] Falling back to AP mode");
        startAPMode();
    }
}

void AGVComm::setupWebServer() {
    // Setup routes based on mode
    if (is_ap_mode) {
        // AP Mode routes
        server->on("/", HTTP_GET, [this]() { handleCaptivePortal(); });
        server->on("/setup", HTTP_GET, [this]() { handleWiFiSetup(); });
        server->on("/scan", HTTP_GET, [this]() { handleScan(); });
        server->on("/savewifi", HTTP_POST, [this]() { handleSaveWiFi(); });
        
        // Captive portal detection URLs
        server->on("/generate_204", HTTP_GET, [this]() { handleCaptivePortal(); });
        server->on("/fwlink", HTTP_GET, [this]() { handleCaptivePortal(); });
        server->on("/redirect", HTTP_GET, [this]() { handleCaptivePortal(); });
        server->onNotFound([this]() { handleCaptivePortal(); });
        
    } else {
        // Station Mode routes
        server->on("/", HTTP_GET, [this]() { handleRoot(); });
        server->on("/login", HTTP_POST, [this]() { handleLogin(); });
        server->on("/dashboard", HTTP_GET, [this]() { handleDashboard(); });
        server->onNotFound([this]() { handleNotFound(); });
    }
}

void AGVComm::setupWebSocket() {
    // WebSocket is only used in Station mode
    if (!is_ap_mode && webSocket) {
        webSocket->begin();
        webSocket->onEvent(webSocketEventStatic);
    }
}

String AGVComm::generateSessionToken() {
    String token = "";
    for(int i = 0; i < 32; i++) {
        token += String(random(0, 16), HEX);
    }
    return token;
}

// ============ Web Handlers ============

void AGVComm::handleRoot() {
    // Serve login page HTML
    server->send(200, "text/html", loginPageHTML);
}

void AGVComm::handleLogin() {
    if (server->method() == HTTP_POST) {
        String body = server->arg("plain");
        
        // Simple JSON parsing
        int userStart = body.indexOf("\"username\":\"") + 12;
        int userEnd = body.indexOf("\"", userStart);
        String username = body.substring(userStart, userEnd);
        
        int passStart = body.indexOf("\"password\":\"") + 12;
        int passEnd = body.indexOf("\"", passStart);
        String password = body.substring(passStart, passEnd);
        
        if (username == admin_username && password == admin_password) {
            session_token = generateSessionToken();
            String response = "{\"success\":true,\"token\":\"" + session_token + "\"}";
            server->send(200, "application/json", response);
            Serial.println("[AGVComm] ✅ Login successful");
        } else {
            server->send(200, "application/json", "{\"success\":false}");
            Serial.println("[AGVComm] ❌ Login failed");
        }
    }
}

void AGVComm::handleDashboard() {
    server->send(200, "text/html", mainPageHTML);
}

void AGVComm::handleWiFiSetup() {
    server->send(200, "text/html", wifiSetupPageHTML);
}

void AGVComm::handleScan() {
    Serial.println("[AGVComm] Scanning WiFi networks...");
    int n = WiFi.scanNetworks();
    String json = "[";
    
    for (int i = 0; i < n; i++) {
        if (i > 0) json += ",";
        json += "{";
        json += "\"ssid\":\"" + WiFi.SSID(i) + "\",";
        json += "\"rssi\":" + String(WiFi.RSSI(i)) + ",";
        json += "\"secured\":" + String(WiFi.encryptionType(i) != WIFI_AUTH_OPEN ? "true" : "false");
        json += "}";
    }
    
    json += "]";
    server->send(200, "application/json", json);
    Serial.printf("[AGVComm] Found %d networks\n", n);
}

void AGVComm::handleSaveWiFi() {
    if (server->method() == HTTP_POST) {
        String body = server->arg("plain");
        
        int ssidStart = body.indexOf("\"ssid\":\"") + 8;
        int ssidEnd = body.indexOf("\"", ssidStart);
        String ssid = body.substring(ssidStart, ssidEnd);
        
        int passStart = body.indexOf("\"password\":\"") + 12;
        int passEnd = body.indexOf("\"", passStart);
        String password = body.substring(passStart, passEnd);
        
        Serial.printf("[AGVComm] Saving WiFi: %s\n", ssid.c_str());
        
        preferences.begin("wifi", false);
        preferences.putString("ssid", ssid);
        preferences.putString("password", password);
        preferences.end();
        
        server->send(200, "application/json", "{\"success\":true}");
        
        Serial.println("[AGVComm] ✅ Credentials saved, restarting...");
        delay(1000);
        ESP.restart();
    }
}

void AGVComm::handleNotFound() {
    server->send(404, "text/plain", "Not Found");
}

void AGVComm::handleCaptivePortal() {
    String header = "HTTP/1.1 302 Found\r\n";
    header += "Location: http://192.168.4.1/setup\r\n";
    header += "Cache-Control: no-cache\r\n\r\n";
    server->client().write(header.c_str(), header.length());
    server->client().stop();
}

// ============ WebSocket Handler ============

void AGVComm::webSocketEventStatic(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
    if (instance) {
        instance->webSocketEvent(num, type, payload, length);
    }
}

void AGVComm::webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
    switch(type) {
        case WStype_DISCONNECTED:
            Serial.printf("[AGVComm] WebSocket client #%u disconnected\n", num);
            if (conn_callback) conn_callback(false);
            break;
            
        case WStype_CONNECTED:
            {
                IPAddress ip = webSocket->remoteIP(num);
                Serial.printf("[AGVComm] WebSocket client #%u connected from %s\n", 
                            num, ip.toString().c_str());
                webSocket->sendTXT(num, "ESP32 Connected - Ready");
                if (conn_callback) conn_callback(true);
            }
            break;
            
        case WStype_TEXT:
            {
                // Create command structure
                AGVCommand cmd;
                snprintf(cmd.command, sizeof(cmd.command), "%.*s", length, (char*)payload);
                cmd.source = SOURCE_WEBSOCKET;
                cmd.timestamp = millis();
                
                Serial.printf("[AGVComm] WebSocket RX: %s\n", cmd.command);
                
                // Send to command queue
                if (xQueueSend(command_queue, &cmd, pdMS_TO_TICKS(100)) != pdPASS) {
                    Serial.println("[AGVComm] WARNING: Command queue full");
                }
                
                // Call callback if registered
                if (cmd_callback) {
                    cmd_callback(cmd.command, SOURCE_WEBSOCKET);
                }
                
                // Echo acknowledgment
                String ack = "ACK: " + String(cmd.command);
                webSocket->sendTXT(num, ack);
            }
            break;
    }
}

// ============ Serial Processing ============

void AGVComm::processSerial() {
    static char serialBuffer[MAX_COMMAND_LENGTH];
    static int bufferIndex = 0;
    
    while (Serial.available() > 0) {
        char c = Serial.read();
        
        if (c == '\n' || c == '\r') {
            if (bufferIndex > 0) {
                serialBuffer[bufferIndex] = '\0';
                
                // Create command structure
                AGVCommand cmd;
                strncpy(cmd.command, serialBuffer, sizeof(cmd.command) - 1);
                cmd.command[sizeof(cmd.command) - 1] = '\0';
                cmd.source = SOURCE_SERIAL;
                cmd.timestamp = millis();
                
                Serial.printf("[AGVComm] Serial RX: %s\n", cmd.command);
                
                // Send to command queue
                if (xQueueSend(command_queue, &cmd, pdMS_TO_TICKS(100)) != pdPASS) {
                    Serial.println("[AGVComm] WARNING: Command queue full");
                }
                
                // Call callback if registered
                if (cmd_callback) {
                    cmd_callback(cmd.command, SOURCE_SERIAL);
                }
                
                // Broadcast to WebSocket clients
                sendToClients(cmd.command);
                
                // Reset buffer
                bufferIndex = 0;
            }
        } else if (bufferIndex < sizeof(serialBuffer) - 1) {
            serialBuffer[bufferIndex++] = c;
        } else {
            // Buffer overflow, reset
            bufferIndex = 0;
            Serial.println("[AGVComm] WARNING: Serial buffer overflow");
        }
    }
}

// ============ Main Loop ============

void AGVComm::loop() {
    // Handle DNS (for captive portal in AP mode)
    if (is_ap_mode && dnsServer) {
        dnsServer->processNextRequest();
    }
    
    // Handle web server
    if (server) {
        server->handleClient();
    }
    
    // Handle WebSocket (only in Station mode)
    if (!is_ap_mode && webSocket) {
        webSocket->loop();
    }
    
    // Process serial commands (only in Station mode)
    if (!is_ap_mode) {
        processSerial();
    }
}

// ============ Core 0 Task ============

void AGVComm::core0TaskFunction(void* parameter) {
    AGVComm* comm = (AGVComm*)parameter;
    Serial.println("[AGVComm] Core 0 task started");
    
    while (true) {
        comm->loop();
        vTaskDelay(pdMS_TO_TICKS(1)); // Yield to other tasks
    }
}

TaskHandle_t AGVComm::startCore0Task(uint8_t priority) {
    if (core0_task_handle != nullptr) {
        Serial.println("[AGVComm] Core 0 task already running");
        return core0_task_handle;
    }
    
    xTaskCreatePinnedToCore(
        core0TaskFunction,
        "AGVCommCore0",
        10000,
        this,
        priority,
        &core0_task_handle,
        0  // Core 0
    );
    
    Serial.println("[AGVComm] ✅ Core 0 task created");
    return core0_task_handle;
}

// ============ HTML Pages ============
// Note: These should be defined in a separate file or kept in PROGMEM
// For now, using the original HTML from your code

const char* AGVComm::loginPageHTML = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>AGV Controller Login</title>
    <style>
        body { font-family: Arial, sans-serif; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); display: flex; justify-content: center; align-items: center; height: 100vh; margin: 0; }
        .login-container { background: white; padding: 40px; border-radius: 10px; box-shadow: 0 10px 25px rgba(0,0,0,0.2); width: 100%; max-width: 400px; }
        h1 { text-align: center; color: #333; margin-bottom: 30px; }
        .form-group { margin-bottom: 20px; }
        label { display: block; margin-bottom: 5px; color: #555; font-weight: bold; }
        input { width: 100%; padding: 12px; border: 1px solid #ddd; border-radius: 5px; box-sizing: border-box; font-size: 16px; }
        button { width: 100%; padding: 12px; background: #667eea; color: white; border: none; border-radius: 5px; font-size: 16px; font-weight: bold; cursor: pointer; transition: background 0.3s; }
        button:hover { background: #5568d3; }
        .error { color: #e74c3c; text-align: center; margin-top: 10px; display: none; }
        .robot-icon { text-align: center; font-size: 48px; margin-bottom: 20px; }
    </style>
</head>
<body>
    <div class="login-container">
        <div class="robot-icon">🚗</div>
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
            const response = await fetch('/login', { method: 'POST', headers: {'Content-Type': 'application/json'}, body: JSON.stringify({username, password}) });
            const result = await response.json();
            if (result.success) { localStorage.setItem('token', result.token); window.location.href = '/dashboard'; } 
            else { document.getElementById('error').style.display = 'block'; }
        });
    </script>
</body>
</html>
)rawliteral";

// Continue in next message due to length...