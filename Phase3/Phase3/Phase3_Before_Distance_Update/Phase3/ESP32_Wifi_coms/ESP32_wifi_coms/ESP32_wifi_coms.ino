/*
 * ESP32_wifi_coms.ino
 * WiFi Communication ESP32 for MARV System
 *
 * WIRING CONNECTIONS:
 * ==================
 * SPI Connection to Main ESP32:
 * - GPIO 18 (SCK)  -> Main ESP32 GPIO 18 (SCK)
 * - GPIO 23 (MOSI) -> Main ESP32 GPIO 23 (MOSI)
 * - GPIO 19 (MISO) -> Main ESP32 GPIO 19 (MISO)
 * - GPIO 5  (CS)   -> Main ESP32 GPIO 5 (CS)
 * - GND            -> Main ESP32 GND
 *
 * GPIO Commands to Main ESP32:
 * - GPIO 4  -> Main ESP32 GPIO 4  (Touch Command)
 * - GPIO 2  -> Main ESP32 GPIO 2  (Pure Tone Command)
 * - GPIO 15 -> Main ESP32 GPIO 15 (Send Packet Command)
 *
 * Configure WiFi credentials below before upload
 */

#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <driver/spi_slave.h>
#include "spi_protocol.h"

// ==================== FUNCTION DECLARATIONS ====================
String getTimeString();
bool connectToWiFi();

// ==================== WIFI CONFIGURATION ====================
// ESP32 creates its own WiFi hotspot - no existing network needed!
const char* ap_ssid = "ERD-Byron";           // ESP32 hotspot name
const char* ap_password = "NoEkweetnie";        // Password to connect to hotspot

// ==================== PIN DEFINITIONS ====================
// SPI Pins (Hardware SPI - WiFi ESP32 as Slave)
#define SPI_SCK   18  // Clock from Main ESP32
#define SPI_MISO  19  // Data TO Main ESP32 (not used currently)
#define SPI_MOSI  23  // Data FROM Main ESP32
#define SPI_CS    5   // Chip Select from Main ESP32

// GPIO Command Pins (outputs to main ESP32)
#define CMD_TOUCH_OUT  4
#define CMD_TONE_OUT   2
#define CMD_SEND_OUT   15

// Status LED (optional) - using built-in LED
#define STATUS_LED 2  // Built-in LED, but conflicts with CMD_TONE_OUT - DISABLED

// ==================== GLOBAL VARIABLES ====================
WebServer server(80);

// SPI Slave buffers (must be DMA-capable memory)
DMA_ATTR uint8_t spi_slave_tx_buf[sizeof(SPIPacket)];
DMA_ATTR uint8_t spi_slave_rx_buf[sizeof(SPIPacket)];

// System Status Structure with "sticky" display (holds values for visibility)
struct {
    String systemState = "IDLE";
    String navconState = "UNKNOWN";
    String lastUpdate = "Never";
    uint32_t packetsReceived = 0;
    uint32_t packetsCorrupted = 0;
    bool connectionStatus = false;
    bool endOfMazeDetected = false;
    unsigned long endOfMazeTime = 0;

    // Sensor Data (holds for visual clarity)
    String sensor1Color = "UNKNOWN";
    String sensor2Color = "UNKNOWN";
    String sensor3Color = "UNKNOWN";
    unsigned long lastSensorUpdate = 0;
    bool sensorDataHeld = false;

    // Movement Data (holds for visual clarity)
    uint8_t wheelSpeedR = 0;
    uint8_t wheelSpeedL = 0;
    uint8_t wheelSetpoint = 0;
    uint16_t distance_mm = 0;
    unsigned long lastMovementUpdate = 0;
    bool movementDataHeld = false;

    // Navigation Data
    String lineColor = "NONE";
    float lineAngle = 0.0;
    String lineType = "NONE";
    uint16_t incidenceAngle = 0;
    unsigned long lastIncidenceUpdate = 0;
    bool incidenceDataHeld = false;
    uint16_t rotationAngle = 0;
    String rotationDirection = "NONE";
    unsigned long lastRotationUpdate = 0;
    bool rotationDataHeld = false;

    // Debug Messages
    String lastDebugMessage = "None";
    String lastDebugSeverity = "INFO";

    // Performance
    unsigned long lastPacketTime = 0;
    float packetsPerSecond = 0.0;
} systemData;

// SPI Communication Class
class WiFiSPIReceiver {
private:
    SPIPacket rx_packet;
    uint32_t last_successful_read = 0;

    uint8_t calculateChecksum(const uint8_t* data, size_t length) {
        uint8_t checksum = 0;
        for (size_t i = 0; i < length; i++) {
            checksum ^= data[i];
        }
        return checksum;
    }

    bool verifyPacket() {
        if (rx_packet.header.sync1 != 0xAA || rx_packet.header.sync2 != 0x55) {
            return false;
        }

        uint8_t calc_header = calculateChecksum((uint8_t*)&rx_packet.header, sizeof(SPIPacketHeader) - 1);
        if (calc_header != rx_packet.header.checksum_header) {
            return false;
        }

        uint8_t calc_payload = calculateChecksum(rx_packet.payload, rx_packet.header.data_length);
        if (calc_payload != rx_packet.checksum_payload) {
            return false;
        }

        return true;
    }

    void processPacket() {
        if (!verifyPacket()) {
            systemData.packetsCorrupted++;
            systemData.connectionStatus = false;  // Corrupted = disconnected
            return;
        }

        systemData.packetsReceived++;
        systemData.lastPacketTime = millis();
        systemData.lastUpdate = getTimeString();
        systemData.connectionStatus = true;  // Mark as connected on valid packet
        last_successful_read = millis();

        // Calculate packets per second
        static uint32_t lastPacketCount = 0;
        static unsigned long lastCalcTime = 0;
        if (millis() - lastCalcTime > 1000) {
            systemData.packetsPerSecond = (systemData.packetsReceived - lastPacketCount);
            lastPacketCount = systemData.packetsReceived;
            lastCalcTime = millis();
        }

        // DEBUG: Log packet type received (reduced frequency)
        static unsigned long lastPacketLog = 0;
        static uint8_t lastPacketType = 0xFF;
        if (rx_packet.header.packet_type != lastPacketType || millis() - lastPacketLog > 10000) {
            Serial.printf("[PKT-RX] Type=0x%02X (%s) Seq=%d Len=%d\n",
                         rx_packet.header.packet_type,
                         getPacketTypeName(rx_packet.header.packet_type),
                         rx_packet.header.sequence,
                         rx_packet.header.data_length);
            lastPacketType = rx_packet.header.packet_type;
            lastPacketLog = millis();
        }

        switch (rx_packet.header.packet_type) {
            case PKT_SYSTEM_STATE:
                processSystemState();
                break;
            case PKT_SENSOR_COLORS:
                processSensorColors();
                break;
            case PKT_INCIDENCE_ANGLE:
                processIncidenceAngle();
                break;
            case PKT_WHEEL_SPEEDS:
                processWheelSpeeds();
                break;
            case PKT_DISTANCE:
                processDistance();
                break;
            case PKT_ROTATION_ANGLE:
                processRotationAngle();
                break;
            case PKT_END_OF_MAZE:
                processEndOfMaze();
                break;
            case PKT_NAVCON_STATE:
                processNavconState();
                break;
            case PKT_LINE_DETECTION:
                processLineDetection();
                break;
            case PKT_DEBUG_MESSAGE:
                processDebugMessage();
                break;
            case PKT_HEARTBEAT:
                systemData.connectionStatus = true;
                last_successful_read = millis();
                break;
            default:
                Serial.printf("[PKT-RX] UNKNOWN packet type: 0x%02X\n", rx_packet.header.packet_type);
                break;
        }
    }

