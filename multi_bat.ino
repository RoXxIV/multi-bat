#include <Wire.h>
#include <U8g2lib.h>
#include <esp_task_wdt.h>
#include "Config.h"
#include "DisplayManager.h"
#include "MenuManager.h"
#include "ButtonManager.h"
#include "ModbusManager.h"
#include "CanBusManager.h"
#include "NvsManager.h"
#include "BatteryLogic.h"
#include "OtaManager.h"
#include "SlaveManager.h"

// ——————— Objets materiels ———————
HardwareSerial modbusSerial(2);
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, OLED_SCL_PIN, OLED_SDA_PIN, OLED_RESET);
// ------- Watchdog -------
#define WDT_TIMEOUT 30
// ——————— Variable de timing  ———————
unsigned long lastDisplayUpdate = 0;
#define DISPLAY_UPDATE_INTERVAL 500
unsigned long lastActivityTime = 0;
unsigned long lastCanSendTime = 0;

// --- séquence de lecture ---
int batteryReadIndex = 0;        // Index de la batterie à lire (0 à configuredBatteryCount-1)
int currentReadStep = 0;         // Étape de lecture pour la batterie en cours
const int TOTAL_READ_STEPS = 18; // lectures individuelles

// --- Structures de données globales ---
AggregateBatteryMetrics latestMetrics;
IndividualBatteryData individualBatteryMetrics[MAX_BATTERIES];
int configuredBatteryCount = 0;

/**
 * @brief Tâches à exécuter pendant les temps morts de Modbus.
 * Gère la lecture et le traitement des boutons.
 */
void background_button_tasks()
{
  updateButtons();
  handleButtonEvents();
}

extern float currentChargeSetpoint;
extern float currentDischargeSetpoint;

// ——————— INITIALISATION SYSTÈME ———————
void setup()
{
  Serial.begin(115200);

  initNvs(); // Chargement du nombre de batteries configurées
  configuredBatteryCount = loadBatteryCount();

  // Initialisation de l'écran OLED et des boutons
  initDisplay(&u8g2);
  setBrightness(brightnessValues[brightnessLevel]);
  initButtons(BTN_UP_PIN, BTN_DOWN_PIN, BTN_OK_PIN, BTN_BACK_PIN);
  setDebounceDelay(DEBOUNCE_DELAY);

  // Initialisation Modbus RS485 CAN
  initModbus();
  initSlaveModbus(); // Initialise le port esclave sur GPIO 1 / 3
  // Reset affichage ID (MODBUS)
  if (configuredBatteryCount > 0)
  {
    const uint8_t ASCII_4_VALUE = 0x34; // '4'
    for (int i = 0; i < configuredBatteryCount; i++)
    {
      uint8_t currentBatteryId = i + 2;
      sendDisplayIdToBattery(currentBatteryId, ASCII_4_VALUE);
      delay(50);
    }
  }

  if (!initCanBus())
  {
    // On gere ici l'échec d'init CAN si nécessaire
  }

  // Consigne CAN à 0A, Envoi de 5 trames 0A
  currentChargeSetpoint = 0.0f;
  currentDischargeSetpoint = 0.0f;
  for (int i = 0; i < 5; i++)
  {
    sendSafeLimitsZero();
    delay(50);
  }

  modbus_set_idle_callback(background_button_tasks);

  lastActivityTime = millis();
  initMenu();
  initBatteryManagement();
  initStatusLeds();
  initOTA();
  // Logique de premier démarrage
  if (configuredBatteryCount == 0)
  {
    showMessage("BIENVENUE", "Config. initiale");
    delay(2000);
    actionPairing();
    configuredBatteryCount = loadBatteryCount();
  }

  // Mise à jour des infos statiques des batteries
  if (configuredBatteryCount > 0)
  {
    for (int i = 0; i < configuredBatteryCount; i++)
    {
      uint8_t currentBatteryId = i + 2;
      updateBatteryStaticInfo(currentBatteryId);
      delay(250);
    }
  }

  showMessage("BIENVENUE", "Demarrage");
  delay(1500);

  // ——————— INITIALISATION WATCHDOG ———————
  // 1. Initialise avec un timeout de 30s et 'true' pour reset automatique
  esp_task_wdt_init(WDT_TIMEOUT, true);
  // 2. Ajoute la tâche courante (le thread principal Arduino) au watchdog
  esp_task_wdt_add(NULL);
}

