#include "gpio_commands.h"
#include "system_state.h"

// ==================== GPIO SETUP ====================
void setupGPIOCommands() {
    Serial.println("Setting up GPIO command inputs...");
    
    pinMode(CMD_TOUCH_PIN, INPUT_PULLDOWN);
    pinMode(CMD_TONE_PIN, INPUT_PULLDOWN);
    pinMode(CMD_SEND_PIN, INPUT_PULLDOWN);
    
    Serial.println("GPIO command pins initialized:");
    Serial.println("   Touch Command = GPIO 4 (Input)");
    Serial.println("   Pure Tone     = GPIO 2 (Input)");
    Serial.println("   Send Packet   = GPIO 15 (Input)");
}

// ==================== GPIO COMMAND CHECKING ====================
bool checkWiFiCommands() {
    bool commandReceived = false;
    
    // Debug GPIO status every 10 seconds
    static unsigned long lastDebug = 0;
    if (millis() - lastDebug > 10000) {
        Serial.printf("GPIO Status: Touch(4)=%d, Tone(2)=%d, Send(15)=%d\n", 
                     digitalRead(CMD_TOUCH_PIN), 
                     digitalRead(CMD_TONE_PIN), 
                     digitalRead(CMD_SEND_PIN));
        lastDebug = millis();
    }
    
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
    if (digitalRead(CMD_SEND_PIN) == HIGH) {
        systemStatus.manualSendTrigger = true;
        systemStatus.gpioCommandCount++;
        Serial.println("SEND PACKET command received via GPIO 15!");
        commandReceived = true;
        
        // Wait for pin to go low (debounce)
        while (digitalRead(CMD_SEND_PIN) == HIGH) {
            delay(1);
        }
        Serial.println("Send packet pulse completed");
    }
    
    return commandReceived;
}