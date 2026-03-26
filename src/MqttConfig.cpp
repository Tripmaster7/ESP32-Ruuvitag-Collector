// MqttConfig.cpp - Part of ESP32 Ruuvitag Collector
// Web portal for MQTT broker settings and Ruuvi sensor selection
#include "MqttConfig.hpp"
#include "config.hpp"
#include "timer.hpp"
#include "network.hpp"
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <esp_random.h>
#include <Update.h>
#include <BLEDevice.h>
#include <esp_ota_ops.h>
#include <set>

namespace mqttconfig {
    namespace {
        const char* NVS_NAMESPACE = "mqttcfg";
        Preferences preferences;

        String csrfToken;

        String generateToken() {
            uint32_t r = esp_random();
            char buf[9];
            snprintf(buf, sizeof(buf), "%08x", r);
            return String(buf);
        }

        bool isValidInput(const String& s) {
            for (unsigned int i = 0; i < s.length(); i++) {
                char c = s.charAt(i);
                if (c < 32 || c > 126) return false;
            }
            return true;
        }

        void saveSettings(const String& server, int port,
                          const String& user, const String& pass,
                          const String& topic,
                          const String& macList,
                          bool powerSave, bool testMode) {
            preferences.begin(NVS_NAMESPACE, false);
            preferences.putString("server", server);
            preferences.putInt("port", port);
            preferences.putString("user", user);
            preferences.putString("pass", pass);
            preferences.putString("topic", topic);
            preferences.putString("macs", macList);
            preferences.putBool("pwrsave", powerSave);
            preferences.putBool("testmode", testMode);
            preferences.end();
            Serial.println("MQTT settings saved to NVS");
        }

