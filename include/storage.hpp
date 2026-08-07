// storage.hpp - Part of ESP32 Ruuvitag Collector
// Hannu Pirila 2019
#ifndef storage_hpp
#define storage_hpp

#include "config.hpp"
#include "Datahandler.hpp"
#include <vector>
#include <sstream>
#include <iomanip>
#include <SPIFFS.h>

namespace storage{
  void begin();
  void end();
  void write(std::string fileName,std::string data);
  
  namespace spif{
    void begin();
    void end();
    void write(std::string fileName,std::string data);
    void deleteOldestFile();
    bool cleanup(uint32_t minimumFreeBytes=8192);
    uint32_t getFreeBytes();
  }
}

#endif