// WifiBootstrap.hpp - Part of ESP32 Ruuvitag Collector
// WiFi provisioning via captive portal AP
#ifndef WifiBootstrap_hpp
#define WifiBootstrap_hpp

#include <Arduino.h>
#include <string>

namespace wifibootstrap {
    // Try saved credentials (NVS), then compile-time defaults.
    // If both fail, start AP with captive portal for configuration.
    // Returns true if WiFi is connected after this call.
    bool begin();

    // Erase stored WiFi credentials from NVS
    void clearCredentials();

    // Check if credentials are stored in NVS
    bool hasStoredCredentials();
}

#endif
