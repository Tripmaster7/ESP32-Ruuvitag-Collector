# ESP32-Ruuvitag-Collector

Collect measurement data from [Ruuvitag](https://ruuvi.com/) Bluetooth Low Energy sensors and publish to an MQTT broker. Designed for low-power operation with deep sleep between measurement cycles.

## Features

- **MQTT publishing** — sensor data sent as JSON to configurable topics
- **WiFi captive portal** — zero-config WiFi provisioning via WPA2-protected AP
- **MQTT configuration portal** — configure broker, credentials, topic prefix, and sensor selection from a web UI
- **Sensor selection** — choose which Ruuvitags to monitor via checkboxes (discovered sensors shown automatically)
- **OTA firmware updates** — upload new firmware via the web portal (no USB needed after initial flash)
- **Remote portal access** — open the config portal remotely by publishing to an MQTT topic
- **SPIFFS offline storage** — measurements buffered locally when WiFi is off
- **Deep sleep** — wakes at configurable intervals, scans BLE, publishes, and sleeps again
- **Factory reset** — clear all stored WiFi and MQTT settings from the web portal

## Version

Current firmware version: **2.5.0**

## Contributors

- Hannu Pirila (original author)
- Jens Hoevelman
- GitHub Copilot (AI-assisted development)

## Quick Start

1. Flash the firmware to an ESP32 via USB (see [Compiling](#compiling))
2. Connect to the **RuuviCollector** WiFi AP (password: `ruuvi1234`)
3. Enter your home WiFi credentials in the captive portal
4. The MQTT configuration portal opens automatically — enter your broker details
5. The device begins its deep sleep cycle, scanning Ruuvitags and publishing data every 60 seconds

## WiFi Configuration

### Captive Portal (recommended)

On first boot (or when stored credentials fail), the device starts a WPA2-protected access point:

- **SSID:** `RuuviCollector`
- **Password:** `ruuvi1234`

Connect to the AP and browse to `http://192.168.123.1`. Enter your home WiFi SSID and password. Credentials are stored in NVS (non-volatile storage) and persist across reboots and firmware updates.

### Compile-time defaults (fallback)

Edit `config.cpp` before flashing:

```cpp
wiFiSSD = "MyHomeWifiAP";
wiFiPassword = "sweethome";
```

These are used only if no stored credentials exist or if stored credentials fail.

## MQTT Configuration

### Web Portal

All MQTT settings can be configured at runtime via the built-in web portal. The portal opens automatically on first boot after WiFi connects. It can also be opened remotely (see [Remote Portal Access](#remote-portal-access)).

The portal allows you to configure:

| Setting | Description | Default |
|---------|-------------|---------|
| Server | MQTT broker IP or hostname | `192.168.1.100` |
| Port | MQTT broker port | `1883` |
| Username | MQTT authentication username | `thepublisher` |
| Password | MQTT authentication password | `iamthepublisher` |
| Topic Prefix | Base topic for all published data | `ruuviesp32` |
| Sensors | Checkboxes to select which Ruuvitags to monitor | All |

All settings are stored in NVS and persist across reboots and firmware updates.

### Compile-time defaults

Default MQTT values can also be set in `config.cpp`. These are used only until the first portal save.

## MQTT Topics

| Topic | Direction | Description |
|-------|-----------|-------------|
| `<prefix>/<MAC>/state` | Publish | Sensor data JSON |
| `<prefix>/online` | Publish | Device IP address on connect |
| `<prefix>/status` | Publish | Portal status messages |
| `<prefix>/OpenPortal` | Subscribe | Send `1` (retained) to open portal, `0` to close |

### Example Sensor Payload

```json
{
  "temperature": 12.33,
  "humidity": 53.60,
  "pressure": 993.70,
  "battery": 2.978,
  "accel_x": 0.956,
  "accel_y": -0.368,
  "accel_z": -0.024,
  "epoch": 1774395693,
  "txdbm": 4,
  "move_count": 120,
  "sequence": 34147
}
```

## Remote Portal Access

You can open the configuration portal remotely without physical access to the device:

1. Publish `1` (retained) to `<prefix>/OpenPortal` on your MQTT broker
2. On the next wake cycle, the device detects the message and starts the web portal
3. Browse to the device IP (shown on the `<prefix>/online` topic) to access the portal
4. When done, publish `0` (retained) to `<prefix>/OpenPortal` — or save settings in the portal to close it

## OTA Firmware Updates

The web portal includes a firmware upload section. Upload a `.bin` file and the device flashes the new firmware and restarts automatically. The partition table supports two app slots for safe OTA (rollback on failure).

## Sensor Selection

Discovered Ruuvitags appear as checkboxes in the configuration portal:

- **live** — sensor was seen during the most recent BLE scan
- **saved** — sensor is in the stored whitelist

Check the sensors you want to monitor. If none are checked, all discovered sensors are monitored. Using a whitelist shortens BLE scan time and saves power — the scan stops as soon as all whitelisted devices are found.

## Data Collection Settings

All timing settings are in `config.cpp`:

| Setting | Description | Default |
|---------|-------------|---------|
| `deepSleepWakeUpAtSecond` | Wake interval in seconds | `60` |
| `deepSleepWakeUpOffset` | Offset to shift wake-up time | `0` |
| `turnOnWifiEvery` | WiFi activation interval in seconds (`0` = every wake) | `900` |

### Real-time mode

Set `turnOnWifiEvery=0` to connect WiFi and publish MQTT data on every wake cycle.

### Battery-saving mode

Set `turnOnWifiEvery=900` (or higher) to store data locally in SPIFFS and only connect WiFi periodically. NTP time is updated only when WiFi is active, so longer intervals may cause clock drift.

## Network Configuration

NTP server (use IP address, not hostname):

```cpp
ntpServerIP = "216.239.35.0";
```

Time zone (sign is positive for west of Prime Meridian, negative for east):

```cpp
timeZone = "UTC-3";
```

## Compiling

This project uses [PlatformIO](https://platformio.org/). The easiest setup is VS Code with the PlatformIO extension.

```bash
# Build
pio run

# Upload via USB
pio run --target upload

# Monitor serial output
pio device monitor --baud 115200
```

## Testing Without a Ruuvitag

You can emulate a Ruuvitag with an Android device:

1. Install **nRF Connect** from Google Play Store
2. Go to the **Advertiser** tab
3. Tap **+** (bottom right) and name it `Ruuvitag Emulator`
4. Choose **Add Record** → **Manufacturer Data**
5. Set **16-bit Company Identifier** to `0499`
6. Set **Data** to `0501AE1E23C75000CC008003B870F663D9A1F109FC2FF596`
7. Enable the advertiser — measurements should appear

## Factory Reset

From the web portal, click the **Factory Reset** button to clear all stored WiFi and MQTT settings. The device restarts and enters WiFi captive portal mode.

## Security

- CSRF tokens on all portal form submissions
- Input validation (printable ASCII only, length-limited) on all user-provided fields
- PubSubClient patched for CVE-2023-52120 (buffer overflow)
- MQTT callback accepts only exact topic matches with strict value checking

## License

MIT License — see [LICENSE](LICENSE) for details.
