#include "BatteryLogic.h"

BatteryState batteryStates[MAX_BATTERIES];
SystemDiagnostics systemDiag;
float currentChargeSetpoint = 0.0f;
float currentDischargeSetpoint = 0.0f;
int respondingBatteryCount = 0;
bool degradedMode = false;
bool systemPowerButtonOn = true;
bool globalChargeMosfetOk = true;
bool globalDischargeMosfetOk = true;
int turnedOffBatteryId = 0;
SystemError systemErrors[MAX_SYSTEM_ERRORS];
int errorCount = 0;

void initBatteryManagement()
{
  turnedOffBatteryId = 0;
  // Initialisation des états des batteries
  for (int i = 0; i < MAX_BATTERIES; i++)
  {
    // logique individuelle
    batteryStates[i].isResponding = false;
    batteryStates[i].lastResponseTime = 0;
    batteryStates[i].status = BATTERY_NOT_RESPONDING;
    batteryStates[i].activeLimitation = NO_LIMITATION; // limitation de courant
    batteryStates[i].lastMosfetCheck = 0;
    batteryStates[i].consecutiveFailures = 0;
  }

  // Initialisation du diagnostic système (parmi toutes les batteries)
  systemDiag.degradedMode = false;
  systemDiag.activeBatteryCount = 0;
  systemDiag.respondingBatteryCount = 0;
  systemDiag.maxBatteryVoltage = 0.0f;
  systemDiag.minBatteryVoltage = 999.0f;
  systemDiag.maxBatteryTemp = -100.0f;
  systemDiag.minBatteryTemp = 999.0f;
  systemDiag.lastDiagnostic = 0; // timestamp du dernier diagnostic

  // Initialisation des consignes en charge et en décharge
  currentChargeSetpoint = 0.0f;
  currentDischargeSetpoint = 0.0f;

  // Initialisation des compteurs
  respondingBatteryCount = 0;

  // Flags
  degradedMode = false;

  initErrorSystem();
}

void runBatteryManagementCycle()
{
  processBatteriesGlobalState();
  // Calcul des nouvelles consignes
  currentChargeSetpoint = calculateChargeSetpoint();
  currentDischargeSetpoint = calculateDischargeSetpoint();
  updateStatusLeds(); // Mise à jour des LEDs d'état
}

// ——————— GESTION DES CONNEXIONS ET SURVEILLANCE ———————
void monitorBatteryConnections()
{
  static unsigned long lastConnectionCheck = 0;
  const unsigned long CONNECTION_CHECK_INTERVAL = 30000; // 30 secondes

  if (millis() - lastConnectionCheck >= CONNECTION_CHECK_INTERVAL)
  {
    lastConnectionCheck = millis();

    int respondingCount = checkBatteryConnections();

    // Mettre à jour la variable globale
    respondingBatteryCount = respondingCount;

    // Log spécial si changement d'état
    static int lastRespondingCount = -1;
    if (lastRespondingCount != respondingCount)
    {
      lastRespondingCount = respondingCount;
    }
  }
}

int checkBatteryConnections()
{
  extern IndividualBatteryData individualBatteryMetrics[MAX_BATTERIES];
  unsigned long now = millis();
  int currentRespondingCount = 0;
  bool batteryLost = false;

  for (int i = 0; i < configuredBatteryCount; i++)
  {
    int batteryId = i + 2;
    BatteryState *state = &batteryStates[i];
    IndividualBatteryData *data = &individualBatteryMetrics[i + 1];

    bool currentlyResponding = data->isValid;

    if (currentlyResponding)
    {
      if (!state->isResponding)
      {
        state->status = BATTERY_OK;
        removeSystemError(ERROR_BATTERY_DISCONNECTED, batteryId);
      }
      state->isResponding = true;
      state->lastResponseTime = now;
      state->consecutiveFailures = 0; // Réinitialiser en cas de succès
      currentRespondingCount++;
    }
    else
    {
      // Si la batterie était active, on vérifie si elle doit être déconnectée
      if (state->isResponding)
      {
        // CONDITION DE DÉCONNEXION : Le timeout est dépassé ET le nombre d'échecs est atteint
        if ((now - state->lastResponseTime > BATTERY_RESPONSE_TIMEOUT) && (state->consecutiveFailures >= MAX_CONSECUTIVE_FAILURES))
        {
          state->isResponding = false;
          state->status = BATTERY_NOT_RESPONDING;
          batteryLost = true;
          addSystemError(ERROR_BATTERY_DISCONNECTED, batteryId, "Batterie non detectee");
        }
      }
      else
      {
        if (state->status != BATTERY_NOT_RESPONDING)
        {
          state->status = BATTERY_NOT_RESPONDING;
        }
      }
    }
  }

  return currentRespondingCount;
}

