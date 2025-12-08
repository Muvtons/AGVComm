#include "AGVCoreNetwork.h"
#include "AGVCoreNetwork_Resources.h"
#include <Arduino.h>

using namespace AGVCoreNetworkLib;

AGVCoreNetwork agvNetwork; // Global instance

// WebSocket event wrapper
void webSocketEventHandler(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  agvNetwork.webSocketEvent(num, type, payload, length);
}

void AGVCoreNetwork::begin(const char* deviceName, const char* adminUser, const char* adminPass) {
  Serial.println("\n[AGVNET] Initializing AGV Core Network System...");
  
  // Initialize mutex for thread safety
  mutex = xSemaphoreCreateMutex();
  if (!mutex) {
    Serial.println("[ERROR] Failed to create network mutex!");
    return;
  }
  
  // Store configuration
  this->mdnsName = deviceName;
  this->admin_username = adminUser;
  this->admin_password = adminPass;
  
  // Initialize preferences
  preferences.begin("agvnet", false);
  stored_ssid = preferences.getString("ssid", "");
  stored_password = preferences.getString("password", "");
  preferences.end();
  
  // Setup WiFi based on stored credentials
  setupWiFi();
  
  // Start Core 0 task (handles all communication)
  if (xTaskCreatePinnedToCore(
    [](void* param) {
      AGVCoreNetwork* net = (AGVCoreNetwork*)param;
      net->core0Task(NULL);
    },
    "AGVNetCore0",
    10240,  // Reduced stack size
    this,
    configMAX_PRIORITIES - 2,  // Slightly lower priority than motion
    &core0TaskHandle,
    0       // Core 0
  ) != pdPASS) {
    Serial.println("[ERROR] Failed to create Core 0 task!");
  }
  
  Serial.println("[AGVNET] ✅ Network System started on Core 0");
}

void AGVCoreNetwork::setupWiFi() {
  if (stored_ssid.length() > 0) {
    Serial.println("[AGVNET] Found saved WiFi credentials, attempting connection...");
    startStationMode();
  } else {
    Serial.println("[AGVNET] No saved credentials, starting AP mode...");
    startAPMode();
  }
}

void AGVCoreNetwork::startAPMode() {
  Serial.println("\n[AGVNET] 📡 Starting Access Point Mode");
  
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid, ap_password);
  
  IPAddress IP = WiFi.softAPIP();
  Serial.printf("[AGVNET] AP IP: %d.%d.%d.%d\n", IP[0], IP[1], IP[2], IP[3]);
  Serial.println("[AGVNET] Connect to '" + String(ap_ssid) + "' network");
  Serial.println("[AGVNET] Open http://192.168.4.1 for setup");
  
  delay(100);
  
  // Create DNS server for captive portal
  dnsServer = new DNSServer();
  dnsServer->start(53, "*", WiFi.softAPIP());
  
  isAPMode = true;
  
  // Setup web server
  server = new WebServer(80);
  
  // Setup routes for AP mode
  server->on("/", HTTP_GET, [this](){ this->handleRoot(); });
  server->on("/setup", HTTP_GET, [this](){ this->handleWiFiSetup(); });
  server->on("/scan", HTTP_GET, [this](){ this->handleScan(); });
  server->on("/savewifi", HTTP_POST, [this](){ this->handleSaveWiFi(); });
  
  // Captive portal redirects
  server->on("/generate_204", HTTP_GET, [this](){ this->handleRoot(); });
  server->on("/fwlink", HTTP_GET, [this](){ this->handleRoot(); });
  server->on("/hotspot-detect.html", HTTP_GET, [this](){ this->handleRoot(); });
  
  server->onNotFound([this](){ this->handleNotFound(); });
  
  server->begin();
  Serial.println("[AGVNET] ✅ AP Mode Web Server Started");
}

void AGVCoreNetwork::startStationMode() {
  Serial.println("\n[AGVNET] 🌐 Starting Station Mode");
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(stored_ssid.c_str(), stored_password.c_str());
  
  Serial.printf("[AGVNET] Connecting to: %s\n", stored_ssid.c_str());
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[AGVNET] ✅ WiFi Connected!");
    Serial.printf("[AGVNET] IP Address: %s\n", WiFi.localIP().toString().c_str());
    
    // Start mDNS
    if (MDNS.begin(mdnsName)) {
      MDNS.addService("http", "tcp", 80);
      Serial.printf("[AGVNET] ✅ mDNS started: http://%s.local\n", mdnsName);
    }
    
    isAPMode = false;
    
    // Setup web server and WebSocket
    server = new WebServer(80);
    webSocket = new WebSocketsServer(81);
    webSocket->begin();
    webSocket->onEvent(webSocketEventHandler);
    
    setupRoutes();
    server->begin();
    
    Serial.println("[AGVNET] ✅ Station Mode Web Server Started");
    Serial.println("[AGVNET] ✅ WebSocket Server Started (Port 81)");
    
  } else {
    Serial.println("\n[AGVNET] ❌ WiFi connection failed");
    Serial.println("[AGVNET] Falling back to AP mode...");
    startAPMode();
  }
}

