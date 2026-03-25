// WifiBootstrap.cpp - Part of ESP32 Ruuvitag Collector
// WiFi provisioning: tries stored/default credentials, falls back to AP with web config
#include "WifiBootstrap.hpp"
#include "config.hpp"
#include "timer.hpp"
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <esp_random.h>
#include <esp_wifi.h>

namespace wifibootstrap {
    namespace {
        const char* NVS_NAMESPACE = "wificreds";
        const char* NVS_KEY_SSID = "ssid";
        const char* NVS_KEY_PASS = "pass";
        const char* AP_SSID = "RuuviCollector";
        const char* AP_PASS = "ruuvi1234";
        const int CONNECT_TIMEOUT_MS = 10000;

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

        const char PORTAL_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html><html><head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Ruuvi Collector WiFi Setup</title>
<style>
body{font-family:sans-serif;background:#1a1a2e;color:#e0e0e0;display:flex;justify-content:center;align-items:center;min-height:100vh;margin:0}
.card{background:#16213e;padding:2em;border-radius:12px;width:90%;max-width:360px;box-shadow:0 4px 20px rgba(0,0,0,.4)}
h2{margin-top:0;color:#0ff}
input{width:100%;padding:10px;margin:8px 0;border:1px solid #444;border-radius:6px;background:#0f3460;color:#fff;box-sizing:border-box}
button{width:100%;padding:12px;margin-top:12px;border:none;border-radius:6px;background:#0ff;color:#1a1a2e;font-weight:bold;font-size:1em;cursor:pointer}
button:hover{background:#00cccc}
.msg{margin-top:10px;color:#0f0;font-size:.9em}
</style></head><body>
<div class="card">
<h2>WiFi Setup</h2>
<form action="/save" method="POST">
<label>SSID</label><input name="ssid" required maxlength="32">
<label>Password</label><input name="pass" type="password" maxlength="64">
<input type="hidden" name="token" value="%TOKEN%">
<button type="submit">Connect</button>
</form>
<div class="msg" id="msg"></div>
</div></body></html>
)rawhtml";

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
<h2>Credentials Saved</h2>
<p>Connecting to WiFi...<br>The access point will shut down shortly.</p>
</div></body></html>
)rawhtml";

        bool tryConnect(const char* ssid, const char* password) {
            if (strlen(ssid) == 0) return false;

            Serial.print("Connecting WiFi SSID: ");
            Serial.println(ssid);

            WiFi.mode(WIFI_STA);
            WiFi.begin(ssid, password);

            unsigned long start = millis();
            while (WiFi.status() != WL_CONNECTED && (millis() - start) < CONNECT_TIMEOUT_MS) {
                delay(500);
                Serial.print(".");
                timer::watchdog::feed();
            }
            Serial.println();

            if (WiFi.status() == WL_CONNECTED) {
                Serial.print("WiFi connected. Local IP is ");
                Serial.println(WiFi.localIP());
                return true;
            }
            Serial.println("WiFi connection failed.");
            return false;
        }

        void saveCredentials(const String& ssid, const String& pass) {
            preferences.begin(NVS_NAMESPACE, false);
            preferences.putString(NVS_KEY_SSID, ssid);
            preferences.putString(NVS_KEY_PASS, pass);
            preferences.end();
            Serial.println("WiFi credentials saved to NVS");
        }

        bool loadCredentials(String& ssid, String& pass) {
            preferences.begin(NVS_NAMESPACE, true);
            ssid = preferences.getString(NVS_KEY_SSID, "");
            pass = preferences.getString(NVS_KEY_PASS, "");
            preferences.end();
            return ssid.length() > 0;
        }

        bool runCaptivePortal() {
            unsigned long bootMs = millis();
            Serial.print("[");
            Serial.print(bootMs / 1000);
            Serial.println("s] Starting WiFi configuration AP...");

            // 1) Start WiFi AP first (server needs the network stack)
            WiFi.mode(WIFI_AP);
            delay(100);

            IPAddress apIP(192, 168, 123, 1);
            IPAddress gateway(192, 168, 123, 1);
            IPAddress subnet(255, 255, 255, 0);
            WiFi.softAPConfig(apIP, gateway, subnet);
            WiFi.softAP(AP_SSID, AP_PASS);
            delay(500);

            Serial.print("[");
            Serial.print(millis() / 1000);
            Serial.print("s] AP started - SSID: ");
            Serial.print(AP_SSID);
            Serial.print(" Pass: ");
            Serial.print(AP_PASS);
            Serial.print(" IP: ");
            Serial.println(WiFi.softAPIP());

            // 2) Now set up and start the web server
            WebServer server(80);
            volatile bool credentialsSaved = false;
            String newSsid, newPass;

            auto servePortal = [&server]() {
                csrfToken = generateToken();
                String html = FPSTR(PORTAL_HTML);
                html.replace("%TOKEN%", csrfToken);
                server.send(200, "text/html", html);
            };

            server.on("/", HTTP_GET, [servePortal]() { servePortal(); });

            server.on("/save", HTTP_POST, [&]() {
                String token = server.arg("token");
                if (csrfToken.length() == 0 || token != csrfToken) {
                    server.send(403, "text/plain", "Invalid request");
                    return;
                }
                csrfToken = "";

                newSsid = server.arg("ssid");
                newPass = server.arg("pass");
                if (newSsid.length() > 0 && newSsid.length() <= 32 &&
                    newPass.length() <= 64 &&
                    isValidInput(newSsid) && isValidInput(newPass)) {
                    saveCredentials(newSsid, newPass);
                    server.send_P(200, "text/html", SAVED_HTML);
                    credentialsSaved = true;
                } else {
                    server.send(400, "text/plain", "Invalid input");
                }
            });

            server.begin();
            Serial.print("[");
            Serial.print(millis() / 1000);
            Serial.println("s] Web server ready - browse to http://192.168.123.1");

            // 3) Wait for credentials, print debug every 5 seconds
            unsigned long lastDebug = millis();
            bool clientSeen = false;
            while (!credentialsSaved) {
                server.handleClient();
                timer::watchdog::feed();

                if (millis() - lastDebug >= 5000) {
                    int clients = WiFi.softAPgetStationNum();
                    Serial.print("[");
                    Serial.print(millis() / 1000);
                    Serial.print("s] Waiting... clients connected: ");
                    Serial.println(clients);
                    if (clients > 0 && !clientSeen) {
                        clientSeen = true;
                        Serial.print("[");
                        Serial.print(millis() / 1000);
                        Serial.println("s] First client connected!");
                    }
                    lastDebug = millis();
                }
                delay(10);
            }

            server.stop();
            WiFi.softAPdisconnect(true);

            if (credentialsSaved) {
                Serial.println("AP shut down. Trying new credentials...");
                delay(500);
                return tryConnect(newSsid.c_str(), newPass.c_str());
            }

            Serial.println("AP timed out. No credentials received.");
            return false;
        }
    }

    bool begin() {
        Serial.print("[");
        Serial.print(millis() / 1000);
        Serial.println("s] WifiBootstrap::begin()");

        // 1) Try stored NVS credentials (skip if empty)
        String storedSsid, storedPass;
        if (loadCredentials(storedSsid, storedPass) && storedSsid.length() > 0) {
            Serial.print("Found stored WiFi credentials for: ");
            Serial.println(storedSsid);
            if (tryConnect(storedSsid.c_str(), storedPass.c_str())) {
                return true;
            }
            Serial.println("Stored credentials failed, clearing stale entry.");
            clearCredentials();
        }

        // 2) Try compile-time defaults (skip if still placeholder)
        if (config::wiFiSSD.length() > 0 && config::wiFiSSD != "MyHomeWifiAP") {
            Serial.println("Trying compile-time WiFi defaults...");
            if (tryConnect(config::wiFiSSD.c_str(), config::wiFiPassword.c_str())) {
                return true;
            }
        }

        // 3) Fall back to AP setup — runs until user provides credentials
        return runCaptivePortal();
    }

    void clearCredentials() {
        preferences.begin(NVS_NAMESPACE, false);
        preferences.clear();
        preferences.end();
        Serial.println("WiFi credentials cleared from NVS");
    }

    bool hasStoredCredentials() {
        String ssid, pass;
        return loadCredentials(ssid, pass);
    }
}
