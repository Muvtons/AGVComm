/**
 * Example Usage of AGVComm Library
 * This demonstrates how to use the library in your main program
 * 
 * Core 0: All communication (automatically handled by library)
 * Core 1: Your AGV control logic (shown below)
 */

#include <Arduino.h>
#include "AGVComm.h"

// ============ Global Variables ============

// Create AGVComm instance
AGVComm comm;

// AGV State Variables (for Core 1)
struct AGVState {
    int sourceX, sourceY;
    int destX, destY;
    bool isRunning;
    bool isLooping;
    int loopCount;
    int currentLoop;
    String status;
} agvState;

// ============ Callback Functions ============

/**
 * This callback is called whenever a command is received
 * from WebSocket or Serial (runs on Core 0)
 */
void onCommandReceived(const char* cmd, CommandSource source) {
    Serial.printf("[MAIN] Command callback: %s from %s\n", 
                  cmd, source == SOURCE_WEBSOCKET ? "WebSocket" : "Serial");
    
    // Commands are automatically queued by the library
    // Core 1 will process them from the queue
}

/**
 * This callback is called when WebSocket connection changes
 */
void onConnectionChanged(bool connected) {
    Serial.printf("[MAIN] WebSocket connection: %s\n", 
                  connected ? "CONNECTED" : "DISCONNECTED");
}

// ============ Core 1 Task - AGV Control Logic ============

