// config.cpp - Part of ESP32 Ruuvitag Collector
// Hannu Pirila 2019
#include "config.hpp"

namespace config{
    // WiFi default credentials (used as fallback; WiFi bootstrapper takes priority)
    // CHANGE THESE or use the captive portal to configure WiFi
    const std::string wiFiSSD="MyHomeWifiAP";
    const std::string wiFiPassword="sweethome";
    // NTP server, use IP address instead of name
    const std::string ntpServerIP="216.239.35.0";

    // Only MAC values listed here are scanned or empty list will scan everything. 
    // Example: {"D96BF9D2116A","C2CA7F7D07F5"} will scan only these two MAC's
    // Example: {} empty list, everything is scanned. Start with empty list
    // and set MAC's to white list once you know the values. The system works more
    // efficiently with white list as bluetooth scan is stopped when list is fulfilled.
    std::vector<std::string>macWhiteList={};
    std::set<std::string> discoveredSensors;

    // MQTT configuration
    // MQTT server a.k.a broker IP address
    std::string mqttServerIP="192.168.1.100";
    // and port number
    int mqttServerPort=1883;
    // The topic name this ESP32 is publishing
    // The complete topic will be mqttTopicPrefix/+Ruuvitag mac address/+state
    // Example: ruuviesp32/C2CA7F7D07F5/state
    std::string mqttTopicPrefix="ruuviesp32";
    // Username to connect MQTT server
    std::string mqttServerUsername="thepublisher";
    // and password
    std::string mqttServerPassword="iamthepublisher";

    // Power save: true=deep sleep between scans, false=WiFi stays on
    bool powerSave = true;
    // Test mode: true=scan every 10s, false=production interval (~15 min)
    bool testMode = false;

    // timeZone: The sign is positive if the local time zone is west of the Prime Meridian and negative if it is east.
    const std::string timeZone="UTC-3";

    // How often Ruuvitag measurements are collected.
    // Example value  60 causes wake-up at 12:00:00, 12:01:00, 12:02:00... and   ~50 s deep sleep.
    // Example value 120 causes wake-up at 12:00:00, 12:02:00, 12:04:00... and  ~110 s deep sleep.
    // Do not set value below 30 as there is maybe not enough time to handle
    // all the collected data in shorter time periods.
    const int deepSleepWakeUpAtSecond=60;
    // Example value -2 causes wake-up at 12:00:58, 12:01:58, 12:02:58
    const int deepSleepWakeUpOffset=0;
    
    // How often WiFi is turned ON. 
    // Example value 3600 turns WiFi on every hour, value 7200 every two hour etc.
    // Value 0 causes WiFi to turn on every time the ESP wakes from deep sleep. 
    // Set value zero for real-time data reporting and higher values to save 
    // energy. All other network related operations like MQTT and
    // NTP are run only when WiFi is ON.
    const int turnOnWifiEvery=900;

    // Watchdog will reset the ESP32 if no action in specified time
    const int watchdogTimeout=60;
}

namespace global{

    BLEScan* pBLEScan;
    const int BLEscanTime=10;
    RTC_DATA_ATTR int bootCount=1;
    const uint64_t nSToSFactor=1000000000;
    const uint64_t uSToSFactor=1000000;
    const uint64_t mSToSFactor=1000;
    bool successfulRun=true;
    const std::vector<RecordHeader> validHeaders={RecordHeader{20,0},RecordHeader{24,1}};

}