void AGVCoreNetwork::setupRoutes() {
  if (!server || isAPMode) return;
  
  // Protected routes (require authentication)
  server->on("/", HTTP_GET, [this](){ 
    if (validateToken()) this->handleDashboard(); 
  });
  
  server->on("/login", HTTP_POST, [this](){ this->handleLogin(); });
  server->on("/command", HTTP_POST, [this](){ 
    if (validateToken()) this->handleCommand(); 
  });
  
  // Public routes
  server->on("/status", HTTP_GET, [this](){ 
    this->server->send(200, "application/json", 
      "{\"emergency\":" + String(systemEmergency) + 
      ",\"connected\":" + String(WiFi.status() == WL_CONNECTED) + "}");
  });
  
  server->onNotFound([this](){ this->handleNotFound(); });
}

void AGVCoreNetwork::core0Task(void *parameter) {
  Serial.println("[CORE0] AGV Network task started on Core 0");
  
  while(1) {
    if (isAPMode && dnsServer) {
      dnsServer->processNextRequest();
    }
    
    if (server) {
      server->handleClient();
    }
    
    if (webSocket) {
      webSocket->loop();
    }
    
    processSerialInput();
    
    delay(1);
  }
}

void AGVCoreNetwork::processSerialInput() {
  static char serialBuffer[64];
  static int bufferIndex = 0;
  
  while (Serial.available() > 0 && bufferIndex < 63) {
    char c = Serial.read();
    
    if (c == '\n' || c == '\r') {
      if (bufferIndex > 0) {
        serialBuffer[bufferIndex] = '\0';
        String cmd = String(serialBuffer);
        cmd.trim();
        
        if (cmd.length() > 0) {
          Serial.printf("\n[SERIAL] Command received: '%s'\n", cmd.c_str());
          
          // Process command immediately
          processSerialCommand(serialBuffer);
          
          // Echo back to serial
          Serial.printf("[SERIAL] Executed: %s\n", serialBuffer);
          
          // Broadcast to web clients (if not in AP mode)
          if (!isAPMode && !systemEmergency) {
            String broadcastMsg = "SERIAL: " + cmd;
            sendStatus(broadcastMsg.c_str());
          }
        }
        
        bufferIndex = 0;
      }
    } else {
      serialBuffer[bufferIndex++] = c;
    }
    
    delay(1); // Prevent watchdog trigger
  }
}

void AGVCoreNetwork::processSerialCommand(const char* cmd) {
  // Emergency commands bypass everything
  String command = String(cmd);
  command.trim();
  
  if (command.equalsIgnoreCase("STOP") || command.equalsIgnoreCase("ABORT")) {
    if (emergencyStateCallback) {
      emergencyStateCallback(true); // Trigger system-wide emergency
    }
    return;
  }
  
  if (command.equalsIgnoreCase("CLEAR_EMERGENCY")) {
    if (emergencyStateCallback) {
      emergencyStateCallback(false); // Clear system-wide emergency
    }
    return;
  }
  
  // Only process valid commands if not in emergency state
  if (systemEmergency) {
    Serial.println("[SERIAL] Command blocked: System emergency active");
    return;
  }
  
  // Send to command callback if registered
  if (commandCallback) {
    commandCallback(cmd);
  }
}

void AGVCoreNetwork::processWebCommand(const char* cmd) {
  // Only process if not in emergency state
  if (systemEmergency) {
    Serial.println("[WEB] Command blocked: System emergency active");
    return;
  }
  
  // Send to command callback if registered
  if (commandCallback) {
    commandCallback(cmd);
  }
}

