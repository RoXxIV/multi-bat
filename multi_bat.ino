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

// ——————— Objets materiels ———————
HardwareSerial modbusSerial(2);                                                           // Port serie pour la communication Modbus
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, OLED_SCL_PIN, OLED_SDA_PIN, OLED_RESET); // Instance de l'écran OLED 128x64 I2C
// ——————— Variable de timing  ———————
unsigned long lastDisplayUpdate = 0;
#define DISPLAY_UPDATE_INTERVAL 500         // Rafraîchissement écran(ms) - 2 fois par seconde
unsigned long lastMetricsUpdate = 0;        // Timestamps pour contrôler la fréquence des mises à jour
const long METRICS_UPDATE_INTERVAL = 10000; // 10 secondes
unsigned long lastActivityTime = 0;         // Variable globale pour la gestion de l'inactivité
int configuredBatteryCount = 0;             // Nombre de batteries configurées par l'utilisateur
// --- Variables pour la séquence de lecture non-bloquante ---
bool isReadingSequenceActive = false;   // Drapeau indiquant si une séquence de lecture est en cours
int batteryReadIndex = 0;               // Index de la prochaine batterie à lire
unsigned long lastBatteryReadTime = 0;  // Timestamp de la dernière lecture
const long BATTERY_READ_INTERVAL = 250; // Intervalle de 250ms entre chaque lecture
// --- Structures de données globales ---
AggregateBatteryMetrics latestMetrics;                         // Datas de toutes les batteries
IndividualBatteryData individualBatteryMetrics[MAX_BATTERIES]; // Datas individuelles de chaque batterie

// ——————— INITIALISATION SYSTÈME ———————
void setup()
{

  Serial.begin(115200); // Initialisation port série pour debug

  // Initialisation de la NVS et Chargement du nombre de batteries configurées
  initNvs();
  configuredBatteryCount = loadBatteryCount();
  Serial.printf("INFO: %d batterie(s) configurée(s) au démarrage.\n", configuredBatteryCount);

  // Initialisation de l'écran OLED et des boutons
  initDisplay(&u8g2);
  setBrightness(brightnessValues[brightnessLevel]);
  initButtons(BTN_UP_PIN, BTN_DOWN_PIN, BTN_OK_PIN, BTN_BACK_PIN);
  setDebounceDelay(DEBOUNCE_DELAY);

  // Initialisation de la communication Modbus RS485 avec les batteries et du bus CAN
  initModbus();
  if (!initCanBus())
  {
    Serial.println("ERREUR: Impossible d'initialiser le CAN Bus!");
  }

  lastActivityTime = millis(); // Initialiser le timer
  initMenu();                  // Initialisation du système de menus
  initBatteryManagement();     // Initialisation de la gestion des batteries
  initStatusLeds();            // Initialisation des LEDs de statut
  Serial.println("✓ Système de gestion batteries initialisé");

  /*
   *Si aucune batterie n'est configurée (premier démarrage),
   *on lance directement le processus d'appairage.
   */
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

  // Mise à jour des infos statiques des batteries
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

  // Vérifier l'inactivité pour éteindre l'écran et Mise à jour de l'affichage à intervalles réguliers
  if (isScreenOn && (millis() - lastActivityTime > SCREEN_TIMEOUT_MS))
  {
    turnOffDisplay();
  }
  if (isScreenOn && now - lastDisplayUpdate >= DISPLAY_UPDATE_INTERVAL)
  {
    updateMenuDisplay();
    lastDisplayUpdate = now;
  }

  // --- LOGIQUE DE LECTURE ET D'ANALYSE ---
  // 1. Déclencheur : Toutes les 10 secondes, on LANCE une nouvelle séquence de lecture.
  if (now - lastMetricsUpdate >= METRICS_UPDATE_INTERVAL)
  {
    // On ne lance une nouvelle séquence que si la précédente est bien terminée.
    if (!isReadingSequenceActive && configuredBatteryCount > 0)
    {
      lastMetricsUpdate = now;                           // Réinitialiser le timer de 10 secondes
      isReadingSequenceActive = true;                    // Lever le drapeau pour démarrer la séquence
      batteryReadIndex = 0;                              // Commencer par la première batterie
      lastBatteryReadTime = now - BATTERY_READ_INTERVAL; // Astuce pour lire la 1ère batterie immédiatement
      Serial.println("\n>>> DÉCLENCHEMENT NOUVELLE SÉQUENCE DE LECTURE (toutes les 10s) <<<");
    }
  }

  // 2. Exécuteur : À chaque tour de boucle, on vérifie l'état de la séquence en cours.
  handleBatteryReadSequence();

  // 3. Tâches continues
  monitorBatteryConnections();
  sendCanData();
}