void core1AGVTask(void *parameter) {
    Serial.println("[CORE1] AGV Control task started");
    
    QueueHandle_t cmdQueue = comm.getCommandQueue();
    AGVCommand cmd;
    
    while (true) {
        // Wait for commands from the queue
        if (xQueueReceive(cmdQueue, &cmd, pdMS_TO_TICKS(100)) == pdPASS) {
            
            Serial.printf("\n[CORE1] Processing: %s\n", cmd.command);
            String command = String(cmd.command);
            command.trim();
            
            // ============ Parse Commands ============
            
            if (command.startsWith("PATH:")) {
                // Parse: PATH:sourceX,sourceY,destX,destY:ONCE or LOOP:count
                int firstColon = command.indexOf(':');
                int secondColon = command.indexOf(':', firstColon + 1);
                
                String coords = command.substring(firstColon + 1, secondColon);
                String mode = command.substring(secondColon + 1);
                
                // Parse coordinates
                int comma1 = coords.indexOf(',');
                int comma2 = coords.indexOf(',', comma1 + 1);
                int comma3 = coords.indexOf(',', comma2 + 1);
                
                agvState.sourceX = coords.substring(0, comma1).toInt();
                agvState.sourceY = coords.substring(comma1 + 1, comma2).toInt();
                agvState.destX = coords.substring(comma2 + 1, comma3).toInt();
                agvState.destY = coords.substring(comma3 + 1).toInt();
                
                // Parse mode
                if (mode.startsWith("LOOP:")) {
                    agvState.isLooping = true;
                    agvState.loopCount = mode.substring(5).toInt();
                    agvState.currentLoop = 0;
                } else {
                    agvState.isLooping = false;
                    agvState.loopCount = 1;
                }
                
                String response = String("Path updated: (") + agvState.sourceX + "," + 
                                 agvState.sourceY + ") → (" + agvState.destX + "," + 
                                 agvState.destY + ")";
                if (agvState.isLooping) {
                    response += " [Loop " + String(agvState.loopCount) + "x]";
                }
                
                comm.sendToClients(response.c_str());
                Serial.println("[CORE1] " + response);
            }
            
            else if (command == "START") {
                agvState.isRunning = true;
                agvState.currentLoop = 0;
                agvState.status = "Running";
                
                comm.sendToClients("AGV Started - Moving to destination");
                Serial.println("[CORE1] AGV Started");
            }
            
            else if (command == "STOP") {
                agvState.isRunning = false;
                agvState.status = "Stopped";
                
                comm.sendToClients("AGV Stopped");
                Serial.println("[CORE1] AGV Stopped");
            }
            
            else if (command == "ABORT") {
                agvState.isRunning = false;
                agvState.status = "Aborted";
                
                comm.sendToClients("EMERGENCY STOP - AGV Aborted");
                Serial.println("[CORE1] AGV Emergency Stop");
            }
            
            else if (command == "DEFAULT") {
                agvState.sourceX = 1;
                agvState.sourceY = 1;
                agvState.destX = 3;
                agvState.destY = 2;
                agvState.isLooping = false;
                
                comm.sendToClients("Default path loaded: (1,1) → (3,2)");
                Serial.println("[CORE1] Default path loaded");
            }
            
            else {
                String response = "Unknown command: " + command;
                comm.sendToClients(response.c_str());
                Serial.println("[CORE1] " + response);
            }
        }
        
        // ============ AGV Movement Simulation ============
        // This is where you would implement your actual AGV control logic
        
        if (agvState.isRunning) {
            // Simulate movement
            static uint32_t lastUpdate = 0;
            if (millis() - lastUpdate > 2000) { // Update every 2 seconds
                lastUpdate = millis();
                
                String status = "Moving... Current position: (" + 
                               String(random(agvState.sourceX, agvState.destX + 1)) + "," +
                               String(random(agvState.sourceY, agvState.destY + 1)) + ")";
                
                if (agvState.isLooping) {
                    status += " [Loop " + String(agvState.currentLoop + 1) + 
                             "/" + String(agvState.loopCount) + "]";
                }
                
                comm.sendToClients(status.c_str());
                Serial.println("[CORE1] " + status);
                
                // Simulate reaching destination
                if (random(0, 10) > 7) {
                    agvState.currentLoop++;
                    
                    if (agvState.isLooping && agvState.currentLoop < agvState.loopCount) {
                        comm.sendToClients("Destination reached! Starting next loop...");
                        Serial.println("[CORE1] Loop complete, starting next");
                    } else {
                        agvState.isRunning = false;
                        comm.sendToClients("✅ Destination reached! Mission complete.");
                        Serial.println("[CORE1] Mission complete");
                    }
                }
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(50)); // Yield to other tasks
    }
}

// ============ Setup ============

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n╔════════════════════════════════════════╗");
    Serial.println("║   AGV Controller with AGVComm Lib     ║");
    Serial.println("╚════════════════════════════════════════╝\n");
    
    // Initialize AGV state
    agvState.sourceX = 1;
    agvState.sourceY = 1;
    agvState.destX = 3;
    agvState.destY = 2;
    agvState.isRunning = false;
    agvState.isLooping = false;
    agvState.loopCount = 1;
    agvState.currentLoop = 0;
    agvState.status = "Idle";
    
    // Set custom admin credentials (optional)
    // comm.setAdminCredentials("myadmin", "mypassword");
    
    // Register callbacks
    comm.onCommand(onCommandReceived);
    comm.onConnection(onConnectionChanged);
    
    // Initialize communication library
    // This will handle WiFi connection, Web Server, WebSocket automatically
    if (!comm.begin("AGV_Setup", "12345678", "myagv")) {
        Serial.println("❌ Failed to initialize AGVComm!");
        while(1) { delay(1000); }
    }
    
    // Start Core 0 task automatically (handles all communication)
    comm.startCore0Task(1);
    
    // Start Core 1 task (your AGV control logic)
    xTaskCreatePinnedToCore(
        core1AGVTask,
        "AGVControl",
        8192,
        NULL,
        1,
        NULL,
        1  // Pin to Core 1
    );
    
    Serial.println("\n✅ System Ready!");
    Serial.println("   Core 0: Communication (AGVComm Library)");
    Serial.println("   Core 1: AGV Control Logic\n");
    
    // Print connection info
    if (!comm.isInAPMode()) {
        Serial.println("Access your AGV at:");
        Serial.println("  " + comm.getMDNSUrl());
        Serial.println("  http://" + comm.getIPAddress());
    } else {
        Serial.println("Setup Mode - Connect to WiFi:");
        Serial.println("  SSID: AGV_Setup");
        Serial.println("  Password: 12345678");
        Serial.println("  Then visit: http://192.168.4.1");
    }
}

// ============ Main Loop ============

void loop() {
    // Main loop is empty - all work done in FreeRTOS tasks
    
    // You can add periodic status updates here if needed
    static uint32_t lastStatusPrint = 0;
    if (millis() - lastStatusPrint > 10000) { // Every 10 seconds
        lastStatusPrint = millis();
        
        Serial.println("\n[STATUS] AGV Status Update:");
        Serial.printf("  Mode: %s\n", comm.isInAPMode() ? "Setup (AP)" : "Connected");
        Serial.printf("  Position: (%d,%d) → (%d,%d)\n", 
                     agvState.sourceX, agvState.sourceY,
                     agvState.destX, agvState.destY);
        Serial.printf("  Running: %s\n", agvState.isRunning ? "Yes" : "No");
        Serial.printf("  Status: %s\n", agvState.status.c_str());
        
        if (comm.isConnected()) {
            Serial.printf("  IP: %s\n", comm.getIPAddress().c_str());
        }
    }
    
    delay(1000);
}