/*
 * PROJET MULTIBATTERIE - Carte de gestion batteries lithium
 *
 * Permet de connecter jusqu'à 9 batteries individuelles à un onduleur
 * via communication Modbus (batteries) et CAN Bus (onduleur)
 *
 * Hardware: ESP32 + écran OLED + 4 boutons + interface RS485
 */
#include <Wire.h>
#include <U8g2lib.h>
#include "Config.h"
#include "DisplayManager.h"
#include "MenuManager.h"
#include "ButtonManager.h"
#include "ModbusManager.h"
#include "CanBusManager.h"
#include "NvsManager.h"

// ——————— OBJETS MATÉRIELS ———————
// Instance de l'écran OLED 128x64 I2C
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, OLED_SCL_PIN, OLED_SDA_PIN, OLED_RESET);

// ——————— VARIABLES DE TIMING ———————
unsigned long lastDisplayUpdate = 0; // Timestamps pour contrôler la fréquence des mises à jour
//  Intervalles de mise à jour
#define DISPLAY_UPDATE_INTERVAL 500 // Rafraîchissement écran(ms)

int configuredBatteryCount = 0; // Nombre de batteries configurées par l'utilisateur
// ——————— INITIALISATION SYSTÈME ———————
void setup()
{
  // Initialisation port série pour debug
  Serial.begin(115200);
  Serial.println("=== MULTI-BATTERIE avec CAN ===");
  initNvs();                                   // Initialisation de la NVS
  configuredBatteryCount = loadBatteryCount(); // Chargement du nombre de batteries configurées
  Serial.printf("INFO: %d batterie(s) configurée(s) au démarrage.\n", configuredBatteryCount);

  initDisplay(&u8g2); // Initialisation de l'écran OLED

  // Initialisation des boutons de navigation avec anti-rebond
  initButtons(BTN_UP_PIN, BTN_DOWN_PIN, BTN_OK_PIN, BTN_BACK_PIN);
  setDebounceDelay(DEBOUNCE_DELAY);

  initModbus(&MODBUS_SERIAL); // Initialisation de la communication Modbus RS485 avec les batteries

  // Initialisation du bus CAN pour communication avec l'onduleur
  if (!initCanBus())
  {
    Serial.println("ERREUR: Impossible d'initialiser le CAN!");
    showMessage("ERREUR", "Echec init CAN");
    delay(2000);
  }

  initMenu(); // Initialisation du système de menus

  // Configuration initiale des consignes de courant
  setChargeCurrentSetpoint(10.0);    // 10A charge par défaut
  setDischargeCurrentSetpoint(10.0); // 10A décharge par défaut

  // Message de démarrage
  Serial.println("Système prêt !");
  Serial.println("Consignes variables: 0-600A pour charge/décharge");
  showMessage("SYSTEME", "Pret ! CAN actif");
  delay(1000);
}

// ——————— BOUCLE PRINCIPALE ———————
void loop()
{
  unsigned long now = millis();

  updateButtons(); // Mise à jour de l'état de tous les boutons (anti-rebond inclus)

  handleButtonEvents(); // Traitement des événements boutons

  // Mise à jour de l'affichage à intervalles réguliers
  if (now - lastDisplayUpdate >= DISPLAY_UPDATE_INTERVAL)
  {
    updateMenuDisplay();
    lastDisplayUpdate = now;
  }
}

// ——————— GESTION DES ÉVÉNEMENTS BOUTONS ———————
void handleButtonEvents()
{
  // Navigation vers le haut (menu, code admin, etc.)
  if (isUpPressed())
  {
    navigateMenuUp();
  }

  // Navigation vers le bas
  if (isDownPressed())
  {
    navigateMenuDown();
  }

  // Validation/sélection
  if (isOkPressed())
  {
    selectMenuItem(); // Gère automatiquement le contexte (menu, écran principal, etc.)
  }

  // Retour/annulation
  if (isBackPressed())
  {
    goBackMenu();
  }
}

// ——————— FONCTIONS UTILITAIRES ———————
// Affiche l'état système sur le port série (pour debug)
void printSystemStatus()
{
  Serial.println("\n=== STATUS SYSTÈME ===");
  Serial.printf("Consigne charge: %.1fA\n", getChargeCurrentSetpoint());
  Serial.printf("Consigne décharge: %.1fA\n", getDischargeCurrentSetpoint());
  Serial.printf("Uptime: %lu s\n", millis() / 1000);
  Serial.println("========================\n");
}