// gestion des boutons et des événements liés
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

void handleBatteryReadSequence()
{
  // Si aucune séquence de lecture n'est active, on ne fait rien.
  if (!isReadingSequenceActive)
  {
    return;
  }

  unsigned long now = millis();

  // Est-il temps de lire la prochaine batterie de la séquence ?
  if (now - lastBatteryReadTime >= BATTERY_READ_INTERVAL)
  {
    lastBatteryReadTime = now; // Réinitialiser le timer pour la prochaine

    // --- Lecture d'UNE SEULE batterie ---
    uint8_t currentBatteryId = batteryReadIndex + 2;
    updateIndividualBatteryMetrics(currentBatteryId);

    bool responding = individualBatteryMetrics[batteryReadIndex + 1].isValid;
    updateBatteryState(batteryReadIndex, responding);

    printIndividualBatteryData(currentBatteryId); // Pour le debug

    // On passe à la batterie suivante
    batteryReadIndex++;

    // --- Avons-nous terminé de lire toutes les batteries ? ---
    if (batteryReadIndex >= configuredBatteryCount)
    {
      // OUI : La séquence est terminée. On peut lancer l'analyse.
      Serial.println("\n=== SÉQUENCE DE LECTURE TERMINÉE - DÉBUT ANALYSE ===");

      updateSystemMetrics();
      checkDegradedModeConditions();
      runBatteryManagementCycle();
      printSystemStatus();

      // On baisse le drapeau pour signaler que la séquence est finie.
      isReadingSequenceActive = false;
    }
    // NON : La fonction se termine, et on attendra 250ms pour lire la suivante.
  }
}
// ==========================================================================
// ================== FONCTION DE TEST MODBUS RAPIDE ========================
// ==========================================================================
/**
 * @brief Lit une seule adresse de registre Modbus sur une batterie et affiche le résultat.
 * @param batteryId L'ID de la batterie à interroger (ex: 2, 3, etc.).
 * @param registerAddress L'adresse du registre en DÉCIMAL (ex: 1173).
 */
void testReadModbusRegister(uint8_t batteryId, uint16_t registerAddress)
{
  uint8_t data_payload[2]; // Un registre fait 2 octets

  Serial.printf("\n--- TEST MODBUS: Lecture du registre %d (0x%04X) sur la batterie ID %d ---\n",
                registerAddress, registerAddress, batteryId);

  // On utilise la fonction bas-niveau de ModbusLib pour lire 1 seul registre
  int bytesRead = modbus_read_registers(
      batteryId,
      registerAddress,
      1, // On ne veut lire qu'un seul registre
      data_payload);

  if (bytesRead == 2) // On s'attend à recevoir 2 octets
  {
    // Combinaison des deux octets pour former une valeur de 16 bits
    uint16_t value = (data_payload[0] << 8) | data_payload[1];

    Serial.printf("✓ SUCCÈS ! Réponse reçue.\n");
    Serial.printf("  - Brute (Hex): %02X %02X\n", data_payload[0], data_payload[1]);
    Serial.printf("  - Valeur (Dec): %u\n", value);
    Serial.printf("  - Valeur (Hex): 0x%04X\n", value);
    // Hypothèse : si le SOH est un pourcentage, il est peut-être divisé par 10
    Serial.printf("  - Hypothèse SOH (valeur / 10): %.1f %%\n", (float)value / 10.0f);
  }
  else
  {
    // Le code de retour est -1, ce qui correspond au timeout dans la librairie
    if (bytesRead == -1)
    {
      Serial.println("✗ ÉCHEC ! Raison: Timeout. La batterie n'a pas repondu.");
      Serial.println("  Pistes d'analyse :");
      Serial.println("    1. L'adresse de registre est tres probablement incorrecte ou non lisible.");
      Serial.println("    2. L'ID de la batterie est-il bien celui attendu pour ce test ?");
      Serial.println("    3. Avez-vous essaye des adresses proches (ex: 1172, 1174) ?");
    }
    else
    {
      Serial.printf("✗ ÉCHEC ! Raison: Erreur inattendue (code de retour: %d).\n", bytesRead);
    }
  }
  Serial.println("--- FIN DU TEST MODBUS ---\n");
}
// ==========================================================================