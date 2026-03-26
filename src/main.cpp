// main.cpp - Part of ESP32 Ruuvitag Collector
// Hannu Pirila 2019
#include <Arduino.h>
#include <esp_ota_ops.h>
#include "config.hpp"
#include "AdvertisedDeviceCallbacks.hpp"
#include "storage.hpp"
#include "network.hpp"
#include "timer.hpp"
#include "MqttConfig.hpp"

void setup() {
  Serial.begin(115200);
  esp_ota_mark_app_valid_cancel_rollback();

  // Report boot partition
  const esp_partition_t* running = esp_ota_get_running_partition();
  Serial.printf("Firmware v" FIRMWARE_VERSION " running from partition: %s\n",
                running ? running->label : "unknown");

  timer::watchdog::set();
  timer::watchdog::feed();
  timer::deepsleep::printBootCount();

  mqttconfig::loadSettings();

  if(timer::wifi::isWifiNeeded()){
    bool wifiOk = network::wifi::begin();
    if(wifiOk){
      network::ntp::update();
      if(!mqttconfig::hasStoredSettings()){
        mqttconfig::runConfigPortal();
      }
    }
  }

  timer::printLocalTime();

  storage::begin();

  if(storage::spif::getFreeBytes()<8192){
    storage::spif::deleteOldestFile();
  }

  // Try MQTT connection — if it fails, open config portal so user can fix settings
  bool mqttConnected = network::mqtt::begin();
  while(!mqttConnected){
    Serial.println("MQTT connect failed — opening config portal for reconfiguration");
    mqttconfig::runConfigPortal();
    timer::watchdog::feed();
    mqttConnected = network::mqtt::begin();
    if(!mqttConnected){
      Serial.println("MQTT still not connected, retrying in 5s...");
      delay(5000);
      timer::watchdog::feed();
    }
  }
  Serial.println("MQTT connected");

  // Publish online status with device IP and firmware version
  std::string onlineTopic = config::mqttTopicPrefix + "/online";
  network::mqtt::publish(onlineTopic, std::string(WiFi.localIP().toString().c_str()));
  std::string fwTopic = config::mqttTopicPrefix + "/firmware";
  network::mqtt::publish(fwTopic, FIRMWARE_VERSION);

  BLEDevice::init("");
  global::pBLEScan = BLEDevice::getScan();
  global::pBLEScan->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks());
  global::pBLEScan->setActiveScan(false);
  BLEScanResults foundDevices = global::pBLEScan->start(global::BLEscanTime);

  storage::end();
  timer::wifi::updateWifiRequirements();
  timer::deepsleep::updateBootCount();

  // Reconnect MQTT after BLE scan (BLE often disrupts WiFi/MQTT)
  if(!network::mqtt::isConnected()){
    Serial.println("MQTT reconnecting after BLE scan...");
    network::mqtt::begin();
  }
  if(network::mqtt::bufferEmpty()){
    std::string statusTopic = config::mqttTopicPrefix + "/status";
    network::mqtt::publish(statusTopic, "No sensor active");
    Serial.println("No sensor active");
  }
  network::mqtt::flushBuffer();

  // Subscribe and listen for OpenPortal command (use retained msg on broker)
  network::mqtt::subscribe();
  Serial.println("Listening for MQTT commands (5s)...");
  for(int i = 0; i < 500; i++){  // ~5 seconds
    network::mqtt::loop();
    timer::watchdog::feed();
    delay(10);
  }

  // Check if portal was requested via MQTT
  if(network::mqtt::isPortalRequested()){
    Serial.println("Portal requested via MQTT, staying awake...");

    mqttconfig::runConfigPortalUntil([]() -> bool {
      return !network::mqtt::isPortalRequested();
    });
  }

  if(config::powerSave){
    timer::deepsleep::start();
  }
  // Non-power-save: stay awake, WiFi on, scan periodically
  Serial.println("Continuous mode — WiFi stays on");
}

void loop() {
  if(!config::powerSave){
    unsigned long interval = config::testMode ? 10000 : 900000; // 10s or 15min

    timer::watchdog::feed();

    // Reconnect MQTT before scan so data can be published
    if(!network::mqtt::isConnected()){
      Serial.println("MQTT reconnecting before scan...");
      network::mqtt::begin();
    }

    // BLE scan — clear cache so devices are re-reported
    storage::begin();
    global::pBLEScan->clearResults();
    BLEScanResults foundDevices = global::pBLEScan->start(global::BLEscanTime);
    storage::end();

    // Reconnect MQTT if BLE scan disrupted it
    if(!network::mqtt::isConnected()){
      network::mqtt::begin();
    }
    // Publish sensor data or report no sensors found
    if(network::mqtt::bufferEmpty()){
      std::string statusTopic = config::mqttTopicPrefix + "/status";
      network::mqtt::publish(statusTopic, "No sensor active");
      Serial.println("No sensor active");
    }
    network::mqtt::flushBuffer();

    // Check for portal command
    network::mqtt::subscribe();
    for(int i = 0; i < 100; i++){
      network::mqtt::loop();
      timer::watchdog::feed();
      delay(10);
    }
    if(network::mqtt::isPortalRequested()){
      Serial.println("Portal requested via MQTT...");
      mqttconfig::runConfigPortalUntil([]() -> bool {
        return !network::mqtt::isPortalRequested();
      });
    }

    // Wait for remaining interval
    unsigned long scanMs = (unsigned long)global::BLEscanTime * 1000 + 1000;
    if(interval > scanMs){
      unsigned long waitMs = interval - scanMs;
      unsigned long start = millis();
      while(millis() - start < waitMs){
        if(!network::mqtt::isConnected()){
          network::mqtt::begin();
        }
        network::mqtt::loop();
        timer::watchdog::feed();
        delay(100);
      }
    }
  }
}