    void processSystemState() {
        SystemStatePayload* p = (SystemStatePayload*)rx_packet.payload;
        switch(p->system_state) {
            case 0: systemData.systemState = "IDLE"; break;
            case 1: systemData.systemState = "CALIBRATION"; break;
            case 2: systemData.systemState = "MAZE"; break;
            case 3: systemData.systemState = "SOS"; break;
            default: systemData.systemState = "UNKNOWN"; break;
        }
    }

    void processSensorColors() {
        SensorColorsPayload* p = (SensorColorsPayload*)rx_packet.payload;

        String newS1 = getColorName(p->sensor1_color);
        String newS2 = getColorName(p->sensor2_color);
        String newS3 = getColorName(p->sensor3_color);

        // DEBUG: Log sensor color updates (reduced frequency)
        static unsigned long lastDebugPrint = 0;
        if (millis() - lastDebugPrint > 10000) { // Every 10 seconds
            Serial.printf("[SENSOR-COLORS] Received: S1=%s(%d) S2=%s(%d) S3=%s(%d)\n",
                         newS1.c_str(), p->sensor1_color,
                         newS2.c_str(), p->sensor2_color,
                         newS3.c_str(), p->sensor3_color);
            lastDebugPrint = millis();
        }

        // Update immediately - no hold timer
        systemData.sensor1Color = newS1;
        systemData.sensor2Color = newS2;
        systemData.sensor3Color = newS3;
        systemData.lastSensorUpdate = millis();
        systemData.sensorDataHeld = true;

        // Log if colors actually changed
        static String lastS1 = "UNKNOWN", lastS2 = "UNKNOWN", lastS3 = "UNKNOWN";
        if (newS1 != lastS1 || newS2 != lastS2 || newS3 != lastS3) {
            Serial.printf("[SENSOR-COLORS] DISPLAY UPDATE: S1=%s S2=%s S3=%s\n",
                         newS1.c_str(), newS2.c_str(), newS3.c_str());
            lastS1 = newS1;
            lastS2 = newS2;
            lastS3 = newS3;
        }
    }

    void processIncidenceAngle() {
        IncidenceAnglePayload* p = (IncidenceAnglePayload*)rx_packet.payload;
        if (p->angle != systemData.incidenceAngle) {
            systemData.incidenceAngle = p->angle;
            systemData.lastIncidenceUpdate = millis();
            systemData.incidenceDataHeld = true;
        }
    }

    void processRotationAngle() {
        RotationAnglePayload* p = (RotationAnglePayload*)rx_packet.payload;
        systemData.rotationAngle = p->angle;
        systemData.rotationDirection = (p->direction == 2) ? "LEFT" : (p->direction == 3) ? "RIGHT" : "UNKNOWN";
        systemData.lastRotationUpdate = millis();
        systemData.rotationDataHeld = true;
    }

    void processEndOfMaze() {
        systemData.endOfMazeDetected = true;
        systemData.endOfMazeTime = millis();
        Serial.println("\n╔═══════════════════════════════════════╗");
        Serial.println("║  🎉 END OF MAZE DETECTED! 🎉         ║");
        Serial.println("║  WiFi Client Received EOM Packet     ║");
        Serial.println("╚═══════════════════════════════════════╝\n");
        Serial.printf("[END-OF-MAZE] Flag set at time: %lu ms\n", systemData.endOfMazeTime);
        Serial.printf("[END-OF-MAZE] systemData.endOfMazeDetected = %d\n", systemData.endOfMazeDetected);
        Serial.println("[END-OF-MAZE] Web page should now show MAZE COMPLETE banner!");
    }

    void processWheelSpeeds() {
        WheelSpeedsPayload* p = (WheelSpeedsPayload*)rx_packet.payload;

        // Debug: Log wheel speeds every time (temporarily)
        static unsigned long lastWheelDebug = 0;
        if (millis() - lastWheelDebug > 1000) {
            Serial.printf("[WHEEL-SPEEDS] Received: vR=%d vL=%d setpoint=%d\n",
                         p->vR, p->vL, p->vop_setpoint);
            lastWheelDebug = millis();
        }

        // Update and mark as held for visibility
        if (p->vR != systemData.wheelSpeedR || p->vL != systemData.wheelSpeedL ||
            p->vop_setpoint != systemData.wheelSetpoint) {
            systemData.wheelSpeedR = p->vR;
            systemData.wheelSpeedL = p->vL;
            systemData.wheelSetpoint = p->vop_setpoint;
            systemData.lastMovementUpdate = millis();
            systemData.movementDataHeld = true;

            Serial.printf("[WHEEL-SPEEDS] Updated systemData: R=%d L=%d Set=%d\n",
                         systemData.wheelSpeedR, systemData.wheelSpeedL, systemData.wheelSetpoint);
        }
    }

    void processDistance() {
        DistancePayload* p = (DistancePayload*)rx_packet.payload;

        if (p->distance_mm != systemData.distance_mm) {
            systemData.distance_mm = p->distance_mm;
            systemData.lastMovementUpdate = millis();
            systemData.movementDataHeld = true;
        }
    }

    void processNavconState() {
        NavconStatePayload* p = (NavconStatePayload*)rx_packet.payload;
        systemData.navconState = getNavconStateName(p->new_state);
    }

    void processLineDetection() {
        LineDetectionPayload* p = (LineDetectionPayload*)rx_packet.payload;
        systemData.lineColor = getColorName(p->color);
        systemData.lineAngle = p->angle / 10.0;
        systemData.lineType = (p->line_type == 0) ? "NAVIGABLE" : "WALL";
    }