// ——————— DIAGNOSTIC ET DÉTECTION DES PANNES ———————
void updateSystemMetrics()
{
  extern IndividualBatteryData individualBatteryMetrics[MAX_BATTERIES];
  systemDiag.lastDiagnostic = millis();
  // Reset des valeurs min/max
  systemDiag.maxBatteryVoltage = 0.0f;
  systemDiag.minBatteryVoltage = 999.0f;
  systemDiag.maxBatteryTemp = -100.0f;
  systemDiag.minBatteryTemp = 999.0f;
  respondingBatteryCount = 0; // Compteurs
  // Parcours des batteries configurées
  for (int i = 0; i < configuredBatteryCount; i++)
  {
    int batteryId = i + 2; // ID commence tooujours à 2
    BatteryState *state = &batteryStates[i];
    IndividualBatteryData *data = &individualBatteryMetrics[i + 1]; // Index dans le tableau global
    // Comptage des batteries répondantes
    if (state->isResponding && data->isValid)
    {
      respondingBatteryCount++;
      // Mise à jour min/max tensions
      if (data->voltage > systemDiag.maxBatteryVoltage)
        systemDiag.maxBatteryVoltage = data->voltage;
      if (data->voltage < systemDiag.minBatteryVoltage)
        systemDiag.minBatteryVoltage = data->voltage;
      // Mise à jour min/max températures
      if (data->maxCellTemp > systemDiag.maxBatteryTemp)
        systemDiag.maxBatteryTemp = data->maxCellTemp;
      if (data->minCellTemp < systemDiag.minBatteryTemp)
        systemDiag.minBatteryTemp = data->minCellTemp;
    }
  }
  // Mise à jour des structures globales
  systemDiag.respondingBatteryCount = respondingBatteryCount;
}

