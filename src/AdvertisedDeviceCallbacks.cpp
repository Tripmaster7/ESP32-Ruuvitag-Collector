// AdvertisedDeviceCallbacks.cpp - Part of ESP32 Ruuvitag Collector
// Hannu Pirila 2019
#include "AdvertisedDeviceCallbacks.hpp"

using namespace std;

void AdvertisedDeviceCallbacks::onResult(NimBLEAdvertisedDevice* advertisedDevice){
    bool processMac=false;
    string data=advertisedDevice->getManufacturerData();
    string mac=buildMac(advertisedDevice->getAddress());

    // Track all discovered Ruuvi sensors regardless of whitelist
    if(data.length() >= 2 && (uint8_t)data[0] == 0x99 && (uint8_t)data[1] == 0x04) {
        config::discoveredSensors.insert(mac);
    }

    for(auto m:config::macWhiteList){
        if(mac==m){
            foundWhiteListedMacs.insert(mac);
            processMac=true;
        }
    }
    if(config::macWhiteList.size()<1){
        processMac=true;
    }
    if(processMac){
        Datahandler dh=Datahandler(data,mac);
        dh.setTime();
        dh.buildMeasurement();
        dh.writeStorage();
        dh.sendMqtt();
    }
    if(config::macWhiteList.size()>0){
        if(foundWhiteListedMacs.size()==config::macWhiteList.size()){
            global::pBLEScan->stop();
        }
    }
}

void AdvertisedDeviceCallbacks::resetScanState() {
    foundWhiteListedMacs.clear();
}

// "aa:bb:cc:dd:ee:ff" -> "AABBCCDDEEFF" (same format as before the NimBLE switch).
// Note: NimBLEAddress::getNative() is little-endian, so use toString() instead.
string AdvertisedDeviceCallbacks::buildMac(const NimBLEAddress& bleAddress) {
    string s=bleAddress.toString();
    string mac;
    mac.reserve(12);
    for(char c:s){
        if(c!=':') mac+=(char)toupper((unsigned char)c);
    }
    return mac;
}