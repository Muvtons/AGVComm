#include "AGV_CommunicationHub.h"
#include "resources.h"

AGV_CommunicationHub commHubInstance; // Singleton instance

// WebSocket event wrapper (required by library)
void webSocketEventHandler(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  commHubInstance.handleWebSocketEvent(num, type, payload, length);
}

void AGV_CommunicationHub::begin(const char* deviceName, const char* adminUser, const char* adminPass) {
  Serial.println("\n[LIB] Initializing AGV Communication Hub...");
  
  // Initialize mutex for thread safety (CRITICAL FIX)
  mutex = xSemaphoreCreateMutex();
  
  // Store configuration
  this->mdnsName = deviceName;
  this->admin_username = adminUser;
  this->admin_password = adminPass;
  
  // Initialize hardware
  Serial.begin(115200);
  
  // Initialize preferences
  preferences.begin("wifi", false);
  stored_ssid = preferences.getString("ssid", "");
  stored_password = preferences.getString("password", "");
  preferences.end();
  
  // Setup WiFi based on stored credentials
  setupWiFi();
  
  // Start Core 0 task (handles all communication)
  xTaskCreatePinnedToCore(
    [](void* param) {
      AGV_CommunicationHub* hub = (AGV_CommunicationHub*)param;
      hub->core0Task(NULL);
    },
    "CommHubTask",
    16384,  // Increased stack size (CRITICAL FIX)
    this,
    1,      // Priority
    NULL,
    0       // Core 0
  );
  
  Serial.println("[LIB] ✅ Communication Hub started on Core 0");
}

void AGV_CommunicationHub::setCommandCallback(CommandCallback callback) {
  if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) == pdPASS) {
    this->commandCallback = callback;
    xSemaphoreGive(mutex);
    Serial.println("[LIB] Command callback registered");
  }
}

void AGV_CommunicationHub::sendStatus(const char* status) {
  if (!status || strlen(status) == 0) return;
  
  if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) == pdPASS) {
    if (!isAPMode && webSocket) {
      webSocket->broadcastTXT(status);
    }
    xSemaphoreGive(mutex);
  }
  Serial.printf("[STATUS] %s\n", status);
}

void AGV_CommunicationHub::broadcastEmergency(const char* message) {
  if (!message || strlen(message) == 0) return;
  
  if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) == pdPASS) {
    if (!isAPMode && webSocket) {
      webSocket->broadcastTXT(String("EMERGENCY: ") + message);
    }
    xSemaphoreGive(mutex);
  }
  Serial.printf("!!! EMERGENCY: %s\n", message);
}

void AGV_CommunicationHub::cleanupServer() {
  if (server) {
    delete server;
    server = nullptr;
  }
}

void AGV_CommunicationHub::cleanupWebSocket() {
  if (webSocket) {
    delete webSocket;
    webSocket = nullptr;
  }
}

void AGV_CommunicationHub::cleanupDNSServer() {
  if (dnsServer) {
    dnsServer->stop();
    delete dnsServer;
    dnsServer = nullptr;
  }
}

void AGV_CommunicationHub::setupWiFi() {
  if (stored_ssid.length() > 0) {
    Serial.println("[LIB] Found saved WiFi credentials, attempting connection...");
    startStationMode();
  } else {
    Serial.println("[LIB] No saved credentials, starting AP mode...");
    startAPMode();
  }
}

void AGV_CommunicationHub::startAPMode() {
  if (xSemaphoreTake(mutex, pdMS_TO_TICKS(1000)) != pdPASS) return;
  
  cleanupServer();
  cleanupWebSocket();
  cleanupDNSServer();
  
  Serial.println("\n[LIB] 📡 Starting Access Point Mode");
  
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid, ap_password);
  
  IPAddress IP = WiFi.softAPIP();
  Serial.printf("[LIB] AP IP: %d.%d.%d.%d\n", IP[0], IP[1], IP[2], IP[3]);
  Serial.println("[LIB] Connect to 'AGV_Controller_Setup' network");
  Serial.println("[LIB] Open browser for setup wizard");
  
  // Create DNS server for captive portal
  dnsServer = new DNSServer();
  dnsServer->start(53, "*", WiFi.softAPIP());
  
  isAPMode = true;
  
  // Setup web server
  server = new WebServer(80);
  setupRoutes();
  server->begin();
  
  xSemaphoreGive(mutex);
  Serial.println("[LIB] ✅ AP Mode Web Server Started");
}