void processBatteriesGlobalState()
{
  extern IndividualBatteryData individualBatteryMetrics[MAX_BATTERIES];
  extern AggregateBatteryMetrics latestMetrics;
  globalChargeMosfetOk = true;
  globalDischargeMosfetOk = true;
  systemPowerButtonOn = true;
  turnedOffBatteryId = 0;
  // Accumulateurs pour les moyennes
  float minSoc = 101.0f;
  float totalSoc = 0.0f;
  float totalVoltage = 0.0f;
  // On utilise un entier 32 bits pour cumuler les valeurs avec offset (30000)
  int32_t totalCurrentRawSum = 0;
  float totalTempSystem = 0.0f;
  int validBatteries = 0;
  unsigned long now = millis();

  for (int i = 0; i < configuredBatteryCount; i++)
  {
    int batteryId = i + 2; // Les IDs commencent à 2
    BatteryState *state = &batteryStates[i];
    IndividualBatteryData *data = &individualBatteryMetrics[i + 1]; // Index 1 pour ID 2
    // Si la batterie ne répond pas ou données invalides, on l'ignore
    if (!state->isResponding || !data->isValid)
    {
      continue;
    }
    // === 1. LOGIQUE MOSFET & LIMITATIONS ===
    bool chargeMosfetOk = data->chargeMosfetStatus;
    bool dischargeMosfetOk = data->dischargeMosfetStatus;
    // Mise à jour des flags globaux (Un seul suffit pour passer à false)
    if (!chargeMosfetOk)
      globalChargeMosfetOk = false;
    if (!dischargeMosfetOk)
      globalDischargeMosfetOk = false;
    // Détection de panne MOSFET active
    if (!chargeMosfetOk || !dischargeMosfetOk)
    {
      // Gestion des erreurs d'affichage
      if (!chargeMosfetOk && !dischargeMosfetOk)
      {
        addSystemError(ERROR_MOSFET_CHARGE, batteryId, "MOSFET charge OFF");
        addSystemError(ERROR_MOSFET_DISCHARGE, batteryId, "MOSFET decharge OFF");
      }
      else if (!chargeMosfetOk)
      {
        addSystemError(ERROR_MOSFET_CHARGE, batteryId, "MOSFET charge OFF");
      }
      else
      {
        addSystemError(ERROR_MOSFET_DISCHARGE, batteryId, "MOSFET decharge OFF");
      }
      // Bascule en statut "Problème MOSFET" si ce n'est pas déjà fait
      if (state->status != BATTERY_MOSFET_ISSUE)
      {
        state->status = BATTERY_MOSFET_ISSUE;
        state->lastMosfetCheck = now; // On mémorise le début du problème
        state->activeLimitation = MOSFET_LIMITATION;
      }
    }
    else
    {
      // Les MOSFETs sont physiquement OK ici.
      removeSystemError(ERROR_MOSFET_CHARGE, batteryId);
      removeSystemError(ERROR_MOSFET_DISCHARGE, batteryId);
      // LOGIQUE DE RÉCUPÉRATION (Recovery)
      if (state->status == BATTERY_MOSFET_ISSUE)
      {
        if (now - state->lastMosfetCheck > MOSFET_RECOVERY_CHECK_INTERVAL)
        {
          if (checkMosfetRecoveryConditions(i))
          {
            state->status = BATTERY_OK;
            state->activeLimitation = NO_LIMITATION;
            state->lastMosfetCheck = 0;
          }
        }
      }
      else if (state->activeLimitation == MOSFET_LIMITATION)
      {
        state->activeLimitation = NO_LIMITATION;
      }
    }
    // === 2. LOGIQUE BOUTON (WAKE UP SOURCE) ===
    // Bit 0 de wakeUpSource = key (bouton physique)
    bool isKeyOn = (data->wakeUpSource & 0x01) != 0;

    if (!isKeyOn)
    {
      addSystemError(ERROR_WAKEUP_BUTTON_OFF, batteryId, "Cle (key) OFF");
      systemPowerButtonOn = false; // Coupe le système global
      if (turnedOffBatteryId == 0)
        turnedOffBatteryId = batteryId;
    }
    else
    {
      removeSystemError(ERROR_WAKEUP_BUTTON_OFF, batteryId);
    }
    // ---LOGIQUE MOYENNES & AGRÉGATION ---
    validBatteries++;
    totalSoc += data->soc;
    // On retient le SOC le plus faible du parc
    if (data->soc < minSoc)
      minSoc = data->soc;

    totalVoltage += data->voltage;

    // --- CUMUL DU COURANT (MÉTHODE BRUTE AVEC OFFSET) ---
    // Exemple : 2.0A -> 20 + 30000 = 30020
    int32_t currentRaw = (int32_t)(data->current * 10.0f) + 30000;
    totalCurrentRawSum += currentRaw;
    // Moyenne Température de ce module
    float batAvgTemp = (data->minCellTemp + data->maxCellTemp) / 2.0f;
    totalTempSystem += batAvgTemp;
  }
  // ——— FINALISATION DES CALCULS ———
  respondingBatteryCount = validBatteries;
  if (validBatteries > 0)
  {
    latestMetrics.averageSoc = totalSoc / validBatteries;         // SOC Moyen
    latestMetrics.averageVoltage = totalVoltage / validBatteries; // Moyenne Tensions
    // --- CALCUL FINAL DU COURANT ---
    // Formule : TotalBrut - (30000 * NombreDeBatteries)
    int32_t totalOffset = 30000 * validBatteries;
    int32_t finalNetRaw = totalCurrentRawSum - totalOffset;
    // Conversion finale en Ampères (float) pour le reste du système
    latestMetrics.totalCurrent = finalNetRaw / 10.0f;
    // -------------------------------

    latestMetrics.averageTemp = totalTempSystem / validBatteries; // Moyenne Temps
    latestMetrics.isDataValid = true;
  }
  else
  {
    latestMetrics.isDataValid = false;
    latestMetrics.averageSoc = 0;
    latestMetrics.averageVoltage = 0;
    latestMetrics.totalCurrent = 0;
    latestMetrics.averageTemp = 0;
  }
}

bool checkMosfetRecoveryConditions(int batteryIndex)
{
  extern IndividualBatteryData individualBatteryMetrics[MAX_BATTERIES];

  BatteryState *state = &batteryStates[batteryIndex];
  IndividualBatteryData *data = &individualBatteryMetrics[batteryIndex + 1];

  bool mosOk = data->chargeMosfetStatus && data->dischargeMosfetStatus;
  bool voltageOk = true;
  float avgCellTemp = (data->maxCellTemp + data->minCellTemp) / 2.0f;
  bool tempOk = avgCellTemp < MAX_MOS_TEMP;

  return mosOk && voltageOk && tempOk;
}