        // Build the HTML page with current config values pre-filled
        String buildPage(const String& token) {
            // Merge whitelisted + discovered into one set for display
            std::set<std::string> allSensors;
            for (auto& m : config::macWhiteList) allSensors.insert(m);
            for (auto& m : config::discoveredSensors) allSensors.insert(m);

            String html = F("<!DOCTYPE html><html><head>"
                "<meta name='viewport' content='width=device-width,initial-scale=1'>"
                "<title>Ruuvi MQTT Config</title>"
                "<style>"
                "body{font-family:sans-serif;background:#1a1a2e;color:#e0e0e0;display:flex;justify-content:center;margin:0;padding:20px}"
                ".card{background:#16213e;padding:2em;border-radius:12px;width:90%;max-width:480px;box-shadow:0 4px 20px rgba(0,0,0,.4)}"
                "h2{margin-top:0;color:#0ff}"
                "h3{color:#0ff;margin-bottom:4px}"
                "label{display:block;margin-top:12px;font-size:.9em;color:#aaa}"
                "input[type=text],input[type=number],input[type=password]{width:100%;padding:10px;margin:4px 0;border:1px solid #444;border-radius:6px;background:#0f3460;color:#fff;box-sizing:border-box}"
                ".btn{width:100%;padding:12px;margin-top:16px;border:none;border-radius:6px;background:#0ff;color:#1a1a2e;font-weight:bold;font-size:1em;cursor:pointer}"
                ".btn:hover{background:#00cccc}"
                ".btn-danger{background:#e74c3c;color:#fff;margin-top:24px}"
                ".btn-danger:hover{background:#c0392b}"
                ".row{display:flex;gap:10px}"
                ".row>div{flex:1}"
                ".hint{font-size:.75em;color:#666;margin-top:2px}"
                ".sensor{display:flex;align-items:center;padding:8px 10px;margin:4px 0;background:#0f3460;border-radius:6px;gap:10px}"
                ".sensor input[type=checkbox]{width:18px;height:18px;accent-color:#0ff;flex-shrink:0}"
                ".sensor .mac{font-family:monospace;font-size:.95em;color:#fff}"
                ".sensor .tag{font-size:.7em;padding:2px 6px;border-radius:4px;margin-left:auto}"
                ".tag-live{background:#27ae60;color:#fff}"
                ".tag-saved{background:#2980b9;color:#fff}"
                ".no-sensors{color:#888;font-style:italic;padding:10px 0}"
                "hr{border:none;border-top:1px solid #333;margin:20px 0}"
                "</style></head><body>"
                "<div class='card'>"
                "<h2>MQTT &amp; Sensor Config</h2>"
                "<p style='text-align:right;color:#666;font-size:.8em;margin:-12px 0 8px'>Firmware v" FIRMWARE_VERSION "</p>"
                "<form action='/save' method='POST'>");

            html += F("<label>MQTT Server</label>"
                "<input type='text' name='server' value='");
            html += String(config::mqttServerIP.c_str());
            html += F("' required maxlength='64'>");

            html += F("<div class='row'>"
                "<div><label>Port</label>"
                "<input name='port' type='number' value='");
            html += String(config::mqttServerPort);
            html += F("' min='1' max='65535'></div>"
                "<div><label>Topic Prefix</label>"
                "<input type='text' name='topic' value='");
            html += String(config::mqttTopicPrefix.c_str());
            html += F("' maxlength='64'></div></div>");

            html += F("<div class='row'>"
                "<div><label>Username</label>"
                "<input type='text' name='user' value='");
            html += String(config::mqttServerUsername.c_str());
            html += F("' maxlength='64'></div>"
                "<div><label>Password</label>"
                "<input name='pass' type='password' value='");
            html += String(config::mqttServerPassword.c_str());
            html += F("' maxlength='64'></div></div>");

            // Mode checkboxes
            html += F("<hr><h3>Device Mode</h3>"
                "<div class='sensor'><input type='checkbox' name='pwrsave' value='1'");
            if (config::powerSave) html += F(" checked");
            html += F("><span class='mac'>Power Save</span>"
                "<span class='tag tag-saved' style='background:#8e44ad'>deep sleep between scans</span></div>"
                "<div class='sensor'><input type='checkbox' name='testmode' value='1'");
            if (config::testMode) html += F(" checked");
            html += F("><span class='mac'>Test Mode</span>"
                "<span class='tag tag-saved' style='background:#e67e22'>scan every 10s</span></div>"
                "<div class='hint'>Power Save off = WiFi stays on. "
                "Test Mode = 10s scan interval (unchecked = ~15 min).</div>");

            // Sensor checklist section
            html += F("<hr><h3>Ruuvi Sensors</h3>");

            if (allSensors.empty()) {
                html += F("<div class='no-sensors'>No sensors discovered yet. "
                    "Sensors appear here after a BLE scan.</div>");
            } else {
                html += F("<div class='hint' style='margin-bottom:8px'>"
                    "Check the sensors you want to monitor. Unchecked sensors are ignored. "
                    "If none are checked, all sensors are monitored.</div>");

                // Build a set from whitelist for quick lookup
                std::set<std::string> whiteSet(config::macWhiteList.begin(), config::macWhiteList.end());
                bool whitelistEmpty = config::macWhiteList.empty();

                for (auto& mac : allSensors) {
                    bool checked = whitelistEmpty || (whiteSet.find(mac) != whiteSet.end());
                    bool isLive = config::discoveredSensors.find(mac) != config::discoveredSensors.end();
                    bool isSaved = whiteSet.find(mac) != whiteSet.end();

                    html += F("<div class='sensor'><input type='checkbox' name='mac' value='");
                    html += String(mac.c_str());
                    html += F("'");
                    if (checked) html += F(" checked");
                    html += F("><span class='mac'>");
                    html += String(mac.c_str());
                    html += F("</span>");
                    if (isLive) html += F("<span class='tag tag-live'>live</span>");
                    if (isSaved) html += F("<span class='tag tag-saved'>saved</span>");
                    html += F("</div>");
                }
            }

            html += F("<input type='hidden' name='token' value='");
            html += token;
            html += F("'><button type='submit' class='btn'>Save &amp; Apply</button></form>");

            // Reset button — separate form
            html += F("<form action='/reset' method='POST'>"
                "<input type='hidden' name='token' value='");
            html += token;
            html += F("'><button type='submit' class='btn btn-danger' "
                "onclick=\"return confirm('Reset ALL settings (WiFi + MQTT)? Device will restart.')\">"
                "Factory Reset</button></form>");

            // Firmware upload section
            html += F("<hr><h3>Update Firmware</h3>"
                "<form action='/update' method='POST' enctype='multipart/form-data'>"
                "<input type='file' name='firmware' accept='.bin' required "
                "style='width:100%;padding:8px;margin:4px 0;background:#0f3460;color:#fff;"
                "border:1px solid #444;border-radius:6px;box-sizing:border-box'>"
                "<button type='submit' class='btn' style='background:#e67e22;color:#fff' "
                "onclick=\"return confirm('Upload new firmware? Device will restart after update.')\">"
                "Upload Firmware</button></form>");

            html += F("</div></body></html>");
            return html;
        }

