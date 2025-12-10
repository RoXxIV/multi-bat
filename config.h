#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ——————— CONFIGURATION MATÉRIELLE ———————
// Configuration écran OLED I2C
#define OLED_SDA_PIN 21          // Pin de données I2C
#define OLED_SCL_PIN 22          // Pin horloge I2C
#define OLED_RESET U8X8_PIN_NONE // Pin reset materiel (non utilisé)

// Pins des LEDs d'état
#define LED_RED_PIN 26    // LED rouge alarme
#define LED_YELLOW_PIN 27 // LED jaune à definir

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
#define DEBOUNCE_DELAY 50          // Délai anti-rebond boutons (ms)
#define MESSAGE_TIMEOUT 1500       // Durée affichage messages (ms)
#define SCREEN_TIMEOUT_MS 300000   // Durée d'inactivité avant extinction écran (5min)
#define MAX_CONSECUTIVE_FAILURES 3 // NOUVELLE LIGNE : Nombre d'échecs de lecture avant déconnexion
// Limites de l'interface utilisateur
#define MAX_MENU_ITEMS 10    // Nombre maximum d'items de menu
#define VISIBLE_MENU_ITEMS 4 // Items visibles simultanément

// ——————— CONFIGURATION MODBUS ———————
#define MODBUS_BAUD 9600         // Vitesse de communication Modbus
#define MODBUS_CONFIG SERIAL_8E1 // Format série : 8 bits, parité paire, 1 stop
#define MAX_BATTERIES 9          // Nombre maximum de batteries supportées
#define MASTER_ADDR 0x81         // Adresse Modbus du maître (ESP32)

// ——————— SEUILS DE GESTION BATTERIES ———————
// Seuils de tension des batteries
#define VOLTAGE_DIFF_THRESHOLD 0.4f        // Seuil différence tension batteries (V)
#define VOLTAGE_DIFF_COUPLE_THRESHOLD 0.3f // Seuil pour couplage batteries (V)
#define VOLTAGE_TOLERANCE 0.3f             // Tolérance tension pour MOSFETs (V)

// Seuils de température
#define TEMP_DIFF_THRESHOLD 10.0f // Seuil différence température batteries (°C)
#define MIN_CHARGE_TEMP 8.0f      // Température min pour charge normale (°C)
#define MAX_CHARGE_TEMP 45.0f     // Température max pour charge normale (°C)
#define MIN_DISCHARGE_TEMP 3.0f   // Température min pour décharge normale (°C)
#define MAX_DISCHARGE_TEMP 50.0f  // Température max pour décharge normale (°C)
#define MAX_MOS_TEMP 80.0f        // Température max MOSFETs (°C)

// Seuils de tension des cellules
#define MAX_CHARGE_CELL_VOLTAGE 3.480f    // Tension max cellule - arrêt charge (V)
#define HIGH_CHARGE_CELL_VOLTAGE 3.450f   // Tension haute cellule - limitation charge (V)
#define LOW_CELL_VOLTAGE 3.000f           // Tension basse cellule - limitation charge/décharge (V)
#define MIN_DISCHARGE_CELL_VOLTAGE 2.750f // Tension min cellule - arrêt décharge (V)
#define CELL_DIFF_THRESHOLD 0.250f        // Seuil différence tension cellules - mode dégradé (V)

// Consignes de courant
#define DEGRADED_MODE_CURRENT 10.0f       // Courant en mode dégradé (A)
#define NORMAL_CURRENT_PER_BATTERY 150.0f // Courant normal par batterie (A)
#define LIMITED_CURRENT_PER_BATTERY 10.0f // Courant limité par batterie (A)

// Timeouts et temporisations
#define BATTERY_RESPONSE_TIMEOUT 5000        // Timeout réponse batterie (ms)
#define MOSFET_RECOVERY_CHECK_INTERVAL 30000 // Intervalle vérification récupération MOSFET (ms)

#define REG_RATED_CAPACITY_START 0x0109  // Adresse du premier des 2 registres pour la capacité nominale
#define REG_ACTUAL_CAPACITY_START 0x010B // Adresse du premier des 2 registres pour la capacité réelle