// ——————— CALCUL DES CONSIGNES ———————
float calculateChargeSetpoint()
{
  // --- VÉRIFICATIONS DE SÉCURITÉ PRIORITAIRES (INCHANGÉ) ---
  if (!systemPowerButtonOn)
  {
    return 0.0f; // Un bouton est OFF, on ne charge pas
  }
  if (!globalChargeMosfetOk)
  {
    // Serial.println("SÉCURITÉ: MOSFET Charge OFF détecté. Consigne de charge globale = 0A.");
    return 0.0f; // Un MOSFET de charge est OFF
  }

  // Si le nombre de batteries qui répondent est inférieur au nombre configuré, on arrête tout.
  if (respondingBatteryCount < configuredBatteryCount)
  {
    /* Serial.printf("SECURITE: Perte de com (%d/%d batteries). Consigne de charge = 0A.\n",
                  respondingBatteryCount, configuredBatteryCount);*/
    return 0.0f;
  }

  extern IndividualBatteryData individualBatteryMetrics[MAX_BATTERIES];

  // --- Initialiser les consignes  ---
  float normalSetpoint = NORMAL_CURRENT_PER_BATTERY * 0.8f;
  float limitedSetpoint = 0.0f;

  // Récupérer la capacité nominale pour le calcul de la consigne limitée
  for (int i = 0; i < configuredBatteryCount; i++)
  {
    // ID 2 -> index 1
    if (batteryStates[i].isResponding && individualBatteryMetrics[i + 1].isValid)
    {
      limitedSetpoint = individualBatteryMetrics[i + 1].ratedCapacity / 10.0f;
      break; // On a trouvé une capacité nominale, c'est suffisant
    }
  }

  if (limitedSetpoint == 0.0f)
    return 0.0f; // Aucune batterie ne répond ou capacité non lue

  // --- Parcourir les batteries pour chercher les ALARMES BMS ---
  bool limitRequired = false; // Flag si une alarme de limitation est active

  for (int i = 0; i < configuredBatteryCount; i++)
  {
    int batteryId = i + 2;                                          // Pour les logs (ID 2, 3...)
    IndividualBatteryData *data = &individualBatteryMetrics[i + 1]; // ID 2 -> index 1

    if (!batteryStates[i].isResponding || !data->isValid)
      continue;

    // Extraire les octets d'alarme
    uint8_t fault_0 = data->faultCode0_1 & 0x00FF;        // 0x6D Low
    uint8_t fault_1 = (data->faultCode0_1 >> 8) & 0x00FF; // 0x6D High
    uint8_t fault_2 = data->faultCode2_3 & 0x00FF;        // 0x6E Low

    // --- Vérifier les alarmes de CHARGE  ---
    // Alarme 1: Single cell over voltage Level-2 (Vérification manuelle)
    if (data->overVoltL2Threshold_mV > 0) // On vérifie que le seuil a bien été lu
    {
      // Convertir le seuil (mV) en Volts
      float threshold_V = data->overVoltL2Threshold_mV / 1000.0f;
      // data->maxCellVoltage est déjà en Volts (lu depuis 0x003E)
      if (data->maxCellVoltage >= threshold_V)
      {
        limitRequired = true;
        /* Serial.printf("CHARGE LIMIT: Surtension L2 MANUELLE (%.3fV >= %.3fV) (Batt ID %d)\n",
                      data->maxCellVoltage, threshold_V, batteryId);*/
        break; // Une alarme /10 suffit, on sort de la boucle
      }
    }

    // Alarme 2: Single cell under voltage Level-1 (0x6D Low, bit 3)
    // (bit5:3 -> 0b001000 = Level 1 = bit 3)
    if (fault_0 & 0b00001000)
    {
      limitRequired = true;
      // Serial.printf("CHARGE LIMIT: Alarme Sous-tension L1 (Batt ID %d)\n", batteryId);
      break;
    }
    // Alarme 3: Charging high temperature Level-1 (0x6D High, bit 3)
    // (bit5:3 -> 0b001000 = Level 1 = bit 3)
    if (fault_1 & 0b00001000)
    {
      limitRequired = true;
      // Serial.printf("CHARGE LIMIT: Alarme Temp Charge Haute L1 (Batt ID %d)\n", batteryId);
      break;
    }
    // Alarme 4: Charging low temperature Level-1 (0x6E Low, bit 0)
    // (bit2:0 -> 0b001 = Level 1 = bit 0)
    if (fault_2 & 0b00000001)
    {
      limitRequired = true;
      // Serial.printf("CHARGE LIMIT: Alarme Temp Charge Basse L1 (Batt ID %d)\n", batteryId);
      break;
    }
  }
  // --- Appliquer les règles ---
  if (limitRequired)
  {
    // Serial.println("CHARGE LIMIT: Une alarme BMS (Rated/10) est active.");
    //  Appliquer la consigne limitée pour l'ensemble du parc
    return limitedSetpoint * respondingBatteryCount;
  }
  // Si aucune alarme n'est atteinte (NIVEAU PLEINE CAPACITÉ)
  return normalSetpoint * respondingBatteryCount;
}

