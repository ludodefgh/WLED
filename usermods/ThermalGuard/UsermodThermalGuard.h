#pragma once

#include "wled.h"
#include "../Temperature/UsermodTemperature.h"

class UsermodThermalGuard : public Usermod {
  public:
    enum ThermalState { STATE_NORMAL, STATE_THROTTLE, STATE_OFF };

  private:
    static const char _name[];
    static const char _enabled[];

    struct StateConfig {
      uint8_t preset;
      uint8_t brightness;
      StateConfig(uint8_t p = 0, uint8_t b = 255) : preset(p), brightness(b) {}
    };

    bool  enabled        = true;
    float threshNormal   = 50.0f;
    float threshThrottle = 60.0f;
    float threshOff      = 70.0f;
    float hysteresis     = 3.0f;
    unsigned long checkInterval = 10000;

    StateConfig cfgNormal;
    StateConfig cfgThrottle  = StateConfig(0, 128);
    StateConfig cfgOff       = StateConfig(0, 0);

    ThermalState state    = STATE_NORMAL;
    unsigned long lastCheck = 0;
    bool initDone          = false;

    void applyStateConfig(const StateConfig& cfg) {
      if (cfg.preset > 0) {
        applyPreset(cfg.preset, CALL_MODE_DIRECT_CHANGE);
      } else {
        bri = cfg.brightness;
        stateUpdated(CALL_MODE_DIRECT_CHANGE);
      }
    }

    void applyState(ThermalState newState) {
      if (newState == state && initDone) return;
      state = newState;
      switch (state) {
        case STATE_NORMAL:   applyStateConfig(cfgNormal);   break;
        case STATE_THROTTLE: applyStateConfig(cfgThrottle); break;
        case STATE_OFF:      applyStateConfig(cfgOff);      break;
      }
    }

    void saveStateConfig(JsonObject& obj, const StateConfig& cfg) {
      obj["preset"]     = cfg.preset;
      obj["brightness"] = cfg.brightness;
    }

    void loadStateConfig(JsonObject& obj, StateConfig& cfg) {
      getJsonValue(obj["preset"],     cfg.preset);
      getJsonValue(obj["brightness"], cfg.brightness);
    }

  public:
    void setup() override {
      pinMode(48, OUTPUT);
      digitalWrite(48, LOW);
      initDone = true;
    }

    void loop() override {
      if (!enabled) return;
      unsigned long now = millis();
      if (now - lastCheck < checkInterval) return;
      lastCheck = now;

      UsermodTemperature* tempMod = UsermodTemperature::getInstance();
      if (!tempMod) return;

      float t = tempMod->getTemperatureC();
      if (t <= -100.0f) return;

      if (t >= threshOff) {
        applyState(STATE_OFF);
      } else if (t >= threshThrottle) {
        applyState(STATE_THROTTLE);
      } else if (state == STATE_OFF && t < threshOff - hysteresis) {
        applyState(STATE_THROTTLE);
      } else if (state == STATE_THROTTLE && t < threshNormal) {
        applyState(STATE_NORMAL);
      }
    }

    void addToJsonInfo(JsonObject& root) override {
      JsonObject user = root["u"];
      if (user.isNull()) user = root.createNestedObject("u");
      JsonArray arr = user.createNestedArray(FPSTR(_name));
      switch (state) {
        case STATE_NORMAL:   arr.add("Normal");   break;
        case STATE_THROTTLE: arr.add("Throttle"); break;
        case STATE_OFF:      arr.add("Off");      break;
      }
    }

    void addToConfig(JsonObject& root) override {
      JsonObject top = root.createNestedObject(FPSTR(_name));
      top[FPSTR(_enabled)]   = enabled;
      top["thresh_normal"]   = threshNormal;
      top["thresh_throttle"] = threshThrottle;
      top["thresh_off"]      = threshOff;
      top["hysteresis"]      = hysteresis;
      top["interval_ms"]     = checkInterval;
      JsonObject n2 = top.createNestedObject("normal");   saveStateConfig(n2, cfgNormal);
      JsonObject th2 = top.createNestedObject("throttle"); saveStateConfig(th2, cfgThrottle);
      JsonObject o2 = top.createNestedObject("off");      saveStateConfig(o2, cfgOff);
    }

    bool readFromConfig(JsonObject& root) override {
      JsonObject top = root[FPSTR(_name)];
      if (top.isNull()) return false;
      getJsonValue(top[FPSTR(_enabled)],   enabled);
      getJsonValue(top["thresh_normal"],   threshNormal);
      getJsonValue(top["thresh_throttle"], threshThrottle);
      getJsonValue(top["thresh_off"],      threshOff);
      getJsonValue(top["hysteresis"],      hysteresis);
      getJsonValue(top["interval_ms"],     checkInterval);
      JsonObject n = top["normal"];   if (!n.isNull()) loadStateConfig(n, cfgNormal);
      JsonObject th = top["throttle"]; if (!th.isNull()) loadStateConfig(th, cfgThrottle);
      JsonObject o = top["off"];      if (!o.isNull()) loadStateConfig(o, cfgOff);
      return true;
    }

    uint16_t getId() override { return USERMOD_ID_UNSPECIFIED; }
};

const char UsermodThermalGuard::_name[]    PROGMEM = "ThermalGuard";
const char UsermodThermalGuard::_enabled[] PROGMEM = "enabled";
