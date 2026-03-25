// MqttConfig.hpp - Part of ESP32 Ruuvitag Collector
// Web-based MQTT and sensor configuration portal
#ifndef MqttConfig_hpp
#define MqttConfig_hpp

#include <Arduino.h>

namespace mqttconfig {
    // Load MQTT settings from NVS into config:: namespace variables
    void loadSettings();

    // Check if MQTT is configured in NVS
    bool hasStoredSettings();

    // Run the config web server (blocking until save or timeout)
    // Call after WiFi is connected. Serves on the device's local IP.
    void runConfigPortal();

    // Run the config portal until stopCondition() returns true.
    // Also pumps MQTT loop while waiting.
    void runConfigPortalUntil(bool (*stopCondition)());

    // Clear all NVS config (WiFi + MQTT) and restart
    void resetAllConfig();
}

#endif