void AGV_CommunicationHub::startStationMode() {
  if (xSemaphoreTake(mutex, pdMS_TO_TICKS(1000)) != pdPASS) return;
  
  cleanupServer();
  cleanupWebSocket();
  cleanupDNSServer();
  
  Serial.println("\n[LIB] 🌐 Starting Station Mode");
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(stored_ssid.c_str(), stored_password.c_str());
  
  Serial.printf("[LIB] Connecting to: %s\n", stored_ssid.c_str());
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[LIB] ✅ WiFi Connected!");
    Serial.printf("[LIB] IP Address: %s\n", WiFi.localIP().toString().c_str());
    
    // Start mDNS
    if (MDNS.begin(mdnsName)) {
      MDNS.addService("http", "tcp", 80);
      Serial.printf("[LIB] ✅ mDNS started: http://%s.local\n", mdnsName);
    }
    
    isAPMode = false;
    
    // Setup web server
    server = new WebServer(80);
    webSocket = new WebSocketsServer(81);
    webSocket->begin();
    webSocket->onEvent(webSocketEventHandler);
    
    setupRoutes();
    server->begin();
    
    xSemaphoreGive(mutex);
    Serial.println("[LIB] ✅ Station Mode Web Server Started");
    Serial.println("[LIB] ✅ WebSocket Server Started (Port 81)");
    
  } else {
    xSemaphoreGive(mutex);
    Serial.println("\n[LIB] ❌ WiFi connection failed");
    Serial.println("[LIB] Falling back to AP mode...");
    startAPMode();
  }
}

void AGV_CommunicationHub::setupRoutes() {
  if (!server) return;
  
  if (isAPMode) {
    // AP Mode routes (setup wizard)
    server->on("/", HTTP_GET, [this](){ this->handleWiFiSetup(); });
    server->on("/setup", HTTP_GET, [this](){ this->handleWiFiSetup(); });
    server->on("/scan", HTTP_GET, [this](){ this->handleScan(); });
    server->on("/savewifi", HTTP_POST, [this](){ this->handleSaveWiFi(); });
    server->onNotFound([this](){ this->handleCaptivePortal(); });
  } else {
    // Station Mode routes (control interface)
    server->on("/", HTTP_GET, [this](){ this->handleRoot(); });
    server->on("/login", HTTP_POST, [this](){ this->handleLogin(); });
    server->on("/dashboard", HTTP_GET, [this](){ this->handleDashboard(); });
  }
}

void AGV_CommunicationHub::core0Task(void *parameter) {
  Serial.println("[CORE0] Communication task started on Core 0");
  
  while(1) {
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(50)) == pdPASS) {
      // Handle AP mode DNS requests
      if (isAPMode && dnsServer) {
        dnsServer->processNextRequest();
      }
      
      // Handle web server requests
      if (server) {
        server->handleClient();
      }
      
      // Handle WebSocket and serial input in station mode
      if (!isAPMode) {
        if (webSocket) {
          webSocket->loop();
        }
        processSerialInput();
      }
      
      xSemaphoreGive(mutex);
    }
    
    delay(10); // Increased delay for stability (CRITICAL FIX)
  }
}

void AGV_CommunicationHub::processSerialInput() {
  static char serialBuffer[64];
  static int bufferIndex = 0;
  
  while (Serial.available() > 0) {
    char c = Serial.read();
    
    if (c == '\n' || c == '\r') {
      if (bufferIndex > 0) {
        serialBuffer[bufferIndex] = '\0';
        String cmd = String(serialBuffer);
        cmd.trim();
        
        if (cmd.length() > 0) {
          Serial.printf("\n[SERIAL] Command received: '%s'\n", cmd.c_str());
          
          // Determine priority (emergency commands get high priority)
          uint8_t priority = 0;
          if (cmd.equalsIgnoreCase("STOP") || cmd.equalsIgnoreCase("ABORT") || 
              cmd.equalsIgnoreCase("EMERGENCY")) {
            priority = 1;
          }
          
          // Send to command callback if registered
          if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) == pdPASS) {
            if (commandCallback) {
              commandCallback(serialBuffer, 1, priority); // source=1 (serial)
            }
            xSemaphoreGive(mutex);
          }
          
          // Echo back to serial
          Serial.printf("[SERIAL] Executed: %s\n", serialBuffer);
          
          // Broadcast to web clients
          if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) == pdPASS) {
            if (!isAPMode && webSocket) {
              webSocket->broadcastTXT(String("SERIAL: ") + serialBuffer);
            }
            xSemaphoreGive(mutex);
          }
        }
        
        bufferIndex = 0;
      }
    } else if (bufferIndex < sizeof(serialBuffer) - 1) {
      serialBuffer[bufferIndex++] = c;
    }
    
    // Prevent watchdog timeout during long serial input
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

String AGV_CommunicationHub::getSessionToken() {
  String token = "";
  for(int i = 0; i < 32; i++) {
    token += String(random(0, 16), HEX);
  }
  return token;
}

