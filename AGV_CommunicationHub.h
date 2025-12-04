#ifndef AGV_COMMUNICATION_HUB_H
#define AGV_COMMUNICATION_HUB_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <DNSServer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

// Command structure for inter-core communication
typedef struct {
  char command[64];
  uint8_t source;  // 0=WebSocket, 1=Serial
  uint8_t priority; // 0=normal, 1=high (STOP/ABORT)
} AGVCommand;

class AGV_CommunicationHub {
public:
  // Callback function type for command processing
  typedef void (*CommandCallback)(const char* command, uint8_t source, uint8_t priority);
  
  // Initialize the communication hub - 1 of 4 lines
  void begin(const char* deviceName = "agvcontrol", 
             const char* adminUser = "admin", 
             const char* adminPass = "admin123");
  
  // Set callback for received commands - 2 of 4 lines
  void setCommandCallback(CommandCallback callback);
  
  // Send status update to all interfaces - 3 of 4 lines
  void sendStatus(const char* status);
  
  // Emergency broadcast (bypasses normal queue)
  void broadcastEmergency(const char* message);
  
  // WebSocket event handler - PUBLIC
  void handleWebSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length);
  
  // Library handles everything else automatically
  void loop(); // Called automatically in background task

private:
  // Internal implementation details hidden from user
  WebServer* server = nullptr;
  WebSocketsServer* webSocket = nullptr;
  DNSServer* dnsServer = nullptr;
  Preferences preferences;
  
  String stored_ssid;
  String stored_password;
  String admin_username;
  String admin_password;
  String sessionToken;
  
  bool isAPMode = false;
  const char* mdnsName = nullptr;
  const char* ap_ssid = "AGV_Controller_Setup";
  const char* ap_password = "12345678";
  
  CommandCallback commandCallback = nullptr;
  SemaphoreHandle_t mutex = nullptr;  // Added mutex for thread safety
  
  // Internal methods (hidden from user)
  void setupWiFi();
  void startAPMode();
  void startStationMode();
  void setupRoutes();
  void processSerialInput();
  String getSessionToken();
  void core0Task(void *parameter);
  
  // Web route handlers
  void handleRoot();
  void handleLogin();
  void handleDashboard();
  void handleWiFiSetup();
  void handleScan();
  void handleSaveWiFi();
  void handleCaptivePortal();
  
  // Cleanup methods
  void cleanupServer();
  void cleanupWebSocket();
  void cleanupDNSServer();
};

#endif