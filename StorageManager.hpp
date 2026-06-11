#ifndef STORAGE_MANAGER_H
#define STORAGE_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>

class StorageManager {
private:
    Preferences _prefs;
    const char* _namespace; // The unique partition name for your settings

public:
    // Constructor: Takes a namespace string (max 15 characters)
    StorageManager(const char* ns = "sys_config");

    // Initialize/open the storage partition
    bool begin();

    // Clear all stored keys in this namespace
    void clearAll();

    // Expose the underlying opened Preferences instance for shared persistence access.
    Preferences& prefs();

    // --- Wi-Fi Configuration Helpers ---
    void saveWifiCredentials(const String& ssid, const String& password);
    String getWifiSSID();
    String getWifiPassword();

    // --- System Flag Helpers (Example: For keeping state across reboots) ---
    void saveSystemMode(uint8_t mode);
    uint8_t getSystemMode(uint8_t defaultMode = 0);

    // Direct read/write helpers for persisted device state
    bool putBool(const char* key, bool value);
    bool putUChar(const char* key, uint8_t value);
    bool getBool(const char* key, bool defaultValue);
    uint8_t getUChar(const char* key, uint8_t defaultValue);
};

// Implementation

StorageManager::StorageManager(const char* ns) : _namespace(ns) {}

bool StorageManager::begin() {
    // Open the namespace in Read/Write mode (false = R/W, true = Read-only)
    return _prefs.begin(_namespace, false);
}

void StorageManager::clearAll() {
    _prefs.clear();
}

Preferences& StorageManager::prefs() {
    return _prefs;
}

void StorageManager::saveWifiCredentials(const String& ssid, const String& password) {
    // Save strings into flash memory using explicit keys
    _prefs.putString("wifi_ssid", ssid);
    _prefs.putString("wifi_pass", password);
}

String StorageManager::getWifiSSID() {
    // If the key doesn't exist, it returns an empty string ""
    return _prefs.getString("wifi_ssid", "");
}

String StorageManager::getWifiPassword() {
    return _prefs.getString("wifi_pass", "");
}

void StorageManager::saveSystemMode(uint8_t mode) {
    // UChar stores a standard 8-bit unsigned integer (uint8_t)
    _prefs.putUChar("sys_mode", mode);
}

uint8_t StorageManager::getSystemMode(uint8_t defaultMode) {
    // Returns the saved byte, or the fallback default if never saved
    return _prefs.getUChar("sys_mode", defaultMode);
}

bool StorageManager::putBool(const char* key, bool value) {
    _prefs.putBool(key, value);
    return true;
}

bool StorageManager::putUChar(const char* key, uint8_t value) {
    _prefs.putUChar(key, value);
    return true;
}

bool StorageManager::getBool(const char* key, bool defaultValue) {
    return _prefs.getBool(key, defaultValue);
}

uint8_t StorageManager::getUChar(const char* key, uint8_t defaultValue) {
    return _prefs.getUChar(key, defaultValue);
}

#endif
