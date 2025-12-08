#ifndef AGVCORENETWORK_H
#define AGVCORENETWORK_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <DNSServer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

namespace AGVCoreNetworkLib {

class AGVCoreNetwork {
public:
  // Callback function types
  typedef void (*CommandCallback)(const char* command);
  typedef void (*EmergencyStateCallback)(bool);
  typedef void (*StatusCallback)(const char* status);
  
  // Initialize the network system
  void begin(const char* deviceName = "agvcontrol", 
             const char* adminUser = "admin", 
             const char* adminPass = "admin123");
  
  // Set callbacks for system integration
  void setCommandCallback(CommandCallback callback);
  void setEmergencyStateCallback(EmergencyStateCallback callback);
  void setStatusCallback(StatusCallback callback);
  
  // Send status update to web clients
  void sendStatus(const char* status);
  
  // Emergency broadcast and state management
  void broadcastEmergency(const char* message);
  void clearEmergencyState();
  
  // WebSocket event handler
  void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length);
  
  // Check connectivity status
  bool isConnected() const { return WiFi.status() == WL_CONNECTED && !isAPMode; }
  bool isInAPMode() const { return isAPMode; }

private:
  // Network resources
  WebServer* server = nullptr;
  WebSocketsServer* webSocket = nullptr;
  DNSServer* dnsServer = nullptr;
  Preferences preferences;
  
  // Configuration
  String stored_ssid;
  String stored_password;
  String admin_username;
  String admin_password;
  String sessionToken;
  
  // System state
  bool isAPMode = false;
  bool systemEmergency = false;
  const char* mdnsName = nullptr;
  
  // Default AP credentials
  const char* ap_ssid = "AGV_Controller";
  const char* ap_password = "AGV_Secure123";
  
  // Callbacks
  CommandCallback commandCallback = nullptr;
  EmergencyStateCallback emergencyStateCallback = nullptr;
  StatusCallback statusCallback = nullptr;
  
  // Synchronization
  SemaphoreHandle_t mutex = nullptr;
  TaskHandle_t core0TaskHandle = nullptr;
  
  // Internal methods
  void setupWiFi();
  void startAPMode();
  void startStationMode();
  void setupRoutes();
  void processSerialInput();
  void core0Task(void *parameter);
  
  // Web handlers
  void handleRoot();
  void handleLogin();
  void handleDashboard();
  void handleWiFiSetup();
  void handleScan();
  void handleSaveWiFi();
  void handleCommand();
  void handleNotFound();
  
  // Utility methods
  String getSessionToken();
  bool validateToken();
  void cleanupResources();
  void restartSystem();
  
  // Command processing
  void processWebCommand(const char* cmd);
  void processSerialCommand(const char* cmd);
};

} // namespace AGVCoreNetworkLib

// Global instance
extern AGVCoreNetworkLib::AGVCoreNetwork agvNetwork;

#endif
