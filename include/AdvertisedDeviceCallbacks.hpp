// AdvertisedDeviceCallbacks.hpp - Part of ESP32 Ruuvitag Collector
// Hannu Pirila 2019
#ifndef AdvertisedDeviceCallbacks_hpp
#define AdvertisedDeviceCallbacks_hpp

#include <NimBLEDevice.h>
#include "config.hpp"
#include "Datahandler.hpp"
#include <vector>
#include <set>

// onResult() runs in the NimBLE host task (small stack). It only collects raw
// advertisements; decoding, storage and MQTT happen in processResults(),
// which must be called from the main task after the scan finished.
class AdvertisedDeviceCallbacks: public NimBLEAdvertisedDeviceCallbacks{
private:
  struct PendingAdvertisement {
    std::string mac;
    std::string data;
    time_t epoch;
  };
  static const size_t MAX_PENDING = 64;

  std::set<std::string> foundWhiteListedMacs;
  std::vector<PendingAdvertisement> pending;
  SemaphoreHandle_t pendingMutex;

  std::string buildMac(const NimBLEAddress& bleAddress);
  bool isWhiteListed(const std::string& mac);
public:
  AdvertisedDeviceCallbacks();
  void onResult(NimBLEAdvertisedDevice* advertisedDevice) override;
  void processResults();
  void resetScanState();
};

#endif
