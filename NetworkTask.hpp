#ifndef NETWORK_TASK_HPP
#define NETWORK_TASK_HPP

#include <Arduino.h>
#include <WiFi.h>
#include "SmartWebServer.hpp"
#include "TaskQueueManager.hpp"
#include "StorageManager.hpp"
#include "LoggingTask.hpp"
#include "Room.hpp"


// Use extern to link cleanly to globals in remote.ino without collisions
extern StorageManager globalStorage;
extern SmartWebServer myWebServer;
extern TaskQueueManager sysQueue;

#define LED_INDICATOR 5

// The 'inline' keyword is critical here to prevent multiple-definition linker errors
inline void WebServerTask(void* pvParameters) {
    Serial.println("[NETWORK TASK] Booting Network Engine...");
    pinMode(LED_INDICATOR, OUTPUT);
    digitalWrite(LED_INDICATOR, LOW);
    
    WiFi.mode(WIFI_STA);

    Room& room = Room::getInstance();
    Setting* settings = room.getSetting();
    String savedSSID = "";
    String savedPass = "";
    if (settings != nullptr) {
        savedSSID = settings->getWifiSSID();
        savedPass = settings->getWifiPassword();
    } else {
        savedSSID = globalStorage.getWifiSSID();
        savedPass = globalStorage.getWifiPassword();
    }
    
    sysQueue.push(new Firmware::LoggingTask(Firmware::LogLevel::DEBUG, "server", "Recovered SSID: " + savedSSID));
    sysQueue.push(new Firmware::LoggingTask(Firmware::LogLevel::DEBUG, "server", "Recovered Pass: [PROTECTED]"));
    
    WiFi.begin(savedSSID, savedPass);

    int retryCounter = 0;
    bool ledState = false;
    while (WiFi.status() != WL_CONNECTED && retryCounter < 30) {
        ledState = !ledState;
        digitalWrite(LED_INDICATOR, ledState ? HIGH : LOW);
        vTaskDelay(pdMS_TO_TICKS(250)); // Fast blink during connection attempts
        Serial.print(".");
        retryCounter++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        digitalWrite(LED_INDICATOR, HIGH); // Solid ON when successfully connected
        sysQueue.push(new Firmware::LoggingTask(Firmware::LogLevel::DEBUG, "NETWORK", "Connected via DHCP! Capturing gateway data..."));

        IPAddress gateway    = WiFi.gatewayIP();
        IPAddress subnet     = WiFi.subnetMask();
        IPAddress primaryDNS = WiFi.dnsIP(0);
        IPAddress static_IP(192, 168, 1, 150);

        sysQueue.push(new Firmware::LoggingTask(Firmware::LogLevel::DEBUG, "NETWORK", "Discovered Gateway: " + gateway.toString()));
        sysQueue.push(new Firmware::LoggingTask(Firmware::LogLevel::DEBUG, "NETWORK", "Discovered Subnet: " + subnet.toString()));

        if (WiFi.config(static_IP, gateway, subnet, primaryDNS)) {
            sysQueue.push(new Firmware::LoggingTask(Firmware::LogLevel::DEBUG, "NETWORK", "Static IP bound successfully! Target: " + WiFi.localIP().toString()));
        } else {
            sysQueue.push(new Firmware::LoggingTask(Firmware::LogLevel::WARNING, "NETWORK", "Static IP assignment failed. Defaulting back to assigned DHCP IP."));
        }

        myWebServer.begin(SmartWebServer::MODE_OPERATIONAL);

    } else {
        sysQueue.push(new Firmware::LoggingTask(Firmware::LogLevel::DEBUG, "NETWORK", "Home Router Not Found. Falling back to Config AP..."));
        
        WiFi.disconnect(true, true); 
        WiFi.mode(WIFI_OFF);
        vTaskDelay(pdMS_TO_TICKS(100)); 
        
        WiFi.mode(WIFI_AP);
        vTaskDelay(pdMS_TO_TICKS(50)); 

        IPAddress ap_IP(192, 168, 4, 1);
        IPAddress ap_Gateway(192, 168, 4, 1);
        IPAddress ap_Subnet(255, 255, 255, 0);
        
        WiFi.softAPConfig(ap_IP, ap_Gateway, ap_Subnet);
        
        if (WiFi.softAP("ESP32_Config_Node", "12345678")) {
            sysQueue.push(new Firmware::LoggingTask(Firmware::LogLevel::DEBUG, "NETWORK", "AP Started Successfully! Fixed IP Address: " + WiFi.softAPIP().toString()));
            myWebServer.begin(SmartWebServer::MODE_CONFIG_AP);
        } else {
            sysQueue.push(new Firmware::LoggingTask(Firmware::LogLevel::ERROR, "NETWORK", "Wi-Fi Hardware failed to bring up Access Point."));
        }
    }

    bool lastConnected = (WiFi.status() == WL_CONNECTED);
    if (lastConnected) {
        digitalWrite(LED_INDICATOR, HIGH);
    }

    for(;;) {
        myWebServer.handleClient();
        
        bool currentConnected = (WiFi.status() == WL_CONNECTED);
        if (currentConnected != lastConnected) {
            lastConnected = currentConnected;
            if (currentConnected) {
                digitalWrite(LED_INDICATOR, HIGH);
            }
        }

        // Handle LED blinking if WiFi is not connected (AP/Config mode)
        if (!currentConnected) {
            static unsigned long lastBlink = 0;
            static bool apLedState = false;
            if (millis() - lastBlink > 1000) { // Slow blink (1 second interval)
                lastBlink = millis();
                apLedState = !apLedState;
                digitalWrite(LED_INDICATOR, apLedState ? HIGH : LOW);
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(10)); 
    }
}

#endif