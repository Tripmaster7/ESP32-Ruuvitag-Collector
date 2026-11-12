// AdvertisedDeviceCallbacks.cpp - Part of ESP32 Ruuvitag Collector
// Hannu Pirila 2019
#include "AdvertisedDeviceCallbacks.hpp"

using namespace std;

AdvertisedDeviceCallbacks::AdvertisedDeviceCallbacks(){
    pendingMutex=xSemaphoreCreateMutex();
}

bool AdvertisedDeviceCallbacks::isWhiteListed(const string& mac){
    for(const auto& m:config::macWhiteList){
        if(mac==m) return true;
    }
    return false;
}

// Runs in the NimBLE host task: keep it light (no storage/MQTT/stringstreams here).
void AdvertisedDeviceCallbacks::onResult(NimBLEAdvertisedDevice* advertisedDevice){
    string data=advertisedDevice->getManufacturerData();

    // Only Ruuvi manufacturer data (0x0499) is of interest
    if(data.length() < 3 || (uint8_t)data[0] != 0x99 || (uint8_t)data[1] != 0x04){
        return;
    }
    string mac=buildMac(advertisedDevice->getAddress());

    if(xSemaphoreTake(pendingMutex, pdMS_TO_TICKS(100)) == pdTRUE){
        if(pending.size() < MAX_PENDING){
            time_t now;
            time(&now);
            pending.push_back({mac, data, now});
        }
        xSemaphoreGive(pendingMutex);
    }

    // Stop early once all whitelisted sensors were seen
    if(config::macWhiteList.size()>0 && isWhiteListed(mac)){
        foundWhiteListedMacs.insert(mac);
        if(foundWhiteListedMacs.size()==config::macWhiteList.size()){
            global::pBLEScan->stop();
        }
    }
}

// Runs in the main task after the scan: decode, store and queue MQTT messages.
void AdvertisedDeviceCallbacks::processResults(){
    vector<PendingAdvertisement> results;
    if(xSemaphoreTake(pendingMutex, portMAX_DELAY) == pdTRUE){
        results.swap(pending);
        xSemaphoreGive(pendingMutex);
    }

    for(const auto& adv:results){
        // Track all discovered Ruuvi sensors regardless of whitelist
        config::discoveredSensors.insert(adv.mac);

        if(config::macWhiteList.size()>0 && !isWhiteListed(adv.mac)){
            continue;
        }
        Datahandler dh=Datahandler(adv.data,adv.mac);
        dh.setTime(adv.epoch);
        dh.buildMeasurement();
        dh.writeStorage();
        dh.sendMqtt();
    }
}

void AdvertisedDeviceCallbacks::resetScanState() {
    foundWhiteListedMacs.clear();
    if(xSemaphoreTake(pendingMutex, portMAX_DELAY) == pdTRUE){
        pending.clear();
        xSemaphoreGive(pendingMutex);
    }
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
