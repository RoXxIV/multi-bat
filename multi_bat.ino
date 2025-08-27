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

unsigned long lastMetricsUpdate = 0;
const long METRICS_UPDATE_INTERVAL = 10000; // 10 secondes
AggregateBatteryMetrics latestMetrics;

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

  initMenu(); // Initialisation du système de menus

  // Si aucune batterie n'est configurée (premier démarrage),
  // on lance directement le processus d'appairage.
  if (configuredBatteryCount == 0)
  {
    Serial.println("INFO: Aucune batterie configurée. Lancement de l'appairage initial...");
    showMessage("BIENVENUE", "Config. initiale");
    delay(2000);
    actionPairing(); // On appelle directement la fonction d'appairage

    // Après l'appairage, on recharge le nombre de batteries pour être à jour
    configuredBatteryCount = loadBatteryCount();
    Serial.printf("INFO: Appairage terminé. %d batterie(s) maintenant configurée(s).\n", configuredBatteryCount);
  }

  // Message de démarrage
  Serial.println("Système prêt !");
  showMessage("BIENVENUE", "Démarrage");
  delay(1500);
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

  // Mise à jour des données globales des batteries
  if (now - lastMetricsUpdate >= METRICS_UPDATE_INTERVAL)
  {
    lastMetricsUpdate = now;
    // On appelle la nouvelle fonction avec le nombre de batteries sauvegardé en NVS
    readAggregateBatteryMetrics(configuredBatteryCount, &latestMetrics);

    // Vous pouvez maintenant utiliser `latestMetrics.averageSoc`, etc.
    // pour mettre à jour l'écran ou envoyer des trames CAN.
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
