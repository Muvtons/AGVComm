/**
 * AGVComm.h - AGV Communication Library
 * Handles all WiFi, WebSocket, Web Server, and Serial communication on Core 0
 * 
 * Author: Your Name
 * Version: 1.0.0
 * License: MIT
 */

#ifndef AGV_COMM_H
#define AGV_COMM_H

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

// Configuration constants
#define MAX_COMMAND_LENGTH 256
#define COMMAND_QUEUE_SIZE 20
#define WEBSOCKET_PORT 81
#define WEB_SERVER_PORT 80
#define DNS_PORT 53
#define SERIAL_BAUD_RATE 115200

// Command source enumeration
enum CommandSource {
    SOURCE_WEBSOCKET = 0,
    SOURCE_SERIAL = 1,
    SOURCE_INTERNAL = 2
};

// Command structure for inter-core communication
typedef struct {
    char command[MAX_COMMAND_LENGTH];
    uint8_t source;
    uint32_t timestamp;
} AGVCommand;

// Callback function types
typedef void (*CommandCallback)(const char* cmd, CommandSource source);
typedef void (*StatusCallback)(const char* status);
typedef void (*ConnectionCallback)(bool connected);

class AGVComm {
public:
    // Constructor & Destructor
    AGVComm();
    ~AGVComm();
    
    // ============ Initialization ============
    
    /**
     * Initialize the communication system
     * @param apSSID Access Point SSID (default: "AGV_Controller_Setup")
     * @param apPassword Access Point password (default: "12345678")
     * @param mdnsName mDNS name for station mode (default: "agvcontrol")
     * @return true if initialization successful
     */
    bool begin(const char* apSSID = "AGV_Controller_Setup", 
               const char* apPassword = "12345678",
               const char* mdnsName = "agvcontrol");
    
    /**
     * Set admin credentials for web interface
     * @param username Admin username
     * @param password Admin password
     */
    void setAdminCredentials(const char* username, const char* password);
    
    // ============ Callbacks ============
    
    /**
     * Set callback for receiving commands
     * @param callback Function to call when command received
     */
    void onCommand(CommandCallback callback);
    
    /**
     * Set callback for connection status changes
     * @param callback Function to call on connection change
     */
    void onConnection(ConnectionCallback callback);
    
    // ============ Command Handling ============
    
    /**
     * Send response/status to all connected clients
     * @param message Message to broadcast
     */
    void sendToClients(const char* message);
    
    /**
     * Send response to specific WebSocket client
     * @param clientNum Client number
     * @param message Message to send
     */
    void sendToClient(uint8_t clientNum, const char* message);
    
    /**
     * Get command queue handle (for Core 1)
     * @return Queue handle
     */
    QueueHandle_t getCommandQueue();
    
    // ============ Network Information ============
    
    /**
     * Check if in Access Point mode
     * @return true if in AP mode
     */
    bool isInAPMode();
    
    /**
     * Check if WiFi is connected (Station mode)
     * @return true if connected
     */
    bool isConnected();
    
    /**
     * Get current IP address
     * @return IP address as String
     */
    String getIPAddress();
    
    /**
     * Get mDNS URL
     * @return mDNS URL as String (e.g., "http://agvcontrol.local")
     */
    String getMDNSUrl();
    
    /**
     * Get number of connected WebSocket clients
     * @return Number of clients
     */
    uint8_t getClientCount();
    
    // ============ WiFi Management ============
    
    /**
     * Check if WiFi credentials are saved
     * @return true if credentials exist
     */
    bool hasStoredCredentials();
    
    /**
     * Clear stored WiFi credentials
     */
    void clearCredentials();
    
    /**
     * Restart ESP32
     */
    void restart();
    
    // ============ Core Task ============
    
    /**
     * Main task loop (runs on Core 0)
     * Should be called from FreeRTOS task
     */
    void loop();
    
    /**
     * Start Core 0 task automatically
     * @param priority Task priority (default: 1)
     * @return Task handle
     */
    TaskHandle_t startCore0Task(uint8_t priority = 1);

private:
    // Network objects
    WebServer* server;
    WebSocketsServer* webSocket;
    DNSServer* dnsServer;
    Preferences preferences;
    
    // Configuration
    String ap_ssid;
    String ap_password;
    String mdns_name;
    String stored_ssid;
    String stored_password;
    String admin_username;
    String admin_password;
    String session_token;
    
    // State
    bool is_ap_mode;
    bool is_authenticated;
    bool is_initialized;
    
    // Communication
    QueueHandle_t command_queue;
    SemaphoreHandle_t command_mutex;
    TaskHandle_t core0_task_handle;
    
    // Callbacks
    CommandCallback cmd_callback;
    ConnectionCallback conn_callback;
    
    // Private methods
    void startAPMode();
    void startStationMode();
    void setupWebServer();
    void setupWebSocket();
    String generateSessionToken();
    
    // Web handlers
    void handleRoot();
    void handleLogin();
    void handleDashboard();
    void handleWiFiSetup();
    void handleScan();
    void handleSaveWiFi();
    void handleNotFound();
    void handleCaptivePortal();
    
    // WebSocket handler
    static void webSocketEventStatic(uint8_t num, WStype_t type, uint8_t* payload, size_t length);
    void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length);
    
    // Serial handling
    void processSerial();
    
    // Static instance for callbacks
    static AGVComm* instance;
    
    // Core 0 task function
    static void core0TaskFunction(void* parameter);
    
    // HTML pages
    static const char* loginPageHTML;
    static const char* wifiSetupPageHTML;
    static const char* mainPageHTML;
};

#endif // AGV_COMM_H