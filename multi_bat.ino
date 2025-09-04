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
#include "BatteryLogic.h"

// ——————— OBJETS MATÉRIELS ———————
HardwareSerial modbusSerial(2);                                                           // Port serie pour la communication Modbus
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, OLED_SCL_PIN, OLED_SDA_PIN, OLED_RESET); // Instance de l'écran OLED 128x64 I2C
// ——————— VARIABLES DE TIMING ———————
unsigned long lastDisplayUpdate = 0; // Timestamps pour contrôler la fréquence des mises à jour
//  Intervalles de mise à jour
#define DISPLAY_UPDATE_INTERVAL 500                            // Rafraîchissement écran(ms) - 2 fois par seconde
unsigned long lastMetricsUpdate = 0;                           // Timestamps pour contrôler la fréquence des mises à jour
const long METRICS_UPDATE_INTERVAL = 10000;                    // 10 secondes
AggregateBatteryMetrics latestMetrics;                         // Datas de toutes les batteries
IndividualBatteryData individualBatteryMetrics[MAX_BATTERIES]; // Datas individuelles de chaque batterie
unsigned long lastActivityTime = 0;                            // Variable globale pour la gestion de l'inactivité
int configuredBatteryCount = 0;                                // Nombre de batteries configurées par l'utilisateur

// ——————— INITIALISATION SYSTÈME ———————
void setup()
{
  // Initialisation port série pour debug
  Serial.begin(115200);
  Serial.println("=== MULTI-BATTERIE avec CAN ===");
  initNvs();                                   // Initialisation de la NVS
  configuredBatteryCount = loadBatteryCount(); // Chargement du nombre de batteries configurées
  Serial.printf("INFO: %d batterie(s) configurée(s) au démarrage.\n", configuredBatteryCount);
  initDisplay(&u8g2);                               // Initialisation de l'écran OLED
  setBrightness(brightnessValues[brightnessLevel]); // Initialiser la luminosité avec la valeur par défaut
  // Initialisation des boutons de navigation avec anti-rebond
  initButtons(BTN_UP_PIN, BTN_DOWN_PIN, BTN_OK_PIN, BTN_BACK_PIN);
  setDebounceDelay(DEBOUNCE_DELAY);
  initModbus();      // Initialisation de la communication Modbus RS485 avec les batteries
  if (!initCanBus()) // Initialisation du bus CAN
  {
    Serial.println("ERREUR: Impossible d'initialiser le CAN Bus!");
  }
  lastActivityTime = millis(); // Initialiser le timer
  initMenu();                  // Initialisation du système de menus
  initBatteryManagement();     // Initialisation de la gestion des batteries
  initStatusLeds();            // Initialisation des LEDs de statut
  Serial.println("✓ Système de gestion batteries initialisé");

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
  if (configuredBatteryCount > 0)
  {
    Serial.println("Lecture des informations statiques des batteries...");
    for (int i = 0; i < configuredBatteryCount; i++)
    {
      uint8_t currentBatteryId = i + 2;
      updateBatteryStaticInfo(currentBatteryId);
      delay(250); // Pause entre chaque batterie
    }
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
  updateButtons();      // Mise à jour de l'état de tous les boutons (anti-rebond inclus)
  handleButtonEvents(); // Traitement des événements boutons

  // Vérifier l'inactivité pour éteindre l'écran
  if (isScreenOn && (millis() - lastActivityTime > SCREEN_TIMEOUT_MS))
  {
    turnOffDisplay();
  }

  // Mise à jour de l'affichage à intervalles réguliers
  if (isScreenOn && now - lastDisplayUpdate >= DISPLAY_UPDATE_INTERVAL)
  {
    updateMenuDisplay();
    lastDisplayUpdate = now;
  }

  monitorBatteryConnections(); // Surveillance continue des connexions

  // Mise à jour des données globales des batteries
  if (now - lastMetricsUpdate >= METRICS_UPDATE_INTERVAL)
  {
    lastMetricsUpdate = now;
    Serial.println("\n=== CYCLE DE LECTURE BATTERIES ===");
    readAggregateBatteryMetrics(configuredBatteryCount, &latestMetrics); // Lecture des données agrégées

    for (int i = 0; i < configuredBatteryCount; i++)
    {
      uint8_t currentBatteryId = i + 2;
      updateIndividualBatteryMetrics(currentBatteryId);
      // Mise à jour de l'état de la batterie
      // Considérer qu'une batterie répond si ses données sont valides
      bool responding = individualBatteryMetrics[i + 1].isValid;
      updateBatteryState(i, responding);
      // Debug
      printIndividualBatteryData(currentBatteryId);
      delay(250); // Petite pause pour ne pas surcharger le bus
    }
    updateVoltageDeltas();         // Calcul des deltas de tension
    checkMosfetStatus();           // Vérification MOSFETs
    checkDegradedModeConditions(); // Vérification conditions mode dégradé
    updateSystemMetrics();         // Mise à jour des métriques système
    runBatteryManagementCycle();   // Gestion intelligente des batteries

    printSystemStatus(); // Affiche l'état du système avec les consignes déjà calculées.
  }
  sendCanData();
}

// ——————— GESTION DES ÉVÉNEMENTS BOUTONS ———————
void handleButtonEvents()
{

  // Si un bouton est pressé
  if (isUpPressed() || isDownPressed() || isOkPressed() || isBackPressed())
  {
    lastActivityTime = millis(); // Réinitialiser le timer d'activité

    // Si l'écran est éteint, le premier appui ne fait que le rallumer
    if (!isScreenOn)
    {
      turnOnDisplay();
      return; // Sortir pour ne pas traiter l'action du bouton immédiatement
    }

    // Sinon, traiter l'action du bouton normalement
    if (isUpPressed())
      navigateMenuUp();
    if (isDownPressed())
      navigateMenuDown();
    if (isOkPressed())
      selectMenuItem();
    if (isBackPressed())
      goBackMenu();
  }
}
