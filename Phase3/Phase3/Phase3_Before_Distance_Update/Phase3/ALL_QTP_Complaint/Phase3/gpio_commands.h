#ifndef GPIO_COMMANDS_H
#define GPIO_COMMANDS_H

#include <Arduino.h>

// ==================== GPIO PIN DEFINITIONS ====================
// GPIO Command pins (inputs from WiFi ESP32)
#define CMD_TOUCH_PIN     4   // Input from WiFi ESP32 GPIO 4
#define CMD_TONE_PIN      2   // Input from WiFi ESP32 GPIO 2
#define CMD_SEND_PIN      15  // Input from WiFi ESP32 GPIO 15

// ==================== GPIO FUNCTIONS ====================
/**
 * Initialize GPIO pins for command inputs
 * Sets up pins as INPUT_PULLDOWN and prints initialization status
 */
void setupGPIOCommands();

/**
 * Check for WiFi commands from GPIO pins
 * Monitors all three command pins and updates system status
 * @return: true if any command was received
 */
bool checkWiFiCommands();

#endif // GPIO_COMMANDS_H