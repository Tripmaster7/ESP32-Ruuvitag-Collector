// network.cpp - Part of ESP32 Ruuvitag Collector
// Hannu Pirila 2019
#include "network.hpp"

namespace network {
    namespace wifi {
        bool begin(){
            return wifibootstrap::begin();
        }
    }
    namespace ntp {
        void update(){
            if(WiFi.isConnected()){
                time_t timeNow;
                Serial.print("NTP update starting... ");
                
                configTime(0,0,config::ntpServerIP.c_str());
                delay(500);
                Serial.println("NTP update completed.");
                time(&timeNow);
                if(timeNow<1500000000){
                    global::successfulRun=false;
                }
            }
        }
    }
    namespace mqtt {
        WiFiClient espClient;
        PubSubClient client(espClient);
        volatile bool portalFlag = false;

        void mqttCallback(char* topic, byte* payload, unsigned int length) {
            std::string t(topic);
            std::string openPortalTopic = config::mqttTopicPrefix + "/OpenPortal";
            if (t == openPortalTopic) {
                std::string val((char*)payload, length);
                if (val == "1") {
                    Serial.println("MQTT: OpenPortal=1 received, starting config portal");
                    portalFlag = true;
                    std::string statusTopic = config::mqttTopicPrefix + "/status";
                    client.publish(statusTopic.c_str(), "Portal Open");
                } else if (val == "0") {
                    Serial.println("MQTT: OpenPortal=0 received, closing config portal");
                    portalFlag = false;
                }
            }
        }

        bool begin(){
            if(WiFi.isConnected() && config::mqttServerIP!=std::string()){
                std::string macAddress=WiFi.macAddress().c_str();
                std::string id;
                std::stringstream stream;
                stream << "ESP32";
                stream << macAddress.substr(0,2)  << macAddress.substr(3,2);
                stream << macAddress.substr(6,2)  << macAddress.substr(9,2);
                stream << macAddress.substr(12,2) << macAddress.substr(15,2);
                client.setServer(config::mqttServerIP.c_str(),config::mqttServerPort);
                client.setCallback(mqttCallback);
                client.connect (stream.str().c_str(),config::mqttServerUsername.c_str(),config::mqttServerPassword.c_str());
                return client.connected();
            }
            return false;
        }

        void subscribe(){
            if(client.connected()){
                std::string openPortalTopic = config::mqttTopicPrefix + "/OpenPortal";
                client.subscribe(openPortalTopic.c_str());
                Serial.print("Subscribed to: ");
                Serial.println(openPortalTopic.c_str());
            }
        }

        void loop(){
            if(client.connected()){
                client.loop();
            }
        }

        bool isConnected(){
            return client.connected();
        }

        bool isPortalRequested(){
            return portalFlag;
        }

        void clearPortalFlag(){
            portalFlag = false;
            // Clear the retained message on the broker so next boot won't reopen the portal
            std::string openPortalTopic = config::mqttTopicPrefix + "/OpenPortal";
            if(client.connected()){
                client.publish(openPortalTopic.c_str(), "0", true);
                Serial.println("Cleared retained OpenPortal message on broker");
            }
        }

        void publish(std::string topic,std::string payload){
            if(WiFi.isConnected() && config::mqttServerIP!=std::string()){
                client.publish(topic.c_str(),payload.c_str());
            }
        }
    }
}
