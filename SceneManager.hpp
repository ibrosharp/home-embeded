#ifndef SCENE_MANAGER_HPP
#define SCENE_MANAGER_HPP

#include <Arduino.h>
#include <vector>
#include "PreferenceModel.hpp"
#include "JsonSerializable.hpp"
#include "StorageManager.hpp"
#include "TaskQueueManager.hpp"
#include "LoggingTask.hpp"

extern TaskQueueManager sysQueue;

class Room;
class Relay;

struct SceneAction {
    uint8_t pin;
    bool state;
};

struct Scene {
    uint8_t id;
    String name;
    std::vector<SceneAction> actions;
};

class SceneManager : public PreferenceModel, public JsonSerializable {
private:
    std::vector<Scene> _scenes;
    const char* _storageKey;
    StorageManager* _storage;
    uint8_t _nextId;

public:
    SceneManager(const char* storageKey, StorageManager& storage)
        : _storageKey(storageKey), _storage(&storage), _nextId(1) {}

    void begin() {
        if (!load(_storage->prefs())) {
            sysQueue.push(new Firmware::LoggingTask(Firmware::LogLevel::INFO, "SCENE", "No scenes loaded or first boot"));
        } else {
            sysQueue.push(new Firmware::LoggingTask(Firmware::LogLevel::INFO, "SCENE", String("Loaded ") + _scenes.size() + " scenes"));
        }
    }

    bool save(Preferences &prefs) override {
        if (_storageKey == nullptr) return false;
        
        prefs.putUShort((String(_storageKey) + "_cnt").c_str(), _scenes.size());
        prefs.putUShort((String(_storageKey) + "_nid").c_str(), _nextId);
        
        for (size_t i = 0; i < _scenes.size(); i++) {
            String prefix = String(_storageKey) + "_" + i;
            prefs.putString((prefix + "_n").c_str(), _scenes[i].name);
            prefs.putUShort((prefix + "_id").c_str(), _scenes[i].id);
            
            String actionsStr = "";
            for (size_t a = 0; a < _scenes[i].actions.size(); a++) {
                actionsStr += String(_scenes[i].actions[a].pin) + ":" + (_scenes[i].actions[a].state ? "1" : "0");
                if (a < _scenes[i].actions.size() - 1) actionsStr += ",";
            }
            prefs.putString((prefix + "_a").c_str(), actionsStr);
        }
        return true;
    }

    bool load(Preferences &prefs) override {
        if (_storageKey == nullptr) return false;
        
        if (!prefs.isKey((String(_storageKey) + "_cnt").c_str())) return false;
        
        uint16_t count = prefs.getUShort((String(_storageKey) + "_cnt").c_str(), 0);
        _nextId = prefs.getUShort((String(_storageKey) + "_nid").c_str(), 1);
        _scenes.clear();
        
        for (size_t i = 0; i < count; i++) {
            String prefix = String(_storageKey) + "_" + i;
            Scene s;
            s.name = prefs.getString((prefix + "_n").c_str(), "Scene " + String(i));
            s.id = prefs.getUShort((prefix + "_id").c_str(), i + 1);
            
            String actionsStr = prefs.getString((prefix + "_a").c_str(), "");
            if (actionsStr.length() > 0) {
                int startIndex = 0;
                int commaIndex = actionsStr.indexOf(',');
                while (commaIndex != -1) {
                    String pair = actionsStr.substring(startIndex, commaIndex);
                    int colonIndex = pair.indexOf(':');
                    if (colonIndex != -1) {
                        SceneAction act;
                        act.pin = pair.substring(0, colonIndex).toInt();
                        act.state = pair.substring(colonIndex + 1).toInt() > 0;
                        s.actions.push_back(act);
                    }
                    startIndex = commaIndex + 1;
                    commaIndex = actionsStr.indexOf(',', startIndex);
                }
                if (startIndex < actionsStr.length()) {
                    String pair = actionsStr.substring(startIndex);
                    int colonIndex = pair.indexOf(':');
                    if (colonIndex != -1) {
                        SceneAction act;
                        act.pin = pair.substring(0, colonIndex).toInt();
                        act.state = pair.substring(colonIndex + 1).toInt() > 0;
                        s.actions.push_back(act);
                    }
                }
            }
            _scenes.push_back(s);
        }
        return true;
    }

    String toJson() override {
        String json = "[";
        for (size_t i = 0; i < _scenes.size(); i++) {
            json += "{\"id\":" + String(_scenes[i].id) + ",";
            json += "\"name\":\"" + _scenes[i].name + "\",";
            json += "\"actions\":[";
            for (size_t a = 0; a < _scenes[i].actions.size(); a++) {
                json += "{\"pin\":" + String(_scenes[i].actions[a].pin) + ",\"state\":" + String(_scenes[i].actions[a].state ? "true" : "false") + "}";
                if (a < _scenes[i].actions.size() - 1) json += ",";
            }
            json += "]}";
            if (i < _scenes.size() - 1) json += ",";
        }
        json += "]";
        return json;
    }
    
    bool addScene(const String& name, const std::vector<SceneAction>& actions) {
        if (_scenes.size() >= 10) return false; // Limit to 10 scenes to avoid large JSON and EEPROM exhaustion
        Scene s;
        s.id = _nextId++;
        s.name = name;
        s.actions = actions;
        _scenes.push_back(s);
        save(_storage->prefs());
        return true;
    }
    
    bool deleteScene(uint8_t id) {
        for (auto it = _scenes.begin(); it != _scenes.end(); ++it) {
            if (it->id == id) {
                // To avoid leaving stale keys in EEPROM (which might grow indefinitely if we only overwrite),
                // we should theoretically clear the old keys, but since we re-save over prefix 0, 1, 2, etc.
                // it naturally compacts. However, the *last* element's old keys remain unless we clear them.
                String lastPrefix = String(_storageKey) + "_" + String(_scenes.size() - 1);
                _storage->prefs().remove((lastPrefix + "_n").c_str());
                _storage->prefs().remove((lastPrefix + "_id").c_str());
                _storage->prefs().remove((lastPrefix + "_a").c_str());

                _scenes.erase(it);
                save(_storage->prefs());
                return true;
            }
        }
        return false;
    }
    
    bool executeScene(uint8_t id, Room& room);
};

#endif // SCENE_MANAGER_HPP
