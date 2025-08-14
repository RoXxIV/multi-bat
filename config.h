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

// Pins Modbus (Serial2 sur ESP32)
#define MODBUS_RX_PIN 16
#define MODBUS_TX_PIN 17
#define MODBUS_SERIAL Serial2

// ——————— CONFIGURATION SYSTÈME ———————
// Code d'accès admin (3 chiffres)
#define ADMIN_CODE_1 0
#define ADMIN_CODE_2 0
#define ADMIN_CODE_3 0

// Timing
#define DEBOUNCE_DELAY 50    // ms
#define MESSAGE_TIMEOUT 2000 // ms

// Limites
#define MAX_MENU_ITEMS 10    // vvv
#define VISIBLE_MENU_ITEMS 4 // vvv

// ——————— ÉNUMÉRATIONS ———————
enum ButtonType
{
    BTN_UP = 0,
    BTN_DOWN = 1,
    BTN_OK = 2,
    BTN_BACK = 3,
    BTN_COUNT = 4
};

enum ScreenType
{
    SCREEN_MAIN_DATA = 0,
    SCREEN_MENU,
    SCREEN_CODE_INPUT,
    SCREEN_CODE_RESULT
};

enum MenuActions
{
    ACTION_DISPLAY_IDS = 1,
    ACTION_ERRORS = 2,
    ACTION_INDIVIDUAL = 3,
    ACTION_ADMIN_CODE = 4,
    ACTION_PAIRING = 5,
    ACTION_SYSTEM_SETTINGS = 6
};

// ——————— STRUCTURES ———————
struct ButtonState
{
    int pin;
    bool currentState;
    bool previousState;
    unsigned long lastDebounce;
};

struct MenuItem
{
    const char *text;
    int action;
    bool isAdminOnly;
};

// ——————— CONFIGURATION MODBUS ———————
#define MODBUS_BAUD 9600
#define MODBUS_CONFIG SERIAL_8N1
#define MAX_BATTERIES 9
#define MASTER_ADDR 0x81 // vvv

// ——————— FONCTIONS UTILITAIRES ———————
inline void printSystemInfo()
{
    Serial.printf("Multi-Batterie v3.0 - RAM: %d bytes\n", ESP.getFreeHeap());
}

#endif