void AGVCoreNetwork::webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.printf("[WS] Client #%u disconnected\n", num);
      break;
      
    case WStype_CONNECTED:
      {
        IPAddress ip = webSocket->remoteIP(num);
        Serial.printf("[WS] Client #%u connected from %d.%d.%d.%d\n", 
                     num, ip[0], ip[1], ip[2], ip[3]);
        webSocket->sendTXT(num, "AGV Connected - Ready for commands");
      }
      break;
      
    case WStype_TEXT:
      {
        // Copy command safely without holding mutex
        char cmdCopy[65];
        snprintf(cmdCopy, sizeof(cmdCopy), "%.*s", (int)length, (char*)payload);
        
        Serial.printf("\n[WS] Command received from client #%u: '%s'\n", num, cmdCopy);
        
        // Process command
        processWebCommand(cmdCopy);
        
        // Send confirmation back to client
        String response = "ACK: " + String(cmdCopy);
        webSocket->sendTXT(num, response.c_str());
        
        // Broadcast to all clients
        String broadcastMsg = "WS: " + String(cmdCopy);
        webSocket->broadcastTXT(broadcastMsg.c_str());
      }
      break;
  }
}

// Callback registration
void AGVCoreNetwork::setCommandCallback(CommandCallback callback) {
  if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) == pdPASS) {
    commandCallback = callback;
    xSemaphoreGive(mutex);
    Serial.println("[AGVNET] Command callback registered");
  }
}

void AGVCoreNetwork::setEmergencyStateCallback(EmergencyStateCallback callback) {
  if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) == pdPASS) {
    emergencyStateCallback = callback;
    xSemaphoreGive(mutex);
    Serial.println("[AGVNET] Emergency state callback registered");
  }
}

void AGVCoreNetwork::setStatusCallback(StatusCallback callback) {
  if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) == pdPASS) {
    statusCallback = callback;
    xSemaphoreGive(mutex);
    Serial.println("[AGVNET] Status callback registered");
  }
}

// Status and emergency handling
void AGVCoreNetwork::sendStatus(const char* status) {
  if (!status || strlen(status) == 0 || isAPMode) return;
  
  if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) == pdPASS) {
    if (webSocket) {
      webSocket->broadcastTXT(status);
    }
    xSemaphoreGive(mutex);
  }
  
  // Also send to serial for logging
  Serial.printf("[STATUS] %s\n", status);
  
  // Forward to status callback if registered
  if (statusCallback) {
    statusCallback(status);
  }
}

void AGVCoreNetwork::broadcastEmergency(const char* message) {
  if (!message || strlen(message) == 0) return;
  
  systemEmergency = true;
  
  Serial.printf("!!! NETWORK EMERGENCY: %s\n", message);
  
  if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) == pdPASS) {
    if (webSocket) {
      String emergencyMsg = "SYSTEM_EMERGENCY: " + String(message);
      webSocket->broadcastTXT(emergencyMsg.c_str());
    }
    xSemaphoreGive(mutex);
  }
  
  // Also send to serial
  Serial.println("!!! SYSTEM EMERGENCY STATE ACTIVE !!!");
  
  // Trigger system-wide emergency if callback exists
  if (emergencyStateCallback) {
    emergencyStateCallback(true);
  }
}

void AGVCoreNetwork::clearEmergencyState() {
  if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) == pdPASS) {
    systemEmergency = false;
    xSemaphoreGive(mutex);
  }
  
  Serial.println("[AGVNET] System emergency state cleared");
  
  // Notify web clients
  sendStatus("SYSTEM_NORMAL: Emergency cleared");
  
  // Clear system-wide emergency if callback exists
  if (emergencyStateCallback) {
    emergencyStateCallback(false);
  }
}

// Web route handlers
void AGVCoreNetwork::handleRoot() {
  if (isAPMode) {
    server->send_P(200, "text/html", wifiSetupPage);
  } else {
    server->send_P(200, "text/html", loginPage);
  }
}

void AGVCoreNetwork::handleLogin() {
  if (server->method() != HTTP_POST) {
    server->send(405, "text/plain", "Method Not Allowed");
    return;
  }
  
  String body = server->arg("plain");
  
  // Parse JSON manually to avoid String fragmentation
  int userStart = body.indexOf("\"username\":\"") + 12;
  int userEnd = body.indexOf("\"", userStart);
  String username = (userStart > 12 && userEnd > userStart) ? body.substring(userStart, userEnd) : "";
  
  int passStart = body.indexOf("\"password\":\"") + 12;
  int passEnd = body.indexOf("\"", passStart);
  String password = (passStart > 12 && passEnd > passStart) ? body.substring(passStart, passEnd) : "";
  
  Serial.printf("\n[AUTH] Login attempt: '%s'\n", username.c_str());
  
  if (username == admin_username && password == admin_password) {
    sessionToken = getSessionToken();
    String response = "{\"success\":true,\"token\":\"" + sessionToken + "\"}";
    server->send(200, "application/json", response);
    Serial.println("[AUTH] ✅ Login successful");
  } else {
    server->send(200, "application/json", "{\"success\":false}");
    Serial.println("[AUTH] ❌ Login failed");
  }
}

