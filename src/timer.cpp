// timer.cpp - Part of ESP32 Ruuvitag Collector
// Hannu Pirila 2019
#include "timer.hpp"

namespace timer{
    namespace{
        RTC_DATA_ATTR int successfulWifiTime=0;
        RTC_DATA_ATTR int wifiNeeded=0;
        hw_timer_t *timer = NULL;
        time_t now;
        // Survives esp_restart(): lets the next boot tell our own watchdog
        // restart apart from a normal software restart.
        const uint32_t WDT_RESTART_MAGIC = 0xDEADB0D1;
        RTC_NOINIT_ATTR uint32_t wdtRestartMarker;
        const int checkTime=1500000000;

        int roundDown(int valueIn){
            return valueIn/config::turnOnWifiEvery*config::turnOnWifiEvery;
        }
    }

    void printLocalTime(){
        struct tm timeInfo;

        time(&now);
        setenv("TZ", config::timeZone.c_str(), 1);
        tzset();

        if(!getLocalTime(&timeInfo)){
            Serial.println("Failed to obtain time");
            return;
        }
        char buffer[80];
        strftime(buffer,sizeof(buffer),"%A, %B %d %Y %H:%M:%S",&timeInfo);
        Serial.println(buffer);
    }

    namespace watchdog{
        void IRAM_ATTR resetModule() {
            ets_printf("Reset due to watchdog\n");  // ISR-safe (Serial is not)
            wdtRestartMarker = WDT_RESTART_MAGIC;
            esp_restart();
        }
        void feed(){
            timerWrite(timer, 0);
        }
        void set(){
            timer = timerBegin(0, 80, true);
            timerAttachInterrupt(timer, &resetModule,true);
            timerAlarmWrite(timer, config::watchdogTimeout * global::uSToSFactor, false);
            timerAlarmEnable(timer);
        }
    }
    namespace deepsleep{
        void updateBootCount(){
            global::bootCount++;
        }

        // Prints why the chip started, e.g. "Woke up from deep sleep (timer), boot number: 74"
        void printBootCount(){
            bool ownWatchdog = (wdtRestartMarker == WDT_RESTART_MAGIC);
            wdtRestartMarker = 0;

            const char* reason;
            switch(esp_reset_reason()){
                case ESP_RST_DEEPSLEEP:
                    switch(esp_sleep_get_wakeup_cause()){
                        case ESP_SLEEP_WAKEUP_TIMER: reason = "Woke up from deep sleep (timer)"; break;
                        case ESP_SLEEP_WAKEUP_EXT0:
                        case ESP_SLEEP_WAKEUP_EXT1:  reason = "Woke up from deep sleep (external)"; break;
                        default:                     reason = "Woke up from deep sleep"; break;
                    }
                    break;
                case ESP_RST_POWERON:  reason = "Power-on reset"; break;
                case ESP_RST_EXT:      reason = "External reset"; break;
                case ESP_RST_SW:       reason = ownWatchdog ? "Watchdog reset (hang detected)"
                                                            : "Software restart"; break;
                case ESP_RST_PANIC:    reason = ownWatchdog ? "Watchdog reset (hang detected)"
                                                            : "Crash (panic) reset"; break;
                case ESP_RST_INT_WDT:
                case ESP_RST_TASK_WDT:
                case ESP_RST_WDT:      reason = "Watchdog reset"; break;
                case ESP_RST_BROWNOUT: reason = "Brownout reset (power supply too weak)"; break;
                default:               reason = "Reset (unknown reason)"; break;
            }
            Serial.println();
            Serial.printf("%s, boot number: %d\n", reason, global::bootCount);
        }

        void start(){
            time(&now);
            int sleepTime=config::deepSleepWakeUpAtSecond - now % config::deepSleepWakeUpAtSecond + config::deepSleepWakeUpOffset;
            esp_sleep_enable_timer_wakeup(sleepTime*global::uSToSFactor);
            Serial.print("Deep sleep for ");
            Serial.print(sleepTime);
            Serial.println(" seconds starting");
            Serial.flush();
            esp_deep_sleep_start();  
        }
    }
    namespace wifi{
        bool wiFiRequested=false;
        bool isWifiNeeded(){
            time(&now);
            if(global::bootCount<2){
                wiFiRequested=true;
                return true;
            }
            if(global::bootCount<5 && now<checkTime){
                wiFiRequested=true;
                return true;
            }
            if(config::turnOnWifiEvery==0){
                wiFiRequested=true;
                return true;
            }
            if(wifiNeeded){
                if(wifiNeeded==global::bootCount-1||global::bootCount % 10){
                    wiFiRequested=true;
                    return true;
                }else{
                    wiFiRequested=false;
                    return false;
                }
            }
            if((roundDown(now)-roundDown(successfulWifiTime))>=config::turnOnWifiEvery){
                wiFiRequested=true;
                return true;
            }
            return false;
        }
        void updateWifiRequirements(){
            if(wiFiRequested){
                if(global::successfulRun){
                    wifiNeeded=0;
                    successfulWifiTime=now;
                }else{
                    wifiNeeded=global::bootCount;
                }
            }else{
                wifiNeeded=0;
            }
        }
    }
}
