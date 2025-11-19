# ESP32 WiFi Communications Module

## Quick Setup Guide

### 1. Arduino IDE Setup
1. Open Arduino IDE
2. Open the folder: `ESP32_wifi_coms`
3. Open `ESP32_wifi_coms.ino`

### 2. Configure WiFi Credentials
**IMPORTANT**: Before uploading, change these lines in `ESP32_wifi_coms.ino`:

```cpp
const char* ssid = "YOUR_WIFI_SSID";           // Replace with your WiFi network name
const char* password = "YOUR_WIFI_PASSWORD";    // Replace with your WiFi password
```

### 3. Required Libraries
Install these libraries via Arduino IDE Library Manager:
- **WiFi** (built-in with ESP32)
- **WebServer** (built-in with ESP32)
- **ArduinoJson** (install via Library Manager)
- **SPI** (built-in)

### 4. Wiring Connections

#### SPI Connection (Main ESP32 → WiFi ESP32)
```
Main ESP32          →    WiFi ESP32
GPIO 18 (SCK)       →    GPIO 18 (SCK)
GPIO 23 (MOSI)      →    GPIO 19 (MISO)
GPIO 15 (CS)        →    GPIO 5  (CS)
GND                 →    GND
3.3V                →    3.3V
```

#### GPIO Commands (WiFi ESP32 → Main ESP32)
```
WiFi ESP32          →    Main ESP32
GPIO 4              →    GPIO 4  (Touch Command)
GPIO 2              →    GPIO 2  (Pure Tone Command)
GPIO 15             →    GPIO 15 (Send Packet Command)
```

### 5. Upload Process
1. Select **ESP32 Dev Module** as board
2. Select correct COM port
3. Upload to the second ESP32 (not your main MARV ESP32)
4. Monitor serial output to see IP address

### 6. Access Web Interface
1. Note the IP address from serial monitor
2. Open browser on any device
3. Navigate to: `http://[IP_ADDRESS]`
4. Enjoy real-time MARV monitoring!

### Files in this folder:
- `ESP32_wifi_coms.ino` - Main WiFi communications code
- `spi_protocol.h` - SPI protocol definitions (simplified for WiFi ESP32)
- `README.md` - This setup guide

### Troubleshooting:
- **WiFi won't connect**: Check SSID/password, ensure 2.4GHz network
- **No SPI data**: Verify wiring, check main ESP32 is running
- **Web interface not loading**: Check IP address, try different browser/device

### Performance:
- Web updates: 100ms (10Hz)
- SPI polling: 10ms (100Hz)
- Memory usage: < 200KB
- Very responsive and efficient!