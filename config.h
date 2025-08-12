#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ——————— CONFIGURATION HARDWARE ———————
// Pins OLED I2C
#define OLED_SDA_PIN 21
#define OLED_SCL_PIN 22
#define OLED_RESET U8X8_PIN_NONE

// Pins des boutons
#define BTN_UP_PIN 39
#define BTN_DOWN_PIN 34
#define BTN_OK_PIN 36
#define BTN_BACK_PIN 0

// ——————— CONFIGURATION SYSTÈME ———————
// Code d'accès admin (3 chiffres)
#define ADMIN_CODE_1 6
#define ADMIN_CODE_2 6
#define ADMIN_CODE_3 6

// Timing
#define DEBOUNCE_DELAY 50    // ms
#define MESSAGE_TIMEOUT 2000 // ms

// Limites
#define MAX_MENU_ITEMS 10

// ——————— FONCTIONS UTILITAIRES ———————
inline void printSystemInfo()
{
    Serial.printf("Multi-Batterie v1.0 - RAM: %d bytes\n", ESP.getFreeHeap());
}

#endif