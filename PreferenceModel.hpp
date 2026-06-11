#ifndef PREFERENCE_MODEL_H
#define PREFERENCE_MODEL_H

#include <Arduino.h>
#include <Preferences.h>

class PreferenceModel {
public:
    virtual ~PreferenceModel() {}
    virtual bool save(Preferences &prefs) = 0;
    virtual bool load(Preferences &prefs) = 0;
};

#endif
