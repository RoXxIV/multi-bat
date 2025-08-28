#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ——————— CONFIGURATION MATÉRIELLE ———————
// Configuration écran OLED I2C
#define OLED_SDA_PIN 21          // Pin de données I2C
#define OLED_SCL_PIN 22          // Pin horloge I2C
#define OLED_RESET U8X8_PIN_NONE // Pin reset materiel (non utilisé)

// Configuration boutons de navigation
#define BTN_UP_PIN 39   // Bouton haut
#define BTN_DOWN_PIN 34 // Bouton bas
#define BTN_OK_PIN 36   // Bouton OK
#define BTN_BACK_PIN 0  // Bouton BACK

// Configuration communication Modbus RS485
#define MODBUS_RX_PIN 16      // Pin réception RS485
#define MODBUS_TX_PIN 17      // Pin transmission RS485
#define MODBUS_DE_RE_PIN 18   // Pin control direction RS485
#define MODBUS_SERIAL Serial2 // Port série utilisé pour Modbus

// ——————— CONFIGURATION SYSTÈME ———————
// Code d'accès admin (3 chiffres)
#define ADMIN_CODE_1 0 // premier chiffre
#define ADMIN_CODE_2 0 // deuxième chiffre
#define ADMIN_CODE_3 0 // troisième chiffre
// Paramètres temporels du système
#define DEBOUNCE_DELAY 50       // Délai anti-rebond boutons (ms)
#define MESSAGE_TIMEOUT 1500    // Durée affichage messages (ms)
#define SCREEN_TIMEOUT_MS 60000 // Durée d'inactivité avant extinction écran (60s)
// Limites de l'interface utilisateur
#define MAX_MENU_ITEMS 10    // Nombre maximum d'items de menu
#define VISIBLE_MENU_ITEMS 4 // Items visibles simultanément

// ——————— CONFIGURATION MODBUS ———————
#define MODBUS_BAUD 9600         // Vitesse de communication Modbus
#define MODBUS_CONFIG SERIAL_8E1 // Format série : 8 bits, parité paire, 1 stop
#define MAX_BATTERIES 9          // Nombre maximum de batteries supportées
#define MASTER_ADDR 0x81         // Adresse Modbus du maître (ESP32)

// ——————— ÉNUMÉRATIONS ———————
// Types de boutons du système
enum ButtonType
{
    BTN_UP = 0,   // Index bouton navigation haut
    BTN_DOWN = 1, // Index bouton navigation bas
    BTN_OK = 2,   // Index bouton validation
    BTN_BACK = 3, // Index bouton retour
    BTN_COUNT = 4 // Nombre total de boutons
};
// Types d'écrans de l'interface
enum ScreenType
{
    SCREEN_MAIN_DATA = 0,     // Écran principal avec données batteries
    SCREEN_MENU = 1,          // Écran de menu principal
    SCREEN_CODE_INPUT = 2,    // Écran de saisie code admin
    SCREEN_CODE_RESULT = 3,   // Écran de résultat validation code
    SCREEN_CAN_FRAMES = 4,    // Écran d'affichage trames CAN
    SCREEN_BRIGHTNESS = 5,    // Écran de réglage de luminosité
    SCREEN_BATTERY_LIST = 6,  // Nouvel écran: liste des batteries
    SCREEN_BATTERY_DETAIL = 7 // Nouvel écran: détails d'une batterie
};
// Actions disponibles dans les menus
enum MenuActions
{
    ACTION_DISPLAY_IDS = 1,     // Afficher IDs des batteries
    ACTION_ERRORS = 2,          // Afficher erreurs système
    ACTION_INDIVIDUAL = 3,      // Données batteries individuelles
    ACTION_ADMIN_CODE = 4,      // Saisie code administrateur
    ACTION_PAIRING = 5,         // Processus d'appairage batteries
    ACTION_SYSTEM_SETTINGS = 6, // Paramètres système (admin)
    ACTION_CAN_FRAMES = 7,      // Affichage trames CAN temps réel
    ACTION_BRIGHTNESS = 8       // Action pour régler la luminosité
};

// ——————— STRUCTURES DE DONNÉES ———————
// Structure d'état d'un bouton physique
struct ButtonState
{
    int pin;                    // Numéro de pin GPIO
    bool currentState;          // État actuel filtré
    bool previousState;         // État précédent (pour détection front)
    unsigned long lastDebounce; // Timestamp dernier changement
};
// Structure d'un item de menu
struct MenuItem
{
    const char *text; // Texte affiché dans le menu
    int action;       // Action à exécuter (enum MenuActions)
    bool isAdminOnly; // Visible uniquement en mode admin
};

#endif