        const char SAVED_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html><html><head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Saved</title>
<style>
body{font-family:sans-serif;background:#1a1a2e;color:#e0e0e0;display:flex;justify-content:center;align-items:center;min-height:100vh;margin:0}
.card{background:#16213e;padding:2em;border-radius:12px;width:90%;max-width:360px;text-align:center}
h2{color:#0ff}
</style></head><body>
<div class="card">
<h2>Settings Saved</h2>
<p>MQTT configuration updated.<br>New settings will be used on the next data cycle.</p>
</div></body></html>
)rawhtml";

        const char UPDATE_OK_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html><html><head><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Update OK</title>
<style>body{font-family:sans-serif;background:#1a1a2e;color:#e0e0e0;display:flex;justify-content:center;align-items:center;min-height:100vh;margin:0}.card{background:#16213e;padding:2em;border-radius:12px;width:90%;max-width:360px;text-align:center}h2{color:#27ae60}</style></head><body>
<div class="card"><h2>Update Successful</h2>
<p>Firmware updated.<br>Device will restart in 3 seconds...</p>
</div></body></html>
)rawhtml";

        const char UPDATE_FAIL_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html><html><head><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Update Failed</title>
<style>body{font-family:sans-serif;background:#1a1a2e;color:#e0e0e0;display:flex;justify-content:center;align-items:center;min-height:100vh;margin:0}.card{background:#16213e;padding:2em;border-radius:12px;width:90%;max-width:360px;text-align:center}h2{color:#e74c3c}</style></head><body>
<div class="card"><h2>Update Failed</h2>
<p>Firmware update failed.<br>Please try again.</p>
</div></body></html>
)rawhtml";

        WebServer* otaServer = nullptr;
        volatile bool otaUpdateDone = false;

        void handleOtaResponse() {
            if (!otaServer) return;
            otaServer->sendHeader("Connection", "close");
            std::string statusTopic = config::mqttTopicPrefix + "/status";
            if (Update.hasError()) {
                otaServer->send_P(200, "text/html", UPDATE_FAIL_HTML);
                char buf[128];
                snprintf(buf, sizeof(buf), "OTA Failed: err=%u", Update.getError());
                network::mqtt::publish(statusTopic, std::string(buf));
                Serial.println(buf);
            } else {
                otaServer->send_P(200, "text/html", UPDATE_OK_HTML);
                const esp_partition_t* boot = esp_ota_get_boot_partition();
                const esp_partition_t* run = esp_ota_get_running_partition();
                char buf[192];
                snprintf(buf, sizeof(buf), "OTA OK boot:%s run:%s - Restarting",
                    boot ? boot->label : "?", run ? run->label : "?");
                network::mqtt::publish(statusTopic, std::string(buf));
                Serial.println(buf);
                otaUpdateDone = true;
            }
        }

        void handleOtaUpload() {
            if (!otaServer) return;
            HTTPUpload& upload = otaServer->upload();
            std::string statusTopic = config::mqttTopicPrefix + "/status";
            if (upload.status == UPLOAD_FILE_START) {
                // Free BLE memory (~65KB) so OTA has enough heap
                BLEDevice::deinit(true);
                Serial.printf("Firmware upload: %s (free heap: %u)\n",
                              upload.filename.c_str(), ESP.getFreeHeap());
                char buf[128];
                snprintf(buf, sizeof(buf), "OTA start: %s heap=%u",
                         upload.filename.c_str(), ESP.getFreeHeap());
                network::mqtt::publish(statusTopic, std::string(buf));
                if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
                    Update.printError(Serial);
                    snprintf(buf, sizeof(buf), "OTA begin FAILED err=%u", Update.getError());
                    network::mqtt::publish(statusTopic, std::string(buf));
                }
            } else if (upload.status == UPLOAD_FILE_WRITE) {
                timer::watchdog::feed();
                if (!Update.isRunning()) return;  // abort if begin() failed
                if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
                    Update.printError(Serial);
                }
            } else if (upload.status == UPLOAD_FILE_END) {
                if (!Update.isRunning()) {
                    Serial.println("OTA upload aborted — Update never started");
                    network::mqtt::publish(statusTopic, "OTA aborted - never started");
                    return;
                }
                if (Update.end(true)) {
                    char buf[128];
                    snprintf(buf, sizeof(buf), "OTA written: %u bytes", upload.totalSize);
                    Serial.println(buf);
                    network::mqtt::publish(statusTopic, std::string(buf));
                } else {
                    Update.printError(Serial);
                    char buf[128];
                    snprintf(buf, sizeof(buf), "OTA end FAILED err=%u", Update.getError());
                    network::mqtt::publish(statusTopic, std::string(buf));
                }
            }
        }

