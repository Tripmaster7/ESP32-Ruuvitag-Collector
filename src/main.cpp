// main.cpp - Part of ESP32 Ruuvitag Collector
// Hannu Pirila 2019
#include <Arduino.h>
#include "config.hpp"
#include "AdvertisedDeviceCallbacks.hpp"
#include "storage.hpp"
#include "network.hpp"
#include "timer.hpp"
#include "MqttConfig.hpp"

void setup() {
  Serial.begin(115200);
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

  // Publish online status with device IP
  std::string onlineTopic = config::mqttTopicPrefix + "/online";
  network::mqtt::publish(onlineTopic, std::string(WiFi.localIP().toString().c_str()));

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

  timer::deepsleep::start();
}

void loop() {
}