extern int configuredBatteryCount; // Nombre de batteries configurées par l'utilisateur

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
  SCREEN_MAIN_DATA = 0,      // Écran principal avec données batteries
  SCREEN_MENU = 1,           // Écran de menu principal
  SCREEN_CODE_INPUT = 2,     // Écran de saisie code admin
  SCREEN_CODE_RESULT = 3,    // Écran de résultat validation code
  SCREEN_BRIGHTNESS = 4,     // Écran de réglage de luminosité
  SCREEN_BATTERY_LIST = 5,   // Nouvel écran: liste des batteries
  SCREEN_BATTERY_DETAIL = 6, // Nouvel écran: détails d'une batterie
  SCREEN_ERROR_LIST = 7,     // Écran des erreurs système
  SCREEN_OTA = 8,            // mise à jour OTA
  SCREEN_VERSION = 9,        // Version du firmware
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
  ACTION_BRIGHTNESS = 7,      // Action pour régler la luminosité
  ACTION_OTA_UPDATE = 8,      // Mise à jour OTA
  ACTION_VERSION = 9          // Afficher la version
};

enum LimitationType
{
  NO_LIMITATION = 0,           // Aucune limitation
  MOSFET_LIMITATION = 1,       // Limitation due aux MOSFETs
  VOLTAGE_LIMITATION = 2,      // Limitation due aux tensions
  TEMP_LIMITATION = 3,         // Limitation due aux températures
  DEGRADED_MODE_LIMITATION = 4 // Limitation mode dégradé
};

enum BatteryStatus
{
  BATTERY_OK = 0,             // Batterie fonctionnelle
  BATTERY_NOT_RESPONDING = 1, // Batterie ne répond pas
  BATTERY_MOSFET_ISSUE = 2,   // Problème MOSFET
  BATTERY_TEMP_ISSUE = 3,     // Problème température
  BATTERY_VOLTAGE_ISSUE = 4,  // Problème tension
  BATTERY_CELL_IMBALANCE = 5, // Déséquilibre cellules
  BATTERY_BMS_FAULT = 6       // Erreur interne du BMS
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

// ——————— STRUCTURES POUR LA GESTION DES BATTERIES ———————
// Structure d'état d'une batterie individuelle
struct BatteryState
{
  bool isResponding;               // Batterie répond aux commandes Modbus
  unsigned long lastResponseTime;  // Timestamp dernière réponse valide
  BatteryStatus status;            // État actuel de la batterie
  LimitationType activeLimitation; // Type de limitation active
  unsigned long lastMosfetCheck;   // Timestamp dernière vérification MOSFET
  int consecutiveFailures;         // Nombre de fois ou la batterie a echoué
};

// Structure de diagnostic système global
struct SystemDiagnostics
{
  bool degradedMode;            // Mode dégradé actif
  int activeBatteryCount;       // Nombre de batteries actives
  int respondingBatteryCount;   // Nombre de batteries qui répondent
  float maxBatteryVoltage;      // Tension max parmi toutes les batteries
  float minBatteryVoltage;      // Tension min parmi toutes les batteries
  float maxBatteryTemp;         // Température max parmi toutes les batteries
  float minBatteryTemp;         // Température min parmi toutes les batteries
  unsigned long lastDiagnostic; // Timestamp dernier diagnostic
};

// ——————— STRUCTURES POUR LA GESTION DES ERREURS ———————

// Types d'erreurs système
enum ErrorType
{
  ERROR_BATTERY_DISCONNECTED = 1,
  ERROR_MOSFET_CHARGE = 2,
  ERROR_MOSFET_DISCHARGE = 3,
  ERROR_CELL_IMBALANCE = 4,
  ERROR_OVERTEMPERATURE = 5,
  ERROR_UNDERTEMPERATURE = 6,
  ERROR_OVERVOLTAGE = 7,
  ERROR_UNDERVOLTAGE = 8,
  ERROR_CAN_BUS = 9,
  ERROR_WAKEUP_BUTTON_OFF = 10,
  ERROR_MOSFET_OVERTEMPERATURE = 11,
  ERROR_SHORT_CIRCUIT = 12,
  ERROR_OVERCURRENT_CHARGE = 13,
  ERROR_OVERCURRENT_DISCHARGE = 14,
  ERROR_CHARGE_PROHIBITED = 15,
  ERROR_DISCHARGE_PROHIBITED = 16,
  ERROR_BMS_INTERNAL_FAULT = 17
};

// Structure d'une erreur
struct SystemError
{
  bool active;                 // Erreur active ou non
  ErrorType type;              // Type d'erreur
  int batteryId;               // ID batterie concernée (-1 si système global)
  unsigned long firstOccurred; // Timestamp première occurrence
  unsigned long lastOccurred;  // Timestamp dernière occurrence
  int occurrenceCount;         // Nombre d'occurrences
  char description[50];        // Description courte
};

#define MAX_SYSTEM_ERRORS 20 // Nombre maximum d'erreurs stockées

#endif