        // Parse newline-separated MAC list into vector
        std::vector<std::string> parseMacList(const String& input) {
            std::vector<std::string> result;
            int start = 0;
            for (int i = 0; i <= (int)input.length(); i++) {
                if (i == (int)input.length() || input.charAt(i) == '\n' || input.charAt(i) == '\r') {
                    if (i > start) {
                        String mac = input.substring(start, i);
                        mac.trim();
                        mac.toUpperCase();
                        if (mac.length() == 12) {
                            bool valid = true;
                            for (unsigned int j = 0; j < mac.length(); j++) {
                                char c = mac.charAt(j);
                                if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F'))) {
                                    valid = false;
                                    break;
                                }
                            }
                            if (valid) {
                                result.push_back(std::string(mac.c_str()));
                            }
                        }
                    }
                    start = i + 1;
                }
            }
            return result;
        }

        void applyToConfig() {
            preferences.begin(NVS_NAMESPACE, true);
            String server = preferences.getString("server", "");
            if (server.length() > 0) {
                config::mqttServerIP = std::string(server.c_str());
                config::mqttServerPort = preferences.getInt("port", 1883);
                config::mqttServerUsername = std::string(preferences.getString("user", "").c_str());
                config::mqttServerPassword = std::string(preferences.getString("pass", "").c_str());
                config::mqttTopicPrefix = std::string(preferences.getString("topic", "ruuviesp32").c_str());

                String macStr = preferences.getString("macs", "");
                config::macWhiteList = parseMacList(macStr);
            }
            config::powerSave = preferences.getBool("pwrsave", true);
            config::testMode = preferences.getBool("testmode", false);
            preferences.end();
        }

        // Collect checked MAC checkboxes from form into newline-separated string
        String collectCheckedMacs(WebServer& srv) {
            String result;
            for (int i = 0; i < srv.args(); i++) {
                if (srv.argName(i) == "mac") {
                    String mac = srv.arg(i);
                    mac.trim();
                    mac.toUpperCase();
                    if (mac.length() == 12) {
                        if (result.length() > 0) result += "\n";
                        result += mac;
                    }
                }
            }
            return result;
        }

        // Common save handler logic — validates, saves, applies
        bool handleSave(WebServer& srv) {
            String token = srv.arg("token");
            if (csrfToken.length() == 0 || token != csrfToken) {
                srv.send(403, "text/plain", "Invalid request");
                return false;
            }
            csrfToken = "";

            String server = srv.arg("server");
            String portStr = srv.arg("port");
            String user = srv.arg("user");
            String pass = srv.arg("pass");
            String topic = srv.arg("topic");
            String macs = collectCheckedMacs(srv);

            if (server.length() == 0 || server.length() > 64 || !isValidInput(server)) {
                srv.send(400, "text/plain", "Invalid server");
                return false;
            }
            if (!isValidInput(user) || !isValidInput(pass) ||
                !isValidInput(topic)) {
                srv.send(400, "text/plain", "Invalid input");
                return false;
            }

            int port = portStr.toInt();
            if (port < 1 || port > 65535) port = 1883;

            bool powerSave = srv.hasArg("pwrsave");
            bool testMode = srv.hasArg("testmode");

            saveSettings(server, port, user, pass, topic, macs, powerSave, testMode);
            applyToConfig();
            srv.send_P(200, "text/html", SAVED_HTML);
            Serial.println("MQTT config saved via portal");
            return true;
        }

