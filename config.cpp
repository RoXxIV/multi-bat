#include "Config.h"
#include <Arduino.h>

// ——————— DÉFINITION DES TEXTES ———————
// Menu principal de base (4 items)
const char *BASE_MENU_ITEMS[MENU_BASE_COUNT] = {
    "Afficher ID batteries",
    "Effectuer appairage",
    "Affichage erreurs",
    "Batteries individuelles",
    "Mode admin"};

// Items admin (3 items supplémentaires)
const char *ADMIN_MENU_ITEMS[MENU_ADMIN_COUNT] = {
    "Parametres systeme" // Configuration avancée
};

// Messages système
const char *MSG_STARTUP_TITLE = "DEMARRAGE";
const char *MSG_STARTUP_TEXT = "Multi-Batterie v2.2";
const char *MSG_CODE_TITLE = "CODE ADMIN";
const char *MSG_CODE_PROMPT = "Entrez code (3 chiffres)";
const char *MSG_CODE_SUCCESS = "CODE VALIDE";
const char *MSG_CODE_ERROR = "CODE INCORRECT";
const char *MSG_ADMIN_ACTIVATED = "Mode Admin Active";

// ——————— FONCTIONS ———————
void initializeConfig()
{
    Serial.println("=== CONFIGURATION SYSTÈME ===");
    Serial.printf("Code admin configuré: %d%d%d\n", ADMIN_CODE_1, ADMIN_CODE_2, ADMIN_CODE_3);
    Serial.printf("Menu de base: %d items\n", MENU_BASE_COUNT);
    Serial.printf("Menu admin: %d items supplémentaires\n", MENU_ADMIN_COUNT);
    Serial.printf("Timeout écran: %d secondes\n", SCREEN_TIMEOUT / 1000);
    Serial.println("===============================");
}

void printSystemInfo()
{
    Serial.println("\n=== INFORMATIONS SYSTÈME ===");
    Serial.printf("Version: Multi-Batterie v2.2\n");
    Serial.printf("RAM libre: %d bytes\n", ESP.getFreeHeap());
    Serial.printf("Flash: %d bytes\n", ESP.getFlashChipSize());
    Serial.printf("CPU: %d MHz\n", ESP.getCpuFreqMHz());
    Serial.println("==============================\n");
}