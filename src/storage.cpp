// storage.cpp - Part of ESP32 Ruuvitag Collector
// Hannu Pirila 2019
#include "storage.hpp"
#include "timer.hpp"

namespace storage{
    void begin(){
        spif::begin();
    }
    void end(){
        spif::end();
    }
    void write(std::string fileName,std::string data){
        spif::write(fileName,data);
    }

    namespace{
        std::string _buildFileName(std::string fileName){
            if(fileName.length()>0){
                if(fileName.at(0)!='/'){
                    fileName="/"+fileName;
                }
            }            
            return fileName;            
        }

        void _write(fs::FS &fs,std::string fsType,std::string fileName,std::string data){
            fileName=_buildFileName(fileName);
            std::vector<uint8_t> vData(data.begin(),data.end());
            File file=fs.open(fileName.c_str(),FILE_APPEND);
            if(!file){
                Serial.print(fsType.c_str());
                Serial.print(": Failed to open file ");
                Serial.print(fileName.c_str());
                Serial.println(" for writing");
                return;
            }
            if(!file.write(&vData[0],data.size())){
                Serial.print(fsType.c_str());
                Serial.print(": Failed to append file ");
                Serial.println(fileName.c_str());
            }
            file.close();                    
        }

        void _deleteFile(fs::FS &fs,std::string fileName){
            fileName=_buildFileName(fileName);
            if(fs.exists(fileName.c_str())){
                if(fs.remove(fileName.c_str())){
                    Serial.println("- file deleted");
                } else {
                    Serial.println("- delete failed");
                }
            }
        } 

        std::string _getOldestFile(fs::FS &fs){
            std::stringstream stream;
            int oldestFileTimeStamp=INT_MAX;
            std::string oldestFileName;
            File root = fs.open("/");
            if(!root || !root.isDirectory()){
                return "";
            }

            File file = root.openNextFile();
            while(file){
                stream << file.name();
                int fileTimeStamp=atoi( stream.str().substr(1).c_str() );
                if(fileTimeStamp < oldestFileTimeStamp && stream.str().find('_')!=std::string::npos){
                    oldestFileTimeStamp=fileTimeStamp;
                    oldestFileName=stream.str();
                }
                stream.str(std::string());
                stream.clear();
                file = root.openNextFile();
            } 
            return oldestFileName;
        }

        void _deleteOldestFile(fs::FS &fs){
            _deleteFile(fs,_getOldestFile(fs));
        }
    }

    namespace spif{
        std::string fsType="SPIFFS";
        void begin(){
            SPIFFS.begin(true);
            SPIFFS.end();
            if(!SPIFFS.begin()){
                Serial.println("SPIFFS init failed");
                return;
            }
        }

        void end(){
            SPIFFS.end();
        }

        void write(std::string fileName,std::string data){
            _write(SPIFFS,fsType,fileName,data);
        }

        void deleteOldestFile(){
            _deleteOldestFile(SPIFFS);
        }

        uint32_t getFreeBytes(){
            return SPIFFS.totalBytes()-SPIFFS.usedBytes();
        }
    }
}