        // Common reset handler logic
        bool handleReset(WebServer& srv) {
            String token = srv.arg("token");
            if (csrfToken.length() == 0 || token != csrfToken) {
                srv.send(403, "text/plain", "Invalid request");
                return false;
            }
            csrfToken = "";

            // Clear MQTT config
            preferences.begin(NVS_NAMESPACE, false);
            preferences.clear();
            preferences.end();

            // Clear WiFi config
            Preferences wifiPrefs;
            wifiPrefs.begin("wificreds", false);
            wifiPrefs.clear();
            wifiPrefs.end();

            srv.send(200, "text/html",
                "<html><head><meta name='viewport' content='width=device-width,initial-scale=1'>"
                "<style>body{font-family:sans-serif;background:#1a1a2e;color:#e0e0e0;display:flex;"
                "justify-content:center;align-items:center;min-height:100vh;margin:0}"
                ".card{background:#16213e;padding:2em;border-radius:12px;width:90%;max-width:360px;text-align:center}"
                "h2{color:#e74c3c}</style></head><body>"
                "<div class='card'><h2>Factory Reset</h2>"
                "<p>All settings cleared.<br>Device will restart in 3 seconds...</p>"
                "</div></body></html>");
            Serial.println("Factory reset — all NVS cleared, restarting...");
            return true;
        }
    }

    void loadSettings() {
        applyToConfig();
        if (config::mqttServerIP.length() > 0 && config::mqttServerIP != "192.168.1.100") {
            Serial.print("MQTT config loaded from NVS: ");
            Serial.println(config::mqttServerIP.c_str());
        }
    }

    bool hasStoredSettings() {
        preferences.begin(NVS_NAMESPACE, true);
        String server = preferences.getString("server", "");
        preferences.end();
        return server.length() > 0;
    }

    void runConfigPortal() {
        if (!WiFi.isConnected()) return;

        Serial.print("MQTT config portal at http://");
        Serial.println(WiFi.localIP());

        WebServer server(80);
        volatile bool settingsSaved = false;
        volatile bool resetRequested = false;
        const unsigned long PORTAL_TIMEOUT_MS = 120000; // 2 minutes

        server.on("/", HTTP_GET, [&server]() {
            csrfToken = generateToken();
            String html = buildPage(csrfToken);
            server.send(200, "text/html", html);
        });

        server.on("/save", HTTP_POST, [&server, &settingsSaved]() {
            if (handleSave(server)) settingsSaved = true;
        });

        server.on("/reset", HTTP_POST, [&server, &resetRequested]() {
            if (handleReset(server)) resetRequested = true;
        });

        otaServer = &server;
        otaUpdateDone = false;
        server.on("/update", HTTP_POST, handleOtaResponse, handleOtaUpload);

        server.begin();

        unsigned long start = millis();
        while (!settingsSaved && !resetRequested && !otaUpdateDone && (millis() - start) < PORTAL_TIMEOUT_MS) {
            server.handleClient();
            timer::watchdog::feed();
            delay(10);
        }

        server.stop();

        if (resetRequested || otaUpdateDone) {
            network::mqtt::clearPortalFlag();
            delay(3000);
            esp_restart();
        } else if (settingsSaved) {
            // settings already applied
        } else {
            Serial.println("MQTT config portal timed out");
        }
    }

    void runConfigPortalUntil(bool (*stopCondition)()) {
        if (!WiFi.isConnected()) return;

        Serial.print("MQTT config portal at http://");
        Serial.println(WiFi.localIP());

        WebServer server(80);
        volatile bool resetRequested = false;

        server.on("/", HTTP_GET, [&server]() {
            csrfToken = generateToken();
            String html = buildPage(csrfToken);
            server.send(200, "text/html", html);
        });

        volatile bool settingsSaved = false;

        server.on("/save", HTTP_POST, [&server, &settingsSaved]() {
            if (handleSave(server)) settingsSaved = true;
        });

        server.on("/reset", HTTP_POST, [&server, &resetRequested]() {
            if (handleReset(server)) resetRequested = true;
        });

        otaServer = &server;
        otaUpdateDone = false;
        server.on("/update", HTTP_POST, handleOtaResponse, handleOtaUpload);

        server.begin();

        while (!resetRequested && !otaUpdateDone && !settingsSaved && !stopCondition()) {
            server.handleClient();
            network::mqtt::loop();
            timer::watchdog::feed();
            delay(10);
        }

        server.stop();

        if (resetRequested || otaUpdateDone) {
            network::mqtt::clearPortalFlag();
            delay(3000);
            esp_restart();
        }
        if (settingsSaved) {
            Serial.println("Settings saved, closing portal");
            std::string statusTopic = config::mqttTopicPrefix + "/status";
            network::mqtt::publish(statusTopic, "Settings Saved");
            network::mqtt::clearPortalFlag();
        }
        Serial.println("Config portal closed");
    }

    void resetAllConfig() {
        Preferences p;
        p.begin("mqttcfg", false);
        p.clear();
        p.end();
        p.begin("wificreds", false);
        p.clear();
        p.end();
        Serial.println("All NVS config cleared");
    }
}