float calculateDischargeSetpoint()
{
  // --- VÉRIFICATIONS DE SÉCURITÉ PRIORITAIRES (INCHANGÉ) ---
  if (!systemPowerButtonOn)
  {
    return 0.0f; // Un bouton est OFF
  }
  if (!globalDischargeMosfetOk)
  {
    // Serial.println("SÉCURITÉ: MOSFET Décharge OFF détecté. Consigne de décharge globale = 0A.");
    return 0.0f; // Un MOSFET de décharge est OFF
  }

  // Si le nombre de batteries qui répondent est inférieur au nombre configuré, on arrête tout.
  if (respondingBatteryCount < configuredBatteryCount)
  {
    /*Serial.printf("SECURITE: Perte de com (%d/%d batteries). Consigne de décharge = 0A.\n",
                  respondingBatteryCount, configuredBatteryCount);*/
    return 0.0f;
  }

  extern IndividualBatteryData individualBatteryMetrics[MAX_BATTERIES];

  // --- 1. Initialiser les consignes ---
  float normalSetpoint = NORMAL_CURRENT_PER_BATTERY * 0.8f;
  float limitedSetpoint = 0.0f;

  for (int i = 0; i < configuredBatteryCount; i++)
  {
    if (batteryStates[i].isResponding && individualBatteryMetrics[i + 1].isValid)
    {
      limitedSetpoint = individualBatteryMetrics[i + 1].ratedCapacity / 10.0f;
      break; // On a trouvé une capacité nominale
    }
  }

  if (limitedSetpoint == 0.0f)
    return 0.0f; // Aucune batterie ne répond

  // ---  Parcourir les batteries pour chercher les ALARMES BMS ---
  bool stopRequired = false;  // Flag pour une alarme 0A
  bool limitRequired = false; // Flag pour une alarme Rated/10

  for (int i = 0; i < configuredBatteryCount; i++)
  {
    int batteryId = i + 2;                                          // Pour les logs (ID 2, 3...)
    IndividualBatteryData *data = &individualBatteryMetrics[i + 1]; // ID 2 -> index 1

    if (!batteryStates[i].isResponding || !data->isValid)
      continue;

    // Extraire les octets d'alarme
    uint8_t fault_0 = data->faultCode0_1 & 0x00FF;        // 0x6D Low
    uint8_t fault_2 = data->faultCode2_3 & 0x00FF;        // 0x6E Low
    uint8_t fault_3 = (data->faultCode2_3 >> 8) & 0x00FF; // 0x6E High

    // --- NIVEAU STOP (Prioritaire) ---
    // Alarme 1: Single cell under voltage Level-3 (0x6D Low, bit 5)
    // (bit5:3 -> 0b100000 = Level 3 = bit 5)
    if (fault_0 & 0b00100000)
    {
      stopRequired = true;
      // Serial.printf("DECHARGE STOP: Alarme Sous-tension L3 (Batt ID %d)\n", batteryId);
      break; // STOP prioritaire, on sort de la boucle
    }
    // --- NIVEAU LIMITÉ (vérifié seulement si pas de STOP) ---
    // Alarme 2: Single cell under voltage Level-1 (0x6D Low, bit 3)
    // (bit5:3 -> 0b001000 = Level 1 = bit 3)
    if (fault_0 & 0b00001000)
    {
      limitRequired = true;
      // Serial.printf("DECHARGE LIMIT: Alarme Sous-tension L1 (Batt ID %d)\n", batteryId);
    }

    // Alarme 3: Discharging high temperature Level-1 (0x6E Low, bit 3)
    // (bit5:3 -> 0b001000 = Level 1 = bit 3)
    if (fault_2 & 0b00001000)
    {
      limitRequired = true;
      // Serial.printf("DECHARGE LIMIT: Alarme Temp Décharge Haute L1 (Batt ID %d)\n", batteryId);
    }

    // Alarme 4: Discharging low temperature Level-1 (0x6E High, bit 0)
    // (bit2:0 -> 0b001 = Level 1 = bit 0)
    if (fault_3 & 0b00000001)
    {
      limitRequired = true;
      // Serial.printf("DECHARGE LIMIT: Alarme Temp Décharge Basse L1 (Batt ID %d)\n", batteryId);
    }
  }

  // --- Appliquer les règles de limitation ---

  // NIVEAU STOP (Prioritaire)
  if (stopRequired)
  {
    // Serial.println("DECHARGE STOP: Une alarme BMS de niveau 3 (Sous-tension) est active.");
    return 0.0f; // Arrêt complet
  }
  // NIVEAU LIMITÉ
  if (limitRequired)
  {
    // Serial.println("DECHARGE LIMIT: Une alarme BMS de niveau 1 ou 2 est active.");
    //  Appliquer la consigne limitée pour l'ensemble du parc
    return limitedSetpoint * respondingBatteryCount;
  }
  // Si aucune règle n'est atteinte (NIVEAU PLEINE CAPACITÉ)
  return normalSetpoint * respondingBatteryCount;
}