void AGVCoreNetwork::handleDashboard() {
  if (!validateToken()) return;
  
  // Include emergency status in dashboard
  String page = String(mainPage);
  if (systemEmergency) {
    page.replace("AGV: Waiting for connection...", "AGV: !!! EMERGENCY STATE ACTIVE !!!");
  }
  server->send(200, "text/html", page.c_str());
}

void AGVCoreNetwork::handleWiFiSetup() {
  server->send_P(200, "text/html", wifiSetupPage);
}

void AGVCoreNetwork::handleScan() {
  if (isAPMode) {
    Serial.println("[WIFI] Scanning networks...");
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
    Serial.printf("[WIFI] Found %d networks\n", n);
  } else {
    server->send(403, "text/plain", "Forbidden in station mode");
  }
}

void AGVCoreNetwork::handleSaveWiFi() {
  if (server->method() != HTTP_POST || !isAPMode) {
    server->send(403, "text/plain", "Forbidden");
    return;
  }
  
  String body = server->arg("plain");
  
  int ssidStart = body.indexOf("\"ssid\":\"") + 8;
  int ssidEnd = body.indexOf("\"", ssidStart);
  String ssid = (ssidStart > 8 && ssidEnd > ssidStart) ? body.substring(ssidStart, ssidEnd) : "";
  
  int passStart = body.indexOf("\"password\":\"") + 12;
  int passEnd = body.indexOf("\"", passStart);
  String password = (passStart > 12 && passEnd > passStart) ? body.substring(passStart, passEnd) : "";
  
  Serial.printf("\n[WIFI] Saving credentials: '%s'\n", ssid.c_str());
  
  // Save to preferences
  preferences.begin("agvnet", false);
  preferences.putString("ssid", ssid);
  preferences.putString("password", password);
  preferences.end();
  
  server->send(200, "application/json", "{\"success\":true}");
  
  Serial.println("[WIFI] ✅ Credentials saved. Restarting...");
  delay(1000);
  restartSystem();
}

void AGVCoreNetwork::handleCommand() {
  if (server->method() != HTTP_POST || systemEmergency) {
    server->send(403, "text/plain", systemEmergency ? "Emergency state active" : "Forbidden");
    return;
  }
  
  String body = server->arg("plain");
  
  int cmdStart = body.indexOf("\"command\":\"") + 11;
  int cmdEnd = body.indexOf("\"", cmdStart);
  String command = (cmdStart > 11 && cmdEnd > cmdStart) ? body.substring(cmdStart, cmdEnd) : "";
  
  if (command.length() > 0) {
    Serial.printf("[WEB] Executing command: '%s'\n", command.c_str());
    processWebCommand(command.c_str());
    server->send(200, "application/json", "{\"success\":true}");
  } else {
    server->send(400, "application/json", "{\"success\":false,\"error\":\"Invalid command\"}");
  }
}

void AGVCoreNetwork::handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server->uri();
  message += "\nMethod: ";
  message += (server->method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server->args();
  message += "\n";
  
  for (uint8_t i = 0; i < server->args(); i++) {
    message += " " + server->argName(i) + ": " + server->arg(i) + "\n";
  }
  
  server->send(404, "text/plain", message);
}

// Utility methods
String AGVCoreNetwork::getSessionToken() {
  String token = "";
  for(int i = 0; i < 32; i++) {
    token += String(random(0, 16), HEX);
  }
  return token;
}

bool AGVCoreNetwork::validateToken() {
  if (isAPMode) return true; // No auth in AP mode
  
  String auth = server->header("Authorization");
  if (auth.startsWith("Bearer ") && auth.substring(7) == sessionToken) {
    return true;
  }
  
  server->send(401, "application/json", "{\"error\":\"Unauthorized\"}");
  return false;
}

void AGVCoreNetwork::cleanupResources() {
  if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) == pdPASS) {
    if (webSocket) {
      delete webSocket;
      webSocket = nullptr;
    }
    
    if (server) {
      delete server;
      server = nullptr;
    }
    
    if (dnsServer) {
      dnsServer->stop();
      delete dnsServer;
      dnsServer = nullptr;
    }
    
    xSemaphoreGive(mutex);
  }
}

void AGVCoreNetwork::restartSystem() {
  cleanupResources();
  ESP.restart();
}