// ——————— BOUCLE PRINCIPALE ———————
void loop()
{
  // ——————— RESET WATCHDOG  ———————
  esp_task_wdt_reset();

  unsigned long now = millis();

  updateButtons();         // GESTION DES BOUTONS
  handleButtonEvents();    // Traite les fronts montants qui ont été détectés
  handleAutoErrorScreen(); //  GESTION DES ERREURS
  handleOTAProcess();      // GESTION OTA
  //  GESTION DE L'AFFICHAGE
  if (isScreenOn && (millis() - lastActivityTime > SCREEN_TIMEOUT_MS))
  {
    turnOffDisplay();
    deactivateAdminMode();
  }
  if (isScreenOn && now - lastDisplayUpdate >= DISPLAY_UPDATE_INTERVAL)
  {
    updateMenuDisplay();
    lastDisplayUpdate = now;
  }

  handleBatteryReadSequence(); // LOGIQUE DE LECTURE, ANALYSE ET ENVOI CAN
  monitorBatteryConnections(); // SURVEILLANCE DES CONNEXIONS

  handleSlaveModbus(); // Vérifie et répond aux requêtes du boitier monabee
}

void handleButtonEvents()
{
  if (isButtonPressed(BTN_UP) || isButtonPressed(BTN_DOWN) || isButtonPressed(BTN_OK) || isButtonPressed(BTN_BACK))
  {
    lastActivityTime = millis();
    if (!isScreenOn)
    { //
      turnOnDisplay();

      // Forcer la mise à jour immédiate à l'allumage
      updateMenuDisplay();
      lastDisplayUpdate = millis();
      return;
    }

    if (isButtonPressed(BTN_UP))
      navigateMenuUp();
    if (isButtonPressed(BTN_DOWN))
      navigateMenuDown();
    if (isButtonPressed(BTN_OK))
      selectMenuItem();
    if (isButtonPressed(BTN_BACK))
      goBackMenu();
    //

    updateMenuDisplay();          // Forcer la mise à jour immédiate après action
    lastDisplayUpdate = millis(); // Réinitialiser le timer
  }
}

// ——————— LECTURE SÉQUENTIELLE ———————
/**
 * @brief Gère la lecture pas-à-pas de chaque batterie.
 * Lit UNE valeur à la fois pour UNE batterie à la fois.
 * La gestion des boutons est effectuée PENDANT la lecture Modbus.
 */
void handleBatteryReadSequence()
{
  // S'il n'y a pas de batteries, on ne fait rien
  if (configuredBatteryCount == 0)
  {
    return;
  }

  uint8_t currentBatteryId = batteryReadIndex + 2; // ID de la batterie en cours de lecture (commence à 2)
  IndividualBatteryData *data = &individualBatteryMetrics[batteryReadIndex + 1];

  // fonction de lecture pour l'étape en cours
  bool readOk = processNextModbusRead(currentBatteryId, currentReadStep);

  if (!readOk)
  {
    // La lecture a échoué
    batteryStates[batteryReadIndex].consecutiveFailures++;
    data->isValid = false; // Marquer comme invalide pour ce cycle

    // On passe à la batterie suivante
    currentReadStep = 0;
    batteryReadIndex++;
  }
  else if (currentReadStep == TOTAL_READ_STEPS)
  {
    // SÉQUENCE TERMINÉE POUR CETTE BATTERIE
    data->isValid = true; //
    batteryStates[batteryReadIndex].consecutiveFailures = 0;
    batteryStates[batteryReadIndex].lastResponseTime = millis();

    // --- ANALYSE ET ENVOI CAN IMMÉDIAT ---
    updateSystemMetrics();
    runBatteryManagementCycle();
    if (latestMetrics.isDataValid)
    {
      sendCanData();
    }
    // On passe à la batterie suivante
    currentReadStep = 0; //
    batteryReadIndex++;
  }
  // A-t-on fini de lire toutes les batteries ?
  if (batteryReadIndex >= configuredBatteryCount)
  {
    // Réinitialiser pour la prochaine boucle
    batteryReadIndex = 0;
    currentReadStep = 0;
  }
}