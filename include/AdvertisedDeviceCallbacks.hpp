// AdvertisedDeviceCallbacks.hpp - Part of ESP32 Ruuvitag Collector
// Hannu Pirila 2019
#ifndef AdvertisedDeviceCallbacks_hpp
#define AdvertisedDeviceCallbacks_hpp

#include <NimBLEDevice.h>
#include "config.hpp"
#include "Datahandler.hpp"
#include <vector>
#include <set>

class AdvertisedDeviceCallbacks: public NimBLEAdvertisedDeviceCallbacks{
private:
  std::set<std::string> foundWhiteListedMacs;
  std::string buildMac(const NimBLEAddress& bleAddress);
public:
  void onResult(NimBLEAdvertisedDevice* advertisedDevice) override;
  void resetScanState();
};

#endif