    void processDebugMessage() {
        DebugMessagePayload* p = (DebugMessagePayload*)rx_packet.payload;
        systemData.lastDebugMessage = String(p->message);
        switch(p->severity) {
            case 0: systemData.lastDebugSeverity = "INFO"; break;
            case 1: systemData.lastDebugSeverity = "WARN"; break;
            case 2: systemData.lastDebugSeverity = "ERROR"; break;
            default: systemData.lastDebugSeverity = "UNKNOWN"; break;
        }
    }

    String getColorName(uint8_t color) {
        switch(color) {
            case 0: return "WHITE";
            case 1: return "RED";
            case 2: return "GREEN";
            case 3: return "BLUE";
            case 4: return "BLACK";
            default: return "UNKNOWN";
        }
    }

    String getNavconStateName(uint8_t state) {
        switch(state) {
            case 0: return "FORWARD_SCAN";
            case 1: return "STOP";
            case 2: return "REVERSE";
            case 3: return "STOP_BEFORE_ROTATE";
            case 4: return "ROTATE";
            case 5: return "EVALUATE_CORRECTION";
            case 6: return "CROSSING_LINE";
            default: return "UNKNOWN";
        }
    }

    const char* getPacketTypeName(uint8_t type) {
        switch(type) {
            case PKT_SYSTEM_STATE: return "SYS_STATE";
            case PKT_TOUCH_DETECTED: return "TOUCH";
            case PKT_PURE_TONE: return "TONE";
            case PKT_SENSOR_COLORS: return "SENSOR_COLORS";
            case PKT_INCIDENCE_ANGLE: return "INCIDENCE";
            case PKT_END_OF_MAZE: return "END_OF_MAZE";
            case PKT_WHEEL_SPEEDS: return "WHEEL_SPEEDS";
            case PKT_DISTANCE: return "DISTANCE";
            case PKT_ROTATION_ANGLE: return "ROTATION";
            case PKT_LINE_DETECTION: return "LINE_DETECT";
            case PKT_NAVCON_STATE: return "NAVCON_STATE";
            case PKT_ROTATION_COMMAND: return "ROT_CMD";
            case PKT_ROTATION_FEEDBACK: return "ROT_FEEDBACK";
            case PKT_ANGLE_EVALUATION: return "ANGLE_EVAL";
            case PKT_DEBUG_MESSAGE: return "DEBUG";
            case PKT_HEARTBEAT: return "HEARTBEAT";
            default: return "UNKNOWN";
        }
    }

private:
    bool initialized = false;
    spi_slave_transaction_t trans;

public:
    void begin() {
        // Configuration for VSPI slave
        spi_bus_config_t buscfg = {
            .mosi_io_num = SPI_MOSI,
            .miso_io_num = SPI_MISO,
            .sclk_io_num = SPI_SCK,
            .quadwp_io_num = -1,
            .quadhd_io_num = -1,
            .max_transfer_sz = sizeof(SPIPacket),
        };

        spi_slave_interface_config_t slvcfg = {
            .spics_io_num = SPI_CS,
            .flags = 0,
            .queue_size = 3,
            .mode = 0,
            .post_setup_cb = NULL,
            .post_trans_cb = NULL
        };

        // Enable pull-ups on SPI lines for stability
        gpio_set_pull_mode((gpio_num_t)SPI_MOSI, GPIO_PULLUP_ONLY);
        gpio_set_pull_mode((gpio_num_t)SPI_SCK, GPIO_PULLUP_ONLY);
        gpio_set_pull_mode((gpio_num_t)SPI_CS, GPIO_PULLUP_ONLY);

        // Initialize SPI slave
        esp_err_t ret = spi_slave_initialize(VSPI_HOST, &buscfg, &slvcfg, SPI_DMA_CH_AUTO);

        if (ret == ESP_OK) {
            initialized = true;
            Serial.println("✅ SPI Slave initialized successfully!");
            Serial.printf("   MOSI: GPIO %d, MISO: GPIO %d, SCK: GPIO %d, CS: GPIO %d\n",
                         SPI_MOSI, SPI_MISO, SPI_SCK, SPI_CS);

            // Prepare first transaction
            memset(&trans, 0, sizeof(trans));
            memset(spi_slave_rx_buf, 0, sizeof(spi_slave_rx_buf));
            trans.length = sizeof(SPIPacket) * 8;
            trans.rx_buffer = spi_slave_rx_buf;
            trans.tx_buffer = spi_slave_tx_buf;

            // Queue the transaction
            spi_slave_queue_trans(VSPI_HOST, &trans, portMAX_DELAY);
            Serial.println("   SPI slave ready to receive data");
        } else {
            Serial.printf("❌ SPI Slave init failed: %d\n", ret);
            initialized = false;
        }
    }

    void poll() {
        if (!initialized) {
            systemData.connectionStatus = false;
            return;
        }

        // Try to get completed transaction (non-blocking, 0ms timeout)
        spi_slave_transaction_t *rtrans;
        esp_err_t ret = spi_slave_get_trans_result(VSPI_HOST, &rtrans, 0);

        if (ret == ESP_OK) {
            // Transaction completed! Copy data
            memcpy(&rx_packet, spi_slave_rx_buf, sizeof(SPIPacket));

            // Debug output
            static unsigned long lastDebug = 0;
            if (millis() - lastDebug > 5000) {
                Serial.printf("[SPI] Received %d bytes, sync1=0x%02X, sync2=0x%02X, type=0x%02X\n",
                             rtrans->trans_len / 8, rx_packet.header.sync1,
                             rx_packet.header.sync2, rx_packet.header.packet_type);
                lastDebug = millis();
            }

            // Process if valid
            if (rx_packet.header.sync1 == 0xAA && rx_packet.header.sync2 == 0x55) {
                processPacket();
            }

            // Queue next transaction immediately
            memset(&trans, 0, sizeof(trans));
            memset(spi_slave_rx_buf, 0, sizeof(spi_slave_rx_buf));
            trans.length = sizeof(SPIPacket) * 8;
            trans.rx_buffer = spi_slave_rx_buf;
            trans.tx_buffer = spi_slave_tx_buf;
            spi_slave_queue_trans(VSPI_HOST, &trans, portMAX_DELAY);
        }
    }
};

WiFiSPIReceiver spiReceiver;

// ==================== UTILITY FUNCTIONS ====================
String getTimeString() {
    unsigned long seconds = millis() / 1000;
    unsigned long minutes = seconds / 60;
    unsigned long hours = minutes / 60;
    seconds %= 60;
    minutes %= 60;
    hours %= 24;

    char timeStr[20];
    sprintf(timeStr, "%02lu:%02lu:%02lu", hours, minutes, seconds);
    return String(timeStr);
}

void sendGPIOCommand(uint8_t pin, const String& commandName) {
    digitalWrite(pin, HIGH);
    delay(100);  // Hold high for 100ms
    digitalWrite(pin, LOW);
    Serial.println("Sent " + commandName + " command via GPIO " + String(pin));
}

bool connectToWiFi() {
    // Create WiFi Access Point - ESP32 becomes a hotspot
    Serial.println("Creating MARV WiFi hotspot...");
    Serial.print("Network Name: ");
    Serial.println(ap_ssid);
    Serial.print("Password: ");
    Serial.println(ap_password);

    WiFi.mode(WIFI_AP);
    bool success = WiFi.softAP(ap_ssid, ap_password);

    if (success) {
        // digitalWrite(STATUS_LED, HIGH);  // Disabled - conflicts with CMD_TONE_OUT
        Serial.println("✅ MARV WiFi hotspot created successfully!");
        Serial.print("📱 Connect your phone to: ");
        Serial.println(ap_ssid);
        Serial.print("🔐 Password: ");
        Serial.println(ap_password);
        Serial.print("🌐 Web interface: http://");
        Serial.println(WiFi.softAPIP());
        return true;
    } else {
        Serial.println("❌ Failed to create WiFi hotspot!");
        return false;
    }
}

// ==================== WEB SERVER HANDLERS ====================
void handleRoot() {
    String html = R"=====(
<!DOCTYPE html>
<html>
<head>
    <title>MARV System Monitor</title>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: #333;
            min-height: 100vh;
        }
        .container {
            max-width: 1400px;
            margin: 0 auto;
            padding: 20px;
        }
        .header {
            text-align: center;
            color: white;
            margin-bottom: 30px;
            text-shadow: 2px 2px 4px rgba(0,0,0,0.3);
        }
        .header h1 { font-size: 2.5em; margin-bottom: 10px; }
        .header p { font-size: 1.2em; opacity: 0.9; }
        .status-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
            gap: 20px;
            margin-bottom: 30px;
        }
        .card {
            background: rgba(255,255,255,0.95);
            border-radius: 15px;
            padding: 25px;
            box-shadow: 0 8px 32px rgba(0,0,0,0.1);
            backdrop-filter: blur(10px);
            border: 1px solid rgba(255,255,255,0.2);
            transition: transform 0.3s ease;
        }
        .card:hover { transform: translateY(-5px); }
        .card h3 {
            color: #4a5568;
            margin-bottom: 15px;
            font-size: 1.3em;
            border-bottom: 2px solid #e2e8f0;
            padding-bottom: 8px;
        }
        .status-item {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin: 12px 0;
            padding: 8px 0;
        }
        .status-label { font-weight: 600; color: #2d3748; }
        .status-value {
            font-weight: 500;
            padding: 4px 12px;
            border-radius: 20px;
            background: #f7fafc;
            border: 1px solid #e2e8f0;
        }
        .status-online { background: #c6f6d5; color: #22543d; border-color: #9ae6b4; }
        .status-offline { background: #fed7d7; color: #742a2a; border-color: #fc8181; }
        .status-active { background: #bee3f8; color: #2a4365; border-color: #90cdf4; }
        .status-held {
            background: #fef3c7;
            color: #78350f;
            border-color: #fbbf24;
            animation: pulse-hold 1s ease-in-out infinite;
        }
        @keyframes pulse-hold {
            0%, 100% { opacity: 1; }
            50% { opacity: 0.7; }
        }
        .controls {
            background: rgba(255,255,255,0.95);
            border-radius: 15px;
            padding: 25px;
            margin-bottom: 20px;
            box-shadow: 0 8px 32px rgba(0,0,0,0.1);
        }
        .controls h3 {
            color: #4a5568;
            margin-bottom: 20px;
            font-size: 1.3em;
            text-align: center;
        }
        .button-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(150px, 1fr));
            gap: 15px;
        }
        .control-btn {
            background: linear-gradient(135deg, #4299e1, #3182ce);
            color: white;
            border: none;
            padding: 15px 20px;
            border-radius: 10px;
            cursor: pointer;
            font-size: 1em;
            font-weight: 600;
            transition: all 0.3s ease;
            box-shadow: 0 4px 15px rgba(66, 153, 225, 0.3);
        }
        .control-btn:hover {
            background: linear-gradient(135deg, #3182ce, #2c5282);
            transform: translateY(-2px);
            box-shadow: 0 6px 20px rgba(66, 153, 225, 0.4);
        }
        .control-btn:active { transform: translateY(0); }
        .sensor-colors {
            display: flex;
            justify-content: space-around;
            align-items: center;
            margin: 15px 0;
        }
        .sensor-color {
            width: 60px;
            height: 60px;
            border-radius: 50%;
            border: 3px solid #fff;
            box-shadow: 0 4px 15px rgba(0,0,0,0.2);
            display: flex;
            align-items: center;
            justify-content: center;
            font-weight: bold;
            color: white;
            text-shadow: 1px 1px 2px rgba(0,0,0,0.5);
            font-size: 0.8em;
        }
        .color-white { background: #f7fafc; color: #2d3748; }
        .color-red { background: #e53e3e; }
        .color-green { background: #38a169; }
        .color-blue { background: #3182ce; }
        .color-black { background: #2d3748; }
        .color-unknown { background: #a0aec0; }
        .completion-banner {
            display: none;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            padding: 30px;
            text-align: center;
            font-size: 2em;
            font-weight: bold;
            border-radius: 15px;
            margin-bottom: 20px;
            box-shadow: 0 10px 40px rgba(102, 126, 234, 0.4);
            animation: celebrate 2s ease-in-out infinite;
        }
        .completion-banner.active { display: block; }
        @keyframes celebrate {
            0%, 100% { transform: scale(1); }
            50% { transform: scale(1.05); }
        }
        .performance-bar {
            width: 100%;
            height: 8px;
            background: #e2e8f0;
            border-radius: 4px;
            overflow: hidden;
            margin: 10px 0;
        }
        .performance-fill {
            height: 100%;
            background: linear-gradient(90deg, #48bb78, #38a169);
            transition: width 0.3s ease;
        }
        @media (max-width: 768px) {
            .container { padding: 15px; }
            .header h1 { font-size: 2em; }
            .button-grid { grid-template-columns: 1fr; }
        }
    </style>
</head>
<body>
    <div class="container">
        <div id="completion-banner" class="completion-banner">
            🎉 MAZE COMPLETE! 🎉
        </div>
        <div id="debug-banner" style="background: #333; color: #0f0; padding: 10px; text-align: center; font-family: monospace; font-size: 0.9em; display: none;">
            DEBUG: End-of-Maze Flag = <span id="debug-eom-flag">false</span> | Last Update: <span id="debug-timestamp">--</span>
        </div>
        <div class="header">
            <h1>🤖 MARV System Monitor</h1>
            <p>Real-time navigation and sensor data monitoring</p>
        </div>

        <div class="status-grid">
            <div class="card">
                <h3>🔧 System Status</h3>
                <div class="status-item">
                    <span class="status-label">Connection:</span>
                    <span id="connection-status" class="status-value">Checking...</span>
                </div>
                <div class="status-item">
                    <span class="status-label">System State:</span>
                    <span id="system-state" class="status-value">Loading...</span>
                </div>
                <div class="status-item">
                    <span class="status-label">Navigation:</span>
                    <span id="navcon-state" class="status-value">Loading...</span>
                </div>
                <div class="status-item">
                    <span class="status-label">Last Update:</span>
                    <span id="last-update" class="status-value">Never</span>
                </div>
                <div class="status-item">
                    <span class="status-label">End of Maze:</span>
                    <span id="end-of-maze" class="status-value">No</span>
                </div>
            </div>

            <div class="card">
                <h3>🎨 Sensor Colors</h3>
                <div class="sensor-colors">
                    <div class="sensor-color" id="sensor1">S1</div>
                    <div class="sensor-color" id="sensor2">S2</div>
                    <div class="sensor-color" id="sensor3">S3</div>
                </div>
                <div class="status-item">
                    <span class="status-label">Line Color:</span>
                    <span id="line-color" class="status-value">None</span>
                </div>
                <div class="status-item">
                    <span class="status-label">Line Angle:</span>
                    <span id="line-angle" class="status-value">0.0°</span>
                </div>
                <div class="status-item">
                    <span class="status-label">Incidence:</span>
                    <span id="incidence-angle" class="status-value">0°</span>
                </div>
            </div>

            <div class="card">
                <h3>🔄 Rotation Data</h3>
                <div class="status-item">
                    <span class="status-label">Angle:</span>
                    <span id="rotation-angle" class="status-value">0°</span>
                </div>
                <div class="status-item">
                    <span class="status-label">Direction:</span>
                    <span id="rotation-direction" class="status-value">NONE</span>
                </div>
            </div>

            <div class="card">
                <h3>⚡ Movement Data</h3>
                <div class="status-item">
                    <span class="status-label">Right Wheel:</span>
                    <span id="wheel-r" class="status-value">0 mm/s</span>
                </div>
                <div class="status-item">
                    <span class="status-label">Left Wheel:</span>
                    <span id="wheel-l" class="status-value">0 mm/s</span>
                </div>
                <div class="status-item">
                    <span class="status-label">Setpoint:</span>
                    <span id="wheel-setpoint" class="status-value">0 mm/s</span>
                </div>
                <div class="status-item">
                    <span class="status-label">Distance:</span>
                    <span id="distance" class="status-value">0 mm</span>
                </div>
            </div>

            <div class="card">
                <h3>📊 Performance</h3>
                <div class="status-item">
                    <span class="status-label">Packets/sec:</span>
                    <span id="packets-per-sec" class="status-value">0</span>
                </div>
                <div class="status-item">
                    <span class="status-label">Data Quality:</span>
                    <span id="data-quality" class="status-value">Excellent</span>
                </div>
                <div class="performance-bar">
                    <div id="performance-fill" class="performance-fill" style="width: 0%"></div>
                </div>
            </div>
        </div>

        <div class="controls">
            <h3>🎮 Manual Controls</h3>
            <div class="button-grid">
                <button class="control-btn" onclick="sendCommand('touch')">Touch Sensor</button>
                <button class="control-btn" onclick="sendCommand('tone')">Pure Tone</button>
                <button class="control-btn" onclick="sendCommand('send')">Send Packet</button>
            </div>
        </div>

        <div class="controls">
            <h3>🔄 System Reset</h3>
            <div class="button-grid">
                <button class="control-btn" style="background: linear-gradient(135deg, #f56565, #c53030);" onclick="confirmReset('wifi')">Reset WiFi ESP32</button>
                <button class="control-btn" style="background: linear-gradient(135deg, #ed8936, #c05621);" onclick="sendCommand('reset_main')">Reset Main ESP32</button>
            </div>
        </div>

        <div class="card">
            <h3>🐛 Debug Information</h3>
            <div class="status-item">
                <span class="status-label">Last Message:</span>
                <span id="debug-message" class="status-value">None</span>
            </div>
            <div class="status-item">
                <span class="status-label">Severity:</span>
                <span id="debug-severity" class="status-value">INFO</span>
            </div>
        </div>
    </div>

    <script>
        let isUpdating = false; // Prevent overlapping requests
        let lastUpdateTime = 0;
        let updateTimeout = null;

        function updateStatus() {
            // Skip if already updating OR if last update was < 5ms ago (prevents spam)
            const now = Date.now();
            if (isUpdating || (now - lastUpdateTime < 5)) return;

            isUpdating = true;
            lastUpdateTime = now;

            // Safety timeout - reset lock after 100ms if stuck
            if (updateTimeout) clearTimeout(updateTimeout);
            updateTimeout = setTimeout(() => {
                if (isUpdating) {
                    console.warn('Request timeout - resetting lock');
                    isUpdating = false;
                }
            }, 100);

            fetch('/api/status', {
                method: 'GET',
                cache: 'no-cache',
                priority: 'high'
            })
                .then(response => response.json())
                .then(data => {
                    clearTimeout(updateTimeout);
                    isUpdating = false;
                    // Connection status
                    const connStatus = document.getElementById('connection-status');
                    connStatus.textContent = data.connectionStatus ? 'Connected' : 'Disconnected';
                    connStatus.className = 'status-value ' + (data.connectionStatus ? 'status-online' : 'status-offline');

                    // System states
                    document.getElementById('system-state').textContent = data.systemState;
                    document.getElementById('navcon-state').textContent = data.navconState;
                    document.getElementById('last-update').textContent = data.lastUpdate;

                    // End of maze indicator
                    const eom = document.getElementById('end-of-maze');
                    eom.textContent = data.endOfMazeDetected ? 'YES! 🎉' : 'No';
                    eom.className = 'status-value ' + (data.endOfMazeDetected ? 'status-online' : '');

                    // Show/hide completion banner
                    const banner = document.getElementById('completion-banner');
                    if (data.endOfMazeDetected) {
                        console.log('END-OF-MAZE DETECTED! Showing banner...');
                        banner.classList.add('active');
                    } else {
                        banner.classList.remove('active');
                    }

                    // Debug: Log end-of-maze status every time it changes
                    if (window.lastEomStatus !== data.endOfMazeDetected) {
                        console.log('End-of-Maze Status Changed:', data.endOfMazeDetected);
                        window.lastEomStatus = data.endOfMazeDetected;
                    }

                    // Update visible debug banner (for mobile debugging)
                    const debugBanner = document.getElementById('debug-banner');
                    const debugFlag = document.getElementById('debug-eom-flag');
                    const debugTimestamp = document.getElementById('debug-timestamp');
                    debugFlag.textContent = data.endOfMazeDetected ? 'TRUE ✅' : 'FALSE';
                    debugFlag.style.color = data.endOfMazeDetected ? '#0f0' : '#f00';
                    debugTimestamp.textContent = new Date().toLocaleTimeString();
                    // Show debug banner if user taps header 5 times
                    if (!window.debugTapCount) window.debugTapCount = 0;

                    // Sensor colors (no hold indicator - just display)
                    updateSensorColor('sensor1', data.sensor1Color, false);
                    updateSensorColor('sensor2', data.sensor2Color, false);
                    updateSensorColor('sensor3', data.sensor3Color, false);

                    document.getElementById('line-color').textContent = data.lineColor;
                    document.getElementById('line-angle').textContent = data.lineAngle + '°';

                    // Incidence angle (no hold indicator, just display)
                    const incAngle = document.getElementById('incidence-angle');
                    incAngle.textContent = data.incidenceAngle + '°';
                    incAngle.className = 'status-value';

                    // Rotation data (no hold indicator, just display)
                    const rotAngle = document.getElementById('rotation-angle');
                    rotAngle.textContent = data.rotationAngle + '°';
                    rotAngle.className = 'status-value';

                    const rotDir = document.getElementById('rotation-direction');
                    rotDir.textContent = data.rotationDirection;
                    if (data.rotationDirection === 'LEFT') {
                        rotDir.className = 'status-value status-active';
                    } else if (data.rotationDirection === 'RIGHT') {
                        rotDir.className = 'status-value status-online';
                    } else {
                        rotDir.className = 'status-value';
                    }

                    // Movement data (no hold indicator, just display)
                    document.getElementById('wheel-r').textContent = data.wheelSpeedR + ' mm/s';
                    document.getElementById('wheel-l').textContent = data.wheelSpeedL + ' mm/s';
                    document.getElementById('wheel-setpoint').textContent = data.wheelSetpoint + ' mm/s';
                    document.getElementById('distance').textContent = data.distance + ' mm';

                    // Performance
                    document.getElementById('packets-per-sec').textContent = data.packetsPerSecond;

                    // Data quality indicator
                    const quality = document.getElementById('data-quality');
                    if (data.packetsCorrupted > 10) {
                        quality.textContent = 'Poor';
                        quality.className = 'status-value status-offline';
                    } else if (data.packetsCorrupted > 0) {
                        quality.textContent = 'Good';
                        quality.className = 'status-value';
                    } else {
                        quality.textContent = 'Excellent';
                        quality.className = 'status-value status-online';
                    }

                    // Performance bar (0-30 packets/sec scale for better visualization)
                    const perfPercent = Math.min(data.packetsPerSecond / 30 * 100, 100);
                    document.getElementById('performance-fill').style.width = perfPercent + '%';

                    // Debug info
                    document.getElementById('debug-message').textContent = data.lastDebugMessage;
                    document.getElementById('debug-severity').textContent = data.lastDebugSeverity;
                })
                .catch(error => {
                    clearTimeout(updateTimeout);
                    isUpdating = false;
                    console.error('Error fetching status:', error);
                    document.getElementById('connection-status').textContent = 'Error';
                    document.getElementById('connection-status').className = 'status-value status-offline';
                });
        }

        function updateSensorColor(elementId, color, isHeld) {
            const element = document.getElementById(elementId);
            element.className = 'sensor-color color-' + color.toLowerCase();
            element.textContent = elementId.toUpperCase();

            // Add pulsing border if held
            if (isHeld) {
                element.style.border = '4px solid #fbbf24';
                element.style.animation = 'pulse-hold 1s ease-in-out infinite';
            } else {
                element.style.border = '3px solid #fff';
                element.style.animation = 'none';
            }
        }

        function sendCommand(command) {
            fetch('/api/command', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ command: command })
            })
            .then(response => response.json())
            .then(data => {
                if (data.success) {
                    // Visual feedback
                    event.target.style.background = 'linear-gradient(135deg, #38a169, #2f855a)';
                    setTimeout(() => {
                        event.target.style.background = 'linear-gradient(135deg, #4299e1, #3182ce)';
                    }, 300);
                }
            })
            .catch(error => console.error('Error sending command:', error));
        }

        // Toggle debug banner by tapping header 5 times
        function toggleDebugBanner() {
            if (!window.debugTapCount) window.debugTapCount = 0;
            window.debugTapCount++;

            if (window.debugTapCount >= 5) {
                const debugBanner = document.getElementById('debug-banner');
                if (debugBanner.style.display === 'none') {
                    debugBanner.style.display = 'block';
                    console.log('Debug banner ENABLED');
                } else {
                    debugBanner.style.display = 'none';
                    console.log('Debug banner DISABLED');
                }
                window.debugTapCount = 0;
            }

            // Reset counter after 2 seconds
            setTimeout(() => { window.debugTapCount = 0; }, 2000);
        }

        // Attach to header
        document.addEventListener('DOMContentLoaded', function() {
            const header = document.querySelector('.header h1');
            if (header) {
                header.addEventListener('click', toggleDebugBanner);
                header.style.cursor = 'pointer';
            }
        });

        function confirmReset(type) {
            if (type === 'wifi') {
                if (confirm('⚠️ Reset WiFi ESP32? You will lose connection and need to reconnect.')) {
                    fetch('/api/command', {
                        method: 'POST',
                        headers: { 'Content-Type': 'application/json' },
                        body: JSON.stringify({ command: 'reset_wifi' })
                    });
                    setTimeout(() => {
                        alert('WiFi ESP32 resetting... Please wait 5 seconds and refresh the page.');
                    }, 100);
                }
            }
        }

        // Update status every 10ms (100Hz - matches SPI rate, ultra-low latency)
        setInterval(updateStatus, 10);
        updateStatus(); // Initial load
    </script>
</body>
</html>
)=====";

    server.send(200, "text/html", html);
}

void handleApiStatus() {
    DynamicJsonDocument doc(1024);

    doc["systemState"] = systemData.systemState;
    doc["navconState"] = systemData.navconState;
    doc["lastUpdate"] = systemData.lastUpdate;
    doc["packetsReceived"] = systemData.packetsReceived;
    doc["packetsCorrupted"] = systemData.packetsCorrupted;
    doc["connectionStatus"] = systemData.connectionStatus;
    doc["sensor1Color"] = systemData.sensor1Color;
    doc["sensor2Color"] = systemData.sensor2Color;
    doc["sensor3Color"] = systemData.sensor3Color;
    doc["wheelSpeedR"] = systemData.wheelSpeedR;
    doc["wheelSpeedL"] = systemData.wheelSpeedL;
    doc["wheelSetpoint"] = systemData.wheelSetpoint;
    doc["distance"] = systemData.distance_mm;
    doc["lineColor"] = systemData.lineColor;
    doc["lineAngle"] = systemData.lineAngle;
    doc["lineType"] = systemData.lineType;
    doc["incidenceAngle"] = systemData.incidenceAngle;
    doc["rotationAngle"] = systemData.rotationAngle;
    doc["rotationDirection"] = systemData.rotationDirection;
    doc["rotationDataHeld"] = systemData.rotationDataHeld;
    doc["incidenceDataHeld"] = systemData.incidenceDataHeld;
    doc["sensorDataHeld"] = systemData.sensorDataHeld;
    doc["movementDataHeld"] = systemData.movementDataHeld;
    doc["endOfMazeDetected"] = systemData.endOfMazeDetected;
    doc["lastDebugMessage"] = systemData.lastDebugMessage;
    doc["lastDebugSeverity"] = systemData.lastDebugSeverity;
    doc["packetsPerSecond"] = systemData.packetsPerSecond;

    // Debug: Log end-of-maze status when sending JSON
    static bool lastEomState = false;
    if (systemData.endOfMazeDetected != lastEomState) {
        Serial.printf("[JSON-API] endOfMazeDetected changed to: %d\n", systemData.endOfMazeDetected);
        lastEomState = systemData.endOfMazeDetected;
    }

    String response;
    serializeJson(doc, response);

    // Debug: Print full JSON when end-of-maze is detected (once per second)
    static unsigned long lastEomJsonPrint = 0;
    if (systemData.endOfMazeDetected && (millis() - lastEomJsonPrint > 1000)) {
        Serial.println("[JSON-API] Sending end-of-maze=true to web page");
        Serial.printf("[JSON-API] JSON contains: endOfMazeDetected=%d\n", systemData.endOfMazeDetected);
        lastEomJsonPrint = millis();
    }

    server.send(200, "application/json", response);
}

void handleApiCommand() {
    if (server.method() != HTTP_POST) {
        server.send(405, "application/json", "{\"error\":\"Method not allowed\"}");
        return;
    }

    String body = server.arg("plain");
    DynamicJsonDocument doc(200);
    deserializeJson(doc, body);

    String command = doc["command"];
    bool success = false;

    if (command == "touch") {
        sendGPIOCommand(CMD_TOUCH_OUT, "TOUCH");
        success = true;
    } else if (command == "tone") {
        sendGPIOCommand(CMD_TONE_OUT, "PURE TONE");
        success = true;
    } else if (command == "send") {
        sendGPIOCommand(CMD_SEND_OUT, "SEND PACKET");
        success = true;
    } else if (command == "reset_wifi") {
        Serial.println("WiFi ESP32 reset requested via web interface");
        success = true;
        DynamicJsonDocument response(100);
        response["success"] = true;
        response["command"] = command;
        String responseStr;
        serializeJson(response, responseStr);
        server.send(200, "application/json", responseStr);
        delay(100);
        ESP.restart();
        return;
    } else if (command == "reset_main") {
        Serial.println("Main ESP32 reset requested - sending reset pulse on GPIO 15");
        // Send a long pulse to trigger reset on main ESP32
        digitalWrite(CMD_SEND_OUT, HIGH);
        delay(2000); // Hold for 2 seconds to distinguish from normal command
        digitalWrite(CMD_SEND_OUT, LOW);
        success = true;
    }

    DynamicJsonDocument response(100);
    response["success"] = success;
    response["command"] = command;

    String responseStr;
    serializeJson(response, responseStr);
    server.send(200, "application/json", responseStr);
}

// ==================== MAIN SETUP ====================
void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n╔══════════════════════════════════════════════════════╗");
    Serial.println("║        MARV WiFi Communication System               ║");
    Serial.println("║        ESP32 WiFi Communications Module             ║");
    Serial.println("╚══════════════════════════════════════════════════════╝");

    Serial.println("Setup starting...");
    Serial.print("Will create WiFi hotspot: ");
    Serial.println(ap_ssid);

    // Initialize GPIO pins
    pinMode(CMD_TOUCH_OUT, OUTPUT);
    pinMode(CMD_TONE_OUT, OUTPUT);
    pinMode(CMD_SEND_OUT, OUTPUT);
    // pinMode(STATUS_LED, OUTPUT);  // Disabled - conflicts with CMD_TONE_OUT

    digitalWrite(CMD_TOUCH_OUT, LOW);
    digitalWrite(CMD_TONE_OUT, LOW);
    digitalWrite(CMD_SEND_OUT, LOW);

    Serial.println("GPIO pins initialized for communication with main ESP32");
    Serial.println("  Touch Command   -> GPIO 4");
    Serial.println("  Tone Command    -> GPIO 2");
    Serial.println("  Send Command    -> GPIO 15");

    delay(1000); // Give some time for serial output
    Serial.println("About to initialize SPI...");
    Serial.flush(); // Ensure serial output is sent

    // Initialize SPI
    spiReceiver.begin();
    Serial.println("SPI initialization complete");
    Serial.flush();

    // Connect to WiFi
    Serial.println("Starting WiFi connection...");
    bool wifi_connected = connectToWiFi();

    if (wifi_connected) {
        Serial.println("\n🎉 MARV WiFi System Ready!");
        Serial.println("════════════════════════════════════");
        Serial.print("📱 WiFi Hotspot: ");
        Serial.println(ap_ssid);
        Serial.print("🔐 Password: ");
        Serial.println(ap_password);
        Serial.print("🌐 Web Interface: http://");
        Serial.println(WiFi.softAPIP());
        Serial.println("════════════════════════════════════");
    } else {
        Serial.println("❌ WiFi hotspot failed - continuing with SPI monitoring only");
    }

    // Setup web server routes
    server.on("/", handleRoot);
    server.on("/api/status", handleApiStatus);
    server.on("/api/command", handleApiCommand);

    server.begin();
    Serial.println("Web server started and ready for connections");

    if (wifi_connected) {
        Serial.println("\n📋 Quick Connection Guide:");
        Serial.println("   1. Connect phone to WiFi: MARV-WiFi");
        Serial.println("   2. Use password: marv1234");
        Serial.println("   3. Open browser to: http://192.168.4.1");
        Serial.println("   4. Monitor MARV robot in real-time!");
    }

    Serial.println("\n╔══════════════════════════════════════════════════════╗");
    Serial.println("║  System Ready - Monitoring SPI and serving web UI   ║");
    Serial.println("╚══════════════════════════════════════════════════════╝\n");
}

// ==================== MAIN LOOP ====================
void loop() {
    // Handle web server requests
    server.handleClient();

    // Poll SPI data (efficient polling every 10ms)
    static unsigned long lastSPIPoll = 0;
    if (millis() - lastSPIPoll > 10) {
        spiReceiver.poll();
        lastSPIPoll = millis();
    }

    // Clear hold flags after timeout (2 seconds for sensors, 500ms for others)
    static unsigned long lastHoldCheck = 0;
    if (millis() - lastHoldCheck > 100) { // Check every 100ms
        unsigned long now = millis();

        // Don't clear sensor hold - colors persist indefinitely until changed
        // (The sensorDataHeld flag is not used for display, just for data freshness tracking)

        // Clear movement hold after 500ms
        if (systemData.movementDataHeld && (now - systemData.lastMovementUpdate > 500)) {
            systemData.movementDataHeld = false;
        }

        // Clear rotation hold after 500ms
        if (systemData.rotationDataHeld && (now - systemData.lastRotationUpdate > 500)) {
            systemData.rotationDataHeld = false;
        }

        // Clear incidence hold after 500ms
        if (systemData.incidenceDataHeld && (now - systemData.lastIncidenceUpdate > 500)) {
            systemData.incidenceDataHeld = false;
        }

        lastHoldCheck = now;
    }

    // WiFi AP monitoring
    static unsigned long lastWiFiCheck = 0;
    if (millis() - lastWiFiCheck > 30000) { // Check every 30 seconds
        // Check if AP is still active (we're in AP mode, not station mode)
        if (WiFi.getMode() != WIFI_AP && WiFi.getMode() != WIFI_AP_STA) {
            Serial.println("WiFi AP inactive, restarting...");
            WiFi.mode(WIFI_AP);
            WiFi.softAP(ap_ssid, ap_password);
        }
        lastWiFiCheck = millis();
    }

    // Status LED heartbeat - DISABLED due to conflict with CMD_TONE_OUT on GPIO 2
    // static unsigned long lastHeartbeat = 0;
    // if (millis() - lastHeartbeat > 1000) {
    //     // Check if AP is active and SPI connection is good
    //     if ((WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) && systemData.connectionStatus) {
    //         digitalWrite(STATUS_LED, !digitalRead(STATUS_LED)); // Fast blink = all good
    //     } else {
    //         digitalWrite(STATUS_LED, LOW); // Solid off = issues
    //     }
    //     lastHeartbeat = millis();
    // }

    // Performance monitoring - print stats every 10 seconds
    static unsigned long lastStats = 0;
    if (millis() - lastStats > 10000) {
        Serial.println("\n╔════════════════════════════════════════════════════╗");
        Serial.println("║       WiFi Communications Status Report           ║");
        Serial.println("╠════════════════════════════════════════════════════╣");
        Serial.printf("║ Packets received:    %6d                      ║\n", systemData.packetsReceived);
        Serial.printf("║ Packets corrupted:   %6d                      ║\n", systemData.packetsCorrupted);
        Serial.printf("║ Packets/second:      %6.1f                      ║\n", systemData.packetsPerSecond);
        Serial.printf("║ SPI connection:      %-10s                ║\n", systemData.connectionStatus ? "Active" : "Inactive");
        Serial.println("╠════════════════════════════════════════════════════╣");
        Serial.printf("║ Current Data:                                      ║\n");
        Serial.printf("║   Sensors: S1=%-7s S2=%-7s S3=%-7s    ║\n",
                     systemData.sensor1Color.c_str(),
                     systemData.sensor2Color.c_str(),
                     systemData.sensor3Color.c_str());
        Serial.printf("║   Wheel Speeds: R=%3d L=%3d Set=%3d             ║\n",
                     systemData.wheelSpeedR,
                     systemData.wheelSpeedL,
                     systemData.wheelSetpoint);
        Serial.printf("║   Distance: %5d mm                              ║\n", systemData.distance_mm);
        Serial.printf("║   Rotation: %3d° %-10s                     ║\n",
                     systemData.rotationAngle,
                     systemData.rotationDirection.c_str());
        Serial.printf("║   End of Maze: %s                                 ║\n",
                     systemData.endOfMazeDetected ? "YES 🎉" : "No");
        Serial.println("╠════════════════════════════════════════════════════╣");
        Serial.printf("║ WiFi signal: %d dBm                              ║\n", WiFi.RSSI());
        Serial.printf("║ Free heap:   %d bytes                          ║\n", ESP.getFreeHeap());
        Serial.println("╚════════════════════════════════════════════════════╝\n");
        lastStats = millis();
    }

    // Small delay to prevent overwhelming the system
    delay(1);
}

// ==================== CONFIGURATION NOTES ====================
/*
 * BEFORE UPLOADING:
 * 1. Change WiFi credentials at the top of this file
 * 2. Verify SPI wiring connections
 * 3. Ensure main ESP32 is running and sending SPI data
 *
 * FEATURES:
 * - Professional web interface accessible from any device
 * - Real-time SPI data monitoring with 100ms updates
 * - GPIO command transmission to main ESP32
 * - Performance monitoring and statistics
 * - Responsive design for mobile devices
 * - Visual sensor color indicators
 * - System status and connection monitoring
 *
 * WIRING VERIFICATION:
 * - SPI pins must match your main ESP32 SPI configuration
 * - GPIO command pins connect to main ESP32 input pins
 * - Common ground connection is essential
 *
 * PERFORMANCE:
 * - SPI polling every 10ms (100Hz max packet rate)
 * - Web updates every 100ms for responsive UI
 * - Efficient memory usage with minimal heap fragmentation
 * - Automatic WiFi reconnection on disconnection
 */