// ——————— GESTION DES LEDS D'ÉTAT ———————
void initStatusLeds()
{
  pinMode(LED_RED_PIN, OUTPUT);
  digitalWrite(LED_RED_PIN, LOW); // LED rouge éteinte au démarrage
}

void updateStatusLeds()
{
  bool shouldShowAlarm = false;

  // Conditions d'alarme (selon votre logique système)
  if (degradedMode || respondingBatteryCount < configuredBatteryCount || systemDiag.maxBatteryTemp > MAX_DISCHARGE_TEMP || (systemDiag.maxBatteryVoltage - systemDiag.minBatteryVoltage) > VOLTAGE_DIFF_THRESHOLD)
  {
    shouldShowAlarm = true;
  }

  // Vérifier les alarmes individuelles des batteries
  for (int i = 0; i < configuredBatteryCount; i++)
  {
    BatteryState *state = &batteryStates[i];
    if (state->status == BATTERY_MOSFET_ISSUE || state->status == BATTERY_TEMP_ISSUE || state->status == BATTERY_CELL_IMBALANCE || state->activeLimitation != NO_LIMITATION)
    {
      shouldShowAlarm = true;
      break;
    }
  }

  // Contrôler la LED rouge
  digitalWrite(LED_RED_PIN, shouldShowAlarm ? HIGH : LOW);
}

// ——————— GESTION DES ERREURS SYSTÈME ———————
void initErrorSystem()
{
  for (int i = 0; i < MAX_SYSTEM_ERRORS; i++)
  {
    systemErrors[i].active = false;
    systemErrors[i].type = ERROR_BATTERY_DISCONNECTED;
    systemErrors[i].batteryId = -1;
    systemErrors[i].firstOccurred = 0;
    systemErrors[i].lastOccurred = 0;
    systemErrors[i].occurrenceCount = 0;
    memset(systemErrors[i].description, 0, sizeof(systemErrors[i].description));
  }
  errorCount = 0;
}

void addSystemError(ErrorType type, int batteryId, const char *description)
{
  unsigned long now = millis();

  // Chercher si l'erreur existe déjà
  for (int i = 0; i < MAX_SYSTEM_ERRORS; i++)
  {
    if (systemErrors[i].active && systemErrors[i].type == type && systemErrors[i].batteryId == batteryId)
    {
      // Erreur existante - mettre à jour
      systemErrors[i].lastOccurred = now;
      systemErrors[i].occurrenceCount++;
      return;
    }
  }

  // Nouvelle erreur - trouver une place libre
  for (int i = 0; i < MAX_SYSTEM_ERRORS; i++)
  {
    if (!systemErrors[i].active)
    {
      systemErrors[i].active = true;
      systemErrors[i].type = type;
      systemErrors[i].batteryId = batteryId;
      systemErrors[i].firstOccurred = now;
      systemErrors[i].lastOccurred = now;
      systemErrors[i].occurrenceCount = 1;
      strncpy(systemErrors[i].description, description, sizeof(systemErrors[i].description) - 1);
      errorCount++;

      break;
    }
  }
}

void removeSystemError(ErrorType type, int batteryId)
{
  for (int i = 0; i < MAX_SYSTEM_ERRORS; i++)
  {
    if (systemErrors[i].active && systemErrors[i].type == type && systemErrors[i].batteryId == batteryId)
    {
      systemErrors[i].active = false;
      errorCount--;
      break;
    }
  }
}
