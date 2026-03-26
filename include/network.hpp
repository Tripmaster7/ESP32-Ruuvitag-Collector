// network.hpp - Part of ESP32 Ruuvitag Collector
// Hannu Pirila 2019
#ifndef network_hpp
#define network_hpp

#include <Arduino.h>
#include <WiFi.h>
#include "config.hpp"
#include "WifiBootstrap.hpp"
#include "PubSubClient.h"
#include <sstream>
#include <vector>
#include <utility>

namespace network {
    namespace wifi{
        bool begin();
    }
    namespace ntp{
        void update();
    }
    namespace mqtt{
        bool begin();
        void publish(std::string topic,std::string payload);
        void bufferMessage(std::string topic,std::string payload);
        void flushBuffer();
        bool bufferEmpty();
        void subscribe();
        void loop();
        bool isConnected();
        bool isPortalRequested();
        void clearPortalFlag();
    }
}

#endif
