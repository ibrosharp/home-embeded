#ifndef SETTING_H
#define SETTING_H

#include "PreferenceModel.hpp"
#include "JsonSerializable.hpp"
#include "StorageManager.hpp"
#include <Arduino.h>

extern void triggerFeedback();

class Setting : public PreferenceModel, public JsonSerializable {
private:
    String wifiSSID;
    String wifiPassword;
    float defaultTargetTemp;
    bool ledFeedbackEnabled;
    String authUsername;
    String authPassword;
    String guestPassword;
    const char* namespaceName; // NVS requires a namespace (max 15 characters)
    StorageManager* _storage;

public:
    Setting(const char* ns, StorageManager* storage = nullptr) 
        : namespaceName(ns), defaultTargetTemp(24.0), ledFeedbackEnabled(true), _storage(storage), authUsername("admin"), authPassword("admin"), guestPassword("guest") {}

    void setStorage(StorageManager* storage) { _storage = storage; }

    // Getters and Setters
    void setWifi(String ssid, String pass) { 
        wifiSSID = ssid; 
        wifiPassword = pass; 
        if (_storage) {
            save(_storage->prefs());
        }
        triggerFeedback();
    }
    void setTargetTemp(float temp) { 
        defaultTargetTemp = temp; 
        if (_storage) {
            save(_storage->prefs());
        }
        triggerFeedback();
    }
    float getTargetTemp() { return defaultTargetTemp; }
    String getWifiSSID() const { return wifiSSID; }
    String getWifiPassword() const { return wifiPassword; }
    bool isLedFeedbackEnabled() const { return ledFeedbackEnabled; }
    void setLedFeedbackEnabled(bool enabled) { 
        ledFeedbackEnabled = enabled; 
        if (_storage) {
            save(_storage->prefs());
        }
        triggerFeedback();
    }
    
    String getAuthUsername() const { return authUsername; }
    String getAuthPassword() const { return authPassword; }
    String getGuestPassword() const { return guestPassword; }
    void setAuth(String user, String pass, String guestPass) {
        authUsername = user;
        authPassword = pass;
        guestPassword = guestPass;
        if (_storage) {
            save(_storage->prefs());
        }
    }
    
    const char* getNamespace() const { return namespaceName; }

    // Implement the toJson() interface method
    String toJson() override {
        String json = "{";
        json += "\"type\":\"settings\",";
        json += "\"wifiSSID\":\"" + wifiSSID + "\",";
        json += "\"wifiPassword\":\"" + wifiPassword + "\",";
        json += "\"authUsername\":\"" + authUsername + "\",";
        json += "\"authPassword\":\"" + authPassword + "\",";
        json += "\"guestPassword\":\"" + guestPassword + "\",";
        json += "\"defaultTargetTemp\":" + String(defaultTargetTemp, 2) + ",";
        json += "\"ledFeedbackEnabled\":" + String(ledFeedbackEnabled ? "true" : "false") + ",";
        json += "\"namespace\":\"" + String(namespaceName) + "\"";
        json += "}";
        return json;
    }

    // Implement the save() interface method
    bool save(Preferences &prefs) override {
        prefs.putString("wifi_ssid", wifiSSID);
        prefs.putString("wifi_pass", wifiPassword);
        prefs.putString("auth_user", authUsername);
        prefs.putString("auth_pass", authPassword);
        prefs.putString("guest_pass", guestPassword);
        prefs.putFloat("temp", defaultTargetTemp);
        prefs.putBool("led", ledFeedbackEnabled);
        return true;
    }

    // Implement the load() interface method
    bool load(Preferences &prefs) override {
        // The second argument provides a default value if the key doesn't exist yet
        wifiSSID = prefs.getString("wifi_ssid", "");
        wifiPassword = prefs.getString("wifi_pass", "");
        authUsername = prefs.getString("auth_user", "admin");
        authPassword = prefs.getString("auth_pass", "admin");
        guestPassword = prefs.getString("guest_pass", "guest");
        defaultTargetTemp = prefs.getFloat("temp", 24.0);
        ledFeedbackEnabled = prefs.getBool("led", true);
        return true;
    }
};

#endif
