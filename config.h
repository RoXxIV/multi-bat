#ifndef CONFIG_H
#define CONFIG_H

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
#define SCREEN_TIMEOUT 60000 // ms (extinction écran après 1 minute)

// Limites
#define MAX_MENU_ITEMS 10
#define MAX_ADMIN_ITEMS 5

// ——————— TEXTES DES MENUS ———————
// Menu principal de base (sans admin)
#define MENU_BASE_COUNT 5
extern const char *BASE_MENU_ITEMS[MENU_BASE_COUNT];

// Items admin supplémentaires
#define MENU_ADMIN_COUNT 1
extern const char *ADMIN_MENU_ITEMS[MENU_ADMIN_COUNT];

// Messages système
extern const char *MSG_STARTUP_TITLE;
extern const char *MSG_STARTUP_TEXT;
extern const char *MSG_CODE_TITLE;
extern const char *MSG_CODE_PROMPT;
extern const char *MSG_CODE_SUCCESS;
extern const char *MSG_CODE_ERROR;
extern const char *MSG_ADMIN_ACTIVATED;

// ——————— FONCTIONS D'INITIALISATION ———————
void initializeConfig();
void printSystemInfo();

#endif