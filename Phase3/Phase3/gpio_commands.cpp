#include "gpio_commands.h"
#include "system_state.h"

// ==================== GPIO SETUP ====================
void setupGPIOCommands() {
    Serial.println("Setting up GPIO command inputs...");

    pinMode(CMD_TOUCH_PIN, INPUT_PULLDOWN);
    pinMode(CMD_TONE_PIN, INPUT_PULLDOWN);
    pinMode(CMD_SEND_PIN, INPUT_PULLDOWN);

    // Setup ADC for pure tone detection
    pinMode(PURE_TONE_ADC_PIN, INPUT);
    analogSetAttenuation(ADC_11db);  // Full range 0-3.3V

    Serial.println("GPIO command pins initialized:");
    Serial.println("   Touch Command = GPIO 4 (Input)");
    Serial.println("   Pure Tone     = GPIO 2 (Input)");
    Serial.println("   Send Packet   = GPIO 15 (Input)");
    Serial.println("   Pure Tone ADC = GPIO 36 (ADC Input, 0-3.3V)");
}

// ==================== GPIO COMMAND CHECKING ====================
bool checkWiFiCommands() {
    bool commandReceived = false;

    // Check for touch command (GPIO 4)
    if (digitalRead(CMD_TOUCH_PIN) == HIGH) {
        systemStatus.touchDetected = true;
        systemStatus.gpioCommandCount++;
        Serial.println("TOUCH command received via GPIO 4!");
        commandReceived = true;
        
        // Wait for pin to go low (debounce)
        while (digitalRead(CMD_TOUCH_PIN) == HIGH) {
            delay(1);
        }
        Serial.println("Touch pulse completed");
    }
    
    // Check for pure tone command (GPIO 2)
    if (digitalRead(CMD_TONE_PIN) == HIGH) {
        systemStatus.pureToneDetected = true;
        systemStatus.gpioCommandCount++;
        Serial.println("PURE TONE command received via GPIO 2!");
        commandReceived = true;
        
        // Wait for pin to go low (debounce)
        while (digitalRead(CMD_TONE_PIN) == HIGH) {
            delay(1);
        }
        Serial.println("Pure tone pulse completed");
    }
    
    // Check for send packet command (GPIO 15)
    // Also detect long pulse (>1 second) as reset command
    if (digitalRead(CMD_SEND_PIN) == HIGH) {
        unsigned long pulseStart = millis();

        // Wait for pin to go low and measure pulse length
        while (digitalRead(CMD_SEND_PIN) == HIGH) {
            delay(1);
            // Check if it's a long pulse (reset command)
            if (millis() - pulseStart > 1000) {
                Serial.println("RESET command detected (long pulse on GPIO 15)!");
                Serial.println("Restarting Main ESP32 in 1 second...");
                delay(1000);
                ESP.restart();
            }
        }

        // Normal short pulse = send packet command
        systemStatus.manualSendTrigger = true;
        systemStatus.gpioCommandCount++;
        Serial.println("SEND PACKET command received via GPIO 15!");
        commandReceived = true;
        Serial.println("Send packet pulse completed");
    }
    
    return commandReceived;
}

// ==================== PURE TONE ADC CHECKING ====================
bool checkPureToneADC() {
    // Read ADC value (0-4095 for 12-bit ADC)
    int adcValue = analogRead(PURE_TONE_ADC_PIN);

    // Convert to voltage (0-3.3V range with 11dB attenuation)
    float voltage = (adcValue / 4095.0) * 3.3;

    // Threshold: 2.5V (ADC value ~3100)
    const float THRESHOLD_VOLTAGE = 2.5;

    // State machine for two-tone detection
    static bool toneActive = false;
    static unsigned long toneStartTime = 0;
    static unsigned long firstToneEndTime = 0;
    static bool firstToneDetected = false;
    static unsigned long firstToneDuration = 0;
    static int peakADC = 0;
    static float peakVoltage = 0.0;

    // Debug output every 100ms for responsiveness
    // static unsigned long lastDebug = 0;  // Disabled for performance
    // if (millis() - lastDebug > 100) {
    //     Serial.printf("[PURE-TONE-ADC] Raw=%d Voltage=%.2fV ", adcValue, voltage);
    //     if (voltage >= THRESHOLD_VOLTAGE) {
    //         Serial.println(">>> ACTIVE <<<");
    //     } else {
    //         Serial.println("(idle)");
    //     }
    //     lastDebug = millis();
    // }

    bool currentlyDetected = (voltage >= THRESHOLD_VOLTAGE);

    // Track peak voltage while tone is active
    if (currentlyDetected && toneActive) {
        if (adcValue > peakADC) {
            peakADC = adcValue;
            peakVoltage = voltage;
        }
    }

    // Detect rising edge (tone starts)
    if (currentlyDetected && !toneActive) {
        toneActive = true;
        toneStartTime = millis();
        peakADC = adcValue;
        peakVoltage = voltage;
        Serial.printf("[PURE-TONE] Tone started - ADC=%d V=%.2fV\n", adcValue, voltage);
    }

    // Detect falling edge (tone ends)
    if (!currentlyDetected && toneActive) {
        toneActive = false;
        unsigned long toneDuration = millis() - toneStartTime;

        Serial.printf("[PURE-TONE] Tone ended. Duration=%lums Peak: ADC=%d V=%.2fV\n", toneDuration, peakADC, peakVoltage);

        // Check if duration is valid (500ms - 1000ms)
        if (toneDuration >= 500 && toneDuration <= 1000) {
            Serial.printf("[PURE-TONE-ADC] Valid tone detected! Peak was ADC=%d V=%.2fV\n", peakADC, peakVoltage);
            if (!firstToneDetected) {
                // First valid tone detected
                firstToneDetected = true;
                firstToneEndTime = millis();
                firstToneDuration = toneDuration;
                Serial.printf("[PURE-TONE] FIRST TONE VALID (duration=%lums). Waiting for second tone...\n", toneDuration);
            } else {
                // Second tone detected - check timing
                unsigned long timeBetweenTones = millis() - firstToneEndTime;

                if (timeBetweenTones <= 2000) {
                    // TWO VALID TONES DETECTED!
                    Serial.println("════════════════════════════════════════");
                    Serial.printf("🎵 TWO TONES DETECTED! 🎵\n");
                    Serial.printf("   First tone:  %lums\n", firstToneDuration);
                    Serial.printf("   Second tone: %lums\n", toneDuration);
                    Serial.printf("   Gap between: %lums\n", timeBetweenTones);
                    Serial.println("════════════════════════════════════════");

                    systemStatus.pureToneDetected = true;

                    // Reset for next detection
                    firstToneDetected = false;
                    return true;
                } else {
                    // Too long between tones - reset and treat this as first tone
                    Serial.printf("[PURE-TONE] Gap too long (%lums). Resetting. This tone is now first.\n", timeBetweenTones);
                    firstToneDetected = true;
                    firstToneEndTime = millis();
                    firstToneDuration = toneDuration;
                }
            }
        } else {
            // Invalid duration - reset
            Serial.printf("[PURE-TONE] Invalid duration (%lums). Must be 500-1000ms. Resetting.\n", toneDuration);
            firstToneDetected = false;
        }
    }

    // Timeout: if waiting for second tone for more than 2 seconds, reset
    if (firstToneDetected && (millis() - firstToneEndTime > 2000)) {
        Serial.println("[PURE-TONE] Timeout waiting for second tone. Resetting.");
        firstToneDetected = false;
    }

    return false;
}