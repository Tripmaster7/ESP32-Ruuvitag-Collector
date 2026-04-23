// AdvertisedDeviceCallbacks.hpp - Part of ESP32 Ruuvitag Collector
// Hannu Pirila 2019
#ifndef AdvertisedDeviceCallbacks_hpp
#define AdvertisedDeviceCallbacks_hpp

#include <BLEDevice.h>
#include "config.hpp"
#include "Datahandler.hpp"
#include <sstream>
#include <iomanip>
#include <vector>
#include <set>

class AdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks{
private:
  std::set<std::string> foundWhiteListedMacs;
  std::string buildMac(BLEAddress bleAddress);
public:
  void onResult(BLEAdvertisedDevice advertisedDevice);
  void resetScanState();
};

#endif