// WebSocket event handler - THREAD SAFE
void AGV_CommunicationHub::handleWebSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) != pdPASS) return;
  
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.printf("[WS] Client #%u disconnected\n", num);
      break;
      
    case WStype_CONNECTED:
      {
        if (webSocket) {
          IPAddress ip = webSocket->remoteIP(num);
          Serial.printf("[WS] Client #%u connected from %d.%d.%d.%d\n", 
                       num, ip[0], ip[1], ip[2], ip[3]);
          webSocket->sendTXT(num, "ESP32 Connected - Ready for commands");
        }
      }
      break;
      
    case WStype_TEXT:
      {
        char cmd[65];
        snprintf(cmd, sizeof(cmd), "%.*s", length, (char*)payload);
        
        Serial.printf("\n[WS] Command received from client #%u: '%s'\n", num, cmd);
        
        // Determine priority
        uint8_t priority = 0;
        String cmdStr = String(cmd);
        if (cmdStr.equalsIgnoreCase("STOP") || cmdStr.equalsIgnoreCase("ABORT") || 
            cmdStr.startsWith("PATH:") || cmdStr.equalsIgnoreCase("START")) {
          priority = 1;
        }
        
        // Send to command callback if registered
        if (commandCallback) {
          commandCallback(cmd, 0, priority); // source=0 (web)
        }
        
        // Echo back to sender
        if (webSocket) {
          String response = "Received: " + String(cmd);
          webSocket->sendTXT(num, response);
          
          // Broadcast to all clients using the proper method
          String broadcastMsg = "CLIENT: " + String(cmd);
          webSocket->broadcastTXT(broadcastMsg);
        }
      }
      break;
  }
  
  xSemaphoreGive(mutex);
}

// Web route handlers (THREAD SAFE)
void AGV_CommunicationHub::handleRoot() {
  if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) != pdPASS) return;
  
  if (server) {
    server->send_P(200, "text/html", loginPage);
  }
  
  xSemaphoreGive(mutex);
}

void AGV_CommunicationHub::handleLogin() {
  if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) != pdPASS) return;
  
  if (!server || server->method() != HTTP_POST) {
    xSemaphoreGive(mutex);
    return;
  }
  
  String body = server->arg("plain");
  
  int userStart = body.indexOf("\"username\":\"") + 12;
  int userEnd = body.indexOf("\"", userStart);
  String username = "";
  if (userStart > 12 && userEnd > userStart && userEnd < (int)body.length()) {
    username = body.substring(userStart, userEnd);
  }
  
  int passStart = body.indexOf("\"password\":\"") + 12;
  int passEnd = body.indexOf("\"", passStart);
  String password = "";
  if (passStart > 12 && passEnd > passStart && passEnd < (int)body.length()) {
    password = body.substring(passStart, passEnd);
  }
  
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
  
  xSemaphoreGive(mutex);
}

void AGV_CommunicationHub::handleDashboard() {
  if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) != pdPASS) return;
  
  if (server) {
    server->send_P(200, "text/html", mainPage);
  }
  
  xSemaphoreGive(mutex);
}

void AGV_CommunicationHub::handleWiFiSetup() {
  if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) != pdPASS) return;
  
  if (server) {
    server->send_P(200, "text/html", wifiSetupPage);
  }
  
  xSemaphoreGive(mutex);
}

void AGV_CommunicationHub::handleScan() {
  if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) != pdPASS) return;
  
  if (!server) {
    xSemaphoreGive(mutex);
    return;
  }
  
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
  
  xSemaphoreGive(mutex);
}

void AGV_CommunicationHub::handleSaveWiFi() {
  if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) != pdPASS) return;
  
  if (!server || server->method() != HTTP_POST) {
    xSemaphoreGive(mutex);
    return;
  }
  
  String body = server->arg("plain");
  
  int ssidStart = body.indexOf("\"ssid\":\"") + 8;
  int ssidEnd = body.indexOf("\"", ssidStart);
  String ssid = "";
  if (ssidStart > 8 && ssidEnd > ssidStart && ssidEnd < (int)body.length()) {
    ssid = body.substring(ssidStart, ssidEnd);
  }
  
  int passStart = body.indexOf("\"password\":\"") + 12;
  int passEnd = body.indexOf("\"", passStart);
  String password = "";
  if (passStart > 12 && passEnd > passStart && passEnd < (int)body.length()) {
    password = body.substring(passStart, passEnd);
  }
  
  Serial.printf("\n[WIFI] Saving credentials: '%s'\n", ssid.c_str());
  
  // Save to preferences
  preferences.begin("wifi", false);
  preferences.putString("ssid", ssid);
  preferences.putString("password", password);
  preferences.end();
  
  server->send(200, "application/json", "{\"success\":true}");
  
  xSemaphoreGive(mutex);
  
  Serial.println("[WIFI] ✅ Credentials saved. Restarting...");
  delay(1000);
  ESP.restart();
}

void AGV_CommunicationHub::handleCaptivePortal() {
  if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) != pdPASS) return;
  
  if (server) {
    server->sendHeader("Location", "http://192.168.4.1/setup", true);
    server->send(302, "text/plain", "");
  }
  
  xSemaphoreGive(mutex);
}