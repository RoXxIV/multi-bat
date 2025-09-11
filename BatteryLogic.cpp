#include "BatteryLogic.h"

BatteryState batteryStates[MAX_BATTERIES];
SystemDiagnostics systemDiag;
float currentChargeSetpoint = 0.0f;
float currentDischargeSetpoint = 0.0f;
int activeBatteryCount = 0;
int respondingBatteryCount = 0;
bool degradedMode = false;
// Variables globales pour les erreurs
SystemError systemErrors[MAX_SYSTEM_ERRORS];
int errorCount = 0;

// ——————— INITIALISATION ET SYSTÈME ———————
void initBatteryManagement()
{
    // Initialisation des états des batteries
    for (int i = 0; i < MAX_BATTERIES; i++)
    {
        // logique individuelle
        batteryStates[i].isResponding = false;
        batteryStates[i].lastResponseTime = 0;
        batteryStates[i].status = BATTERY_NOT_RESPONDING;
        batteryStates[i].activeLimitation = NO_LIMITATION; // limitation de courant
        batteryStates[i].isActive = false;                 // vvv
        batteryStates[i].voltageDelta = 0.0f;              // delta de tension avec la batterie la plus chargé ?
        batteryStates[i].lastMosfetCheck = 0;
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
    activeBatteryCount = 0;
    respondingBatteryCount = 0;

    // Flags
    degradedMode = false;

    initErrorSystem();
    Serial.println("Battery Management System Initialised");
}

void runBatteryManagementCycle()
{
    Serial.println("\n=== Cycle principale ===");

    updateVoltageDeltas();      // Mise à jour des deltas de tension
    checkMosfetStatus();        // Vérification MOSFETs
    checkWakeUpSource();        // Vérification du bouton power batterie
    checkBmsFaults();           // Vérification des codes defauts BMS
    manageBatteryLimitations(); // Gestion des limitations individuelles
    manageBatteryActivation();  // Gestion de l'activation des batteries
    calculateAverages();        // Calcul des moyennes et agrégation

    // Calcul des nouvelles consignes
    currentChargeSetpoint = calculateChargeSetpoint();
    currentDischargeSetpoint = calculateDischargeSetpoint();

    updateStatusLeds(); // Mise à jour des LEDs d'état

    Serial.printf("CYCLE TERMINÉ - Consignes: Charge=%.1fA Décharge=%.1fA\n",
                  currentChargeSetpoint, currentDischargeSetpoint);
}

void printSystemStatus()
{
    Serial.println("\n=== ÉTAT SYSTÈME BATTERIES ===");
    Serial.printf("Mode dégradé: %s\n", degradedMode ? "ACTIF" : "NORMAL");
    Serial.printf("Batteries actives: %d/%d\n", activeBatteryCount, configuredBatteryCount);
    Serial.printf("Batteries répondantes: %d/%d\n", respondingBatteryCount, configuredBatteryCount);
    Serial.printf("Consigne charge: %.1fA\n", currentChargeSetpoint);
    Serial.printf("Consigne décharge: %.1fA\n", currentDischargeSetpoint);

    if (systemDiag.maxBatteryVoltage > 0)
    {
        Serial.printf("Tensions: %.2fV - %.2fV\n",
                      systemDiag.minBatteryVoltage, systemDiag.maxBatteryVoltage);
        Serial.printf("Delta tension: %.3fV\n",
                      systemDiag.maxBatteryVoltage - systemDiag.minBatteryVoltage);
    }

    if (systemDiag.maxBatteryTemp > -100)
    {
        Serial.printf("Températures: %.1f°C - %.1f°C\n",
                      systemDiag.minBatteryTemp, systemDiag.maxBatteryTemp);
        Serial.printf("Delta température: %.1f°C\n",
                      systemDiag.maxBatteryTemp - systemDiag.minBatteryTemp);
    }

    // Détail par batterie (seulement si configurées)
    Serial.println("--- Détail par batterie ---");
    for (int i = 0; i < configuredBatteryCount; i++)
    {
        int batteryId = i + 2;
        BatteryState *state = &batteryStates[i];

        const char *statusText = "Inconnu";
        switch (state->status)
        {
        case BATTERY_OK:
            statusText = "OK";
            break;
        case BATTERY_NOT_RESPONDING:
            statusText = "Muette";
            break;
        case BATTERY_MOSFET_ISSUE:
            statusText = "MOSFET";
            break;
        case BATTERY_TEMP_ISSUE:
            statusText = "Temp";
            break;
        case BATTERY_VOLTAGE_ISSUE:
            statusText = "Tension";
            break;
        case BATTERY_CELL_IMBALANCE:
            statusText = "Déséq";
            break;
        }

        const char *limitText = "Aucune";
        switch (state->activeLimitation)
        {
        case NO_LIMITATION:
            limitText = "Aucune";
            break;
        case MOSFET_LIMITATION:
            limitText = "MOSFET";
            break;
        case VOLTAGE_LIMITATION:
            limitText = "Tension";
            break;
        case TEMP_LIMITATION:
            limitText = "Temp";
            break;
        case DEGRADED_MODE_LIMITATION:
            limitText = "Dégradé";
            break;
        }

        Serial.printf("  ID=%d: %s, %s, Limite: %s\n",
                      batteryId,
                      statusText,
                      state->isActive ? "Active" : "Inactive",
                      limitText);
    }
}

void updateSystemMetrics()
{
    extern IndividualBatteryData individualBatteryMetrics[MAX_BATTERIES];
    systemDiag.lastDiagnostic = millis();
    // Reset des valeurs min/max
    systemDiag.maxBatteryVoltage = 0.0f;
    systemDiag.minBatteryVoltage = 999.0f;
    systemDiag.maxBatteryTemp = -100.0f;
    systemDiag.minBatteryTemp = 999.0f;
    // Compteurs
    activeBatteryCount = 0;
    respondingBatteryCount = 0;

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

        // Comptage des batteries actives (pour l'instant toutes les répondantes sont actives)
        if (state->isActive)
        {
            activeBatteryCount++;
        }
    }

    // Mise à jour des structures globales
    systemDiag.activeBatteryCount = activeBatteryCount;
    systemDiag.respondingBatteryCount = respondingBatteryCount;
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
            Serial.printf("🔄 CHANGEMENT ÉTAT: %d→%d batteries répondantes\n",
                          lastRespondingCount, respondingCount);
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

    Serial.println("=== VÉRIFICATION CONNEXIONS BATTERIES ===");

    for (int i = 0; i < configuredBatteryCount; i++)
    {
        int batteryId = i + 2; // ID commence à 2
        BatteryState *state = &batteryStates[i];
        IndividualBatteryData *data = &individualBatteryMetrics[i + 1];

        // Vérifier si la batterie répond actuellement
        bool currentlyResponding = data->isValid;

        if (currentlyResponding)
        {
            // Batterie répond - mettre à jour le timestamp
            if (!state->isResponding)
            {
                Serial.printf("✅ BATTERIE ID=%d RECONNECTÉE\n", batteryId);
                state->status = BATTERY_OK;
                removeSystemError(ERROR_BATTERY_DISCONNECTED, batteryId);
            }

            state->isResponding = true;
            state->lastResponseTime = now;
            currentRespondingCount++;
        }
        else
        {
            // Batterie ne répond pas - vérifier le timeout
            if (state->isResponding)
            {
                // La batterie répondait avant, vérifier le timeout
                if (now - state->lastResponseTime > BATTERY_RESPONSE_TIMEOUT)
                {
                    Serial.printf("❌ BATTERIE ID=%d NON DÉTECTÉE (timeout: %lums)\n",
                                  batteryId, now - state->lastResponseTime);

                    state->isResponding = false;
                    state->status = BATTERY_NOT_RESPONDING;
                    state->isActive = false; // Désactiver la batterie
                    batteryLost = true;
                    addSystemError(ERROR_BATTERY_DISCONNECTED, batteryId, "Batterie non detectee");
                }
            }
            else
            {
                // Batterie déjà marquée comme non répondante
                if (state->status != BATTERY_NOT_RESPONDING)
                {
                    state->status = BATTERY_NOT_RESPONDING;
                    state->isActive = false;
                }
            }
        }

        Serial.printf("  ID=%d: %s (dernière réponse: %lums)\n",
                      batteryId,
                      state->isResponding ? "RÉPOND" : "MUETTE",
                      state->isResponding ? 0 : (now - state->lastResponseTime));
    }

    // Activer le mode dégradé si des batteries sont perdues
    if (batteryLost && !degradedMode)
    {
        enableDegradedMode("Batterie(s) non détectée(s)");
    }

    // Désactiver le mode dégradé si toutes les batteries sont revenues
    if (currentRespondingCount == configuredBatteryCount && degradedMode)
    {
        // Vérifier si le mode dégradé était activé uniquement pour les connexions
        bool canDisableDegraded = true;

        // Ici on pourrait ajouter d'autres vérifications (température, MOSFETs, etc.)
        // Pour l'instant, on se contente des connexions

        if (canDisableDegraded)
        {
            disableDegradedMode();
        }
    }

    Serial.printf("RÉSULTAT: %d/%d batteries répondent\n",
                  currentRespondingCount, configuredBatteryCount);

    return currentRespondingCount;
}

// ——————— DIAGNOSTIC ET DÉTECTION DES PANNES ———————

bool checkMosfetStatus()
{

    extern IndividualBatteryData individualBatteryMetrics[MAX_BATTERIES];

    bool mosfetIssueDetected = false;
    unsigned long now = millis();

    Serial.println("=== VÉRIFICATION ÉTAT MOSFETs ===");

    for (int i = 0; i < configuredBatteryCount; i++)
    {
        int batteryId = i + 2;
        BatteryState *state = &batteryStates[i];
        IndividualBatteryData *data = &individualBatteryMetrics[i + 1];

        // Ignorer les batteries qui ne répondent pas
        if (!state->isResponding || !data->isValid)
        {
            continue;
        }

        bool chargeMosfetOk = data->chargeMosfetStatus;
        bool dischargeMosfetOk = data->dischargeMosfetStatus;
        bool hadMosfetIssue = (state->status == BATTERY_MOSFET_ISSUE);

        // Vérifier les MOSFETs
        if (!chargeMosfetOk || !dischargeMosfetOk)
        {
            Serial.printf("⚠️  BATTERIE ID=%d: MOSFET ", batteryId);

            if (!chargeMosfetOk && !dischargeMosfetOk)
            {
                Serial.println("CHARGE+DÉCHARGE OFF");
                addSystemError(ERROR_MOSFET_CHARGE, batteryId, "MOSFET charge OFF");
                addSystemError(ERROR_MOSFET_DISCHARGE, batteryId, "MOSFET decharge OFF");
            }
            else if (!chargeMosfetOk)
            {
                Serial.println("CHARGE OFF");
                addSystemError(ERROR_MOSFET_CHARGE, batteryId, "MOSFET charge OFF");
            }
            else
            {
                Serial.println("DÉCHARGE OFF");
                addSystemError(ERROR_MOSFET_DISCHARGE, batteryId, "MOSFET decharge OFF");
            }

            // Marquer comme ayant un problème MOSFET
            if (state->status != BATTERY_MOSFET_ISSUE)
            {
                state->status = BATTERY_MOSFET_ISSUE;
                state->lastMosfetCheck = now;

                // Appliquer une limitation de courant
                state->activeLimitation = MOSFET_LIMITATION;

                Serial.printf("🚨 LIMITATION COURANT activée pour batterie ID=%d\n", batteryId);
            }

            mosfetIssueDetected = true;
        }
        else
        {
            // MOSFETs OK - vérifier si récupération d'un problème précédent
            if (hadMosfetIssue)
            {
                // Vérifier si assez de temps s'est écoulé et si la tension est équilibrée
                if (now - state->lastMosfetCheck > MOSFET_RECOVERY_CHECK_INTERVAL)
                {
                    bool canRecover = checkMosfetRecoveryConditions(i);

                    if (canRecover)
                    {
                        Serial.printf("✅ BATTERIE ID=%d: MOSFETs récupérés\n", batteryId);

                        state->status = BATTERY_OK;
                        state->activeLimitation = NO_LIMITATION;
                        state->lastMosfetCheck = 0;
                        removeSystemError(ERROR_MOSFET_CHARGE, batteryId);
                        removeSystemError(ERROR_MOSFET_DISCHARGE, batteryId);
                    }
                    else
                    {
                        Serial.printf("⏳ BATTERIE ID=%d: En attente d'équilibrage (delta=%.3fV)\n",
                                      batteryId, state->voltageDelta);
                        removeSystemError(ERROR_MOSFET_CHARGE, batteryId);
                        removeSystemError(ERROR_MOSFET_DISCHARGE, batteryId);
                    }
                }
            }
            else
            {
                // Tout va bien
                Serial.printf("✅ BATTERIE ID=%d: MOSFETs OK\n", batteryId);
            }
        }
    }

    return mosfetIssueDetected;
}

bool checkMosfetRecoveryConditions(int batteryIndex)
{
    extern IndividualBatteryData individualBatteryMetrics[MAX_BATTERIES];

    BatteryState *state = &batteryStates[batteryIndex];
    IndividualBatteryData *data = &individualBatteryMetrics[batteryIndex + 1];

    // Conditions de récupération :
    // 1. MOSFETs maintenant ON
    // 2. Delta tension avec batterie max < VOLTAGE_TOLERANCE (0.3V)
    // 3. Pas de surtempérature

    bool mosOk = data->chargeMosfetStatus && data->dischargeMosfetStatus;
    bool voltageOk = state->voltageDelta < VOLTAGE_TOLERANCE;
    float avgCellTemp = (data->maxCellTemp + data->minCellTemp) / 2.0f;
    bool tempOk = avgCellTemp < MAX_MOS_TEMP;

    return mosOk && voltageOk && tempOk;
}

bool checkDegradedModeConditions()
{

    extern IndividualBatteryData individualBatteryMetrics[MAX_BATTERIES];

    bool shouldActivateDegraded = false;
    static char degradedReason[100] = "";

    Serial.println("=== VÉRIFICATION CONDITIONS MODE DÉGRADÉ ===");

    // ——————— CONDITION 1: Différence de température > 10°C ———————
    if (systemDiag.maxBatteryTemp - systemDiag.minBatteryTemp > TEMP_DIFF_THRESHOLD)
    {
        snprintf(degradedReason, sizeof(degradedReason),
                 "Delta température %.1f°C > %.1f°C",
                 systemDiag.maxBatteryTemp - systemDiag.minBatteryTemp,
                 TEMP_DIFF_THRESHOLD);

        Serial.printf("🚨 %s\n", degradedReason);
        addSystemError(ERROR_OVERTEMPERATURE, -1, "Delta temperature > 10C");
        shouldActivateDegraded = true;
    }
    else
    {
        removeSystemError(ERROR_OVERTEMPERATURE, -1);
    }

    // ——————— CONDITION 2: Différence tension cellules > 250mV ———————
    for (int i = 0; i < configuredBatteryCount; i++)
    {
        int batteryId = i + 2;
        BatteryState *state = &batteryStates[i];
        IndividualBatteryData *data = &individualBatteryMetrics[i + 1];

        if (!state->isResponding || !data->isValid)
            continue;

        if (data->cellVoltageDifference > CELL_DIFF_THRESHOLD)
        {
            snprintf(degradedReason, sizeof(degradedReason),
                     "Déséquilibrage batterie ID=%d: %.3fV > %.3fV",
                     batteryId, data->cellVoltageDifference, CELL_DIFF_THRESHOLD);

            Serial.printf("🚨 %s\n", degradedReason);

            // Marquer la batterie avec un problème de déséquilibre
            addSystemError(ERROR_CELL_IMBALANCE, batteryId, "Desequilibrage cellules");
            state->status = BATTERY_CELL_IMBALANCE;
            shouldActivateDegraded = true;
        }
        else
        {
            removeSystemError(ERROR_CELL_IMBALANCE, batteryId);
        }
    }

    // ——————— CONDITION 3: Batteries non répondantes ———————
    if (respondingBatteryCount < configuredBatteryCount)
    {
        int lostBatteries = configuredBatteryCount - respondingBatteryCount;
        snprintf(degradedReason, sizeof(degradedReason),
                 "%d batterie(s) non détectée(s)", lostBatteries);

        Serial.printf("🚨 %s\n", degradedReason);
        shouldActivateDegraded = true;
    }

    // ——————— CONDITION 4: Problèmes de MOSFETs ———————
    bool mosfetIssues = false;
    for (int i = 0; i < configuredBatteryCount; i++)
    {
        if (batteryStates[i].status == BATTERY_MOSFET_ISSUE)
        {
            mosfetIssues = true;
            break;
        }
    }

    if (mosfetIssues)
    {
        snprintf(degradedReason, sizeof(degradedReason),
                 "Problème(s) MOSFET détecté(s)");

        Serial.printf("🚨 %s\n", degradedReason);
        shouldActivateDegraded = true;
    }

    // ——————— CONDITION 5: Surtempérature générale ———————
    if (systemDiag.maxBatteryTemp > MAX_DISCHARGE_TEMP)
    {
        snprintf(degradedReason, sizeof(degradedReason),
                 "Surtempérature système: %.1f°C > %.1f°C",
                 systemDiag.maxBatteryTemp, MAX_DISCHARGE_TEMP);

        Serial.printf("🚨 %s\n", degradedReason);
        addSystemError(ERROR_OVERTEMPERATURE, -1, "Surtemperature systeme");
        shouldActivateDegraded = true;
    }

    // ——————— APPLIQUER LE CHANGEMENT D'ÉTAT ———————
    if (shouldActivateDegraded && !degradedMode)
    {
        enableDegradedMode(degradedReason);
    }
    else if (!shouldActivateDegraded && degradedMode)
    {
        // Vérifier si on peut sortir du mode dégradé
        bool allBatteriesRespond = (respondingBatteryCount == configuredBatteryCount);
        bool tempOk = (systemDiag.maxBatteryTemp <= MAX_DISCHARGE_TEMP);
        bool tempDeltaOk = (systemDiag.maxBatteryTemp - systemDiag.minBatteryTemp <= TEMP_DIFF_THRESHOLD);
        bool noCriticalImbalance = true;

        extern IndividualBatteryData individualBatteryMetrics[MAX_BATTERIES];
        for (int i = 0; i < configuredBatteryCount; i++)
        {
            IndividualBatteryData *data = &individualBatteryMetrics[i + 1];
            if (batteryStates[i].isResponding && data->isValid)
            {
                if (data->cellVoltageDifference > CELL_DIFF_THRESHOLD)
                {
                    noCriticalImbalance = false;
                    break;
                }
            }
        }

        if (allBatteriesRespond && tempOk && tempDeltaOk && noCriticalImbalance)
        {
            disableDegradedMode();
        }
    }
    else if (!shouldActivateDegraded)
    {
        Serial.println("✅ Toutes les conditions sont normales");
    }

    return shouldActivateDegraded;
}

// ——————— GESTION MODE DÉGRADÉ ———————

void enableDegradedMode(const char *reason)
{
    if (!degradedMode)
    {
        degradedMode = true;
        systemDiag.degradedMode = true;

        Serial.printf("🚨 ACTIVATION MODE DÉGRADÉ: %s\n", reason);

        // Appliquer les limitations à toutes les batteries actives
        for (int i = 0; i < configuredBatteryCount; i++)
        {
            if (batteryStates[i].isActive)
            {
                batteryStates[i].activeLimitation = DEGRADED_MODE_LIMITATION;
            }
        }

        // Log détaillé
        Serial.println("├── Consignes limitées à 10A par batterie");
        Serial.println("├── Surveillance renforcée activée");
        Serial.println("└── Vérification recovery toutes les 30s");
    }
}

void disableDegradedMode()
{
    if (degradedMode)
    {
        degradedMode = false;
        systemDiag.degradedMode = false;

        Serial.println("✅ DÉSACTIVATION MODE DÉGRADÉ");

        // Retirer les limitations de mode dégradé (garder les autres)
        for (int i = 0; i < configuredBatteryCount; i++)
        {
            if (batteryStates[i].activeLimitation == DEGRADED_MODE_LIMITATION)
            {
                batteryStates[i].activeLimitation = NO_LIMITATION;
            }
        }

        Serial.println("└── Consignes normales rétablies");
    }
}

// ——————— GESTION DES BATTERIES ACTIVES ———————

void manageBatteryActivation()
{

    extern IndividualBatteryData individualBatteryMetrics[MAX_BATTERIES];

    Serial.println("=== GESTION ACTIVATION BATTERIES ===");

    // Trouver la batterie avec la tension la plus haute
    int highestBatteryIndex = findHighestVoltageBattery();
    if (highestBatteryIndex == -1)
    {
        Serial.println("Aucune batterie valide trouvée");
        activeBatteryCount = 0;
        return;
    }

    float maxVoltage = individualBatteryMetrics[highestBatteryIndex + 1].voltage;
    int newActiveBatteryCount = 0;

    Serial.printf("Batterie référence: ID=%d, Tension=%.3fV\n",
                  highestBatteryIndex + 2, maxVoltage);

    // Parcourir toutes les batteries pour déterminer l'activation
    for (int i = 0; i < configuredBatteryCount; i++)
    {
        int batteryId = i + 2;
        BatteryState *state = &batteryStates[i];
        IndividualBatteryData *data = &individualBatteryMetrics[i + 1];

        // Ignorer les batteries non répondantes
        if (!state->isResponding || !data->isValid)
        {
            state->isActive = false;
            state->activeLimitation = NO_LIMITATION;
            Serial.printf("  ID=%d: INACTIVE (non répondante)\n", batteryId);
            continue;
        }

        float voltageDelta = maxVoltage - data->voltage;
        state->voltageDelta = voltageDelta;

        // Logique d'activation selon delta tension
        if (voltageDelta > VOLTAGE_DIFF_THRESHOLD) // > 0.4V
        {
            // Delta trop important - batterie inactive avec limitation
            state->isActive = false;
            state->activeLimitation = VOLTAGE_LIMITATION;

            Serial.printf("  ID=%d: INACTIVE (delta=%.3fV > %.3fV) - LIMITATION\n",
                          batteryId, voltageDelta, VOLTAGE_DIFF_THRESHOLD);
        }
        else if (voltageDelta <= VOLTAGE_DIFF_COUPLE_THRESHOLD) // <= 0.3V
        {
            // Delta acceptable - batterie peut être couplée
            bool canActivate = checkBatteryCanBeActivated(i);

            if (canActivate)
            {
                state->isActive = true;
                state->activeLimitation = NO_LIMITATION;
                newActiveBatteryCount++;

                Serial.printf("  ID=%d: ACTIVE (delta=%.3fV <= %.3fV)\n",
                              batteryId, voltageDelta, VOLTAGE_DIFF_COUPLE_THRESHOLD);
            }
            else
            {
                state->isActive = false;
                Serial.printf("  ID=%d: INACTIVE (conditions non remplies)\n", batteryId);
            }
        }
        else
        {
            // Entre 0.3V et 0.4V - en attente d'équilibrage
            state->isActive = false;
            state->activeLimitation = VOLTAGE_LIMITATION;

            Serial.printf("  ID=%d: EN ATTENTE (delta=%.3fV - équilibrage)\n",
                          batteryId, voltageDelta);
        }
    }

    // Toujours activer la batterie avec la tension la plus haute
    if (highestBatteryIndex >= 0 &&
        batteryStates[highestBatteryIndex].isResponding)
    {
        // On vérifie si elle n'était pas déjà comptée comme active
        bool alreadyActive = batteryStates[highestBatteryIndex].isActive;

        // On force son statut à actif dans tous les cas
        batteryStates[highestBatteryIndex].isActive = true;
        batteryStates[highestBatteryIndex].activeLimitation = NO_LIMITATION;

        // On n'incrémente le compteur que si elle vient d'être activée par ce bloc
        if (!alreadyActive)
        {
            newActiveBatteryCount++;
        }

        Serial.printf("  ID=%d: ACTIVE (tension max - forcée)\n",
                      highestBatteryIndex + 2);
    }

    // Mettre à jour le compteur global
    activeBatteryCount = newActiveBatteryCount;
    systemDiag.activeBatteryCount = activeBatteryCount;

    Serial.printf("RÉSULTAT: %d batteries actives sur %d configurées\n",
                  activeBatteryCount, configuredBatteryCount);
}

bool checkBatteryCanBeActivated(int batteryIndex)
{
    extern IndividualBatteryData individualBatteryMetrics[MAX_BATTERIES];

    BatteryState *state = &batteryStates[batteryIndex];
    IndividualBatteryData *data = &individualBatteryMetrics[batteryIndex + 1];

    // Conditions d'activation :
    // 1. Pas de problème MOSFET
    // 2. Température acceptable
    // 3. Pas de déséquilibre critique des cellules
    // 4. Pas en mode dégradé à cause de cette batterie

    bool mosfetOk = (state->status != BATTERY_MOSFET_ISSUE);
    bool tempOk = (state->status != BATTERY_TEMP_ISSUE);
    bool balanceOk = (state->status != BATTERY_CELL_IMBALANCE);
    bool voltageOk = (state->status != BATTERY_VOLTAGE_ISSUE);

    // Vérification température en temps réel avec température moyenne des cellules
    float avgCellTemp = (data->maxCellTemp + data->minCellTemp) / 2.0f;
    bool realtimeTempOk = (avgCellTemp >= MIN_DISCHARGE_TEMP && avgCellTemp <= MAX_DISCHARGE_TEMP);

    bool canActivate = mosfetOk && tempOk && balanceOk && voltageOk && realtimeTempOk;

    if (!canActivate)
    {
        Serial.printf("    Blocage ID=%d: MOS=%s T=%s Bal=%s V=%s RT=%s\n",
                      batteryIndex + 2,
                      mosfetOk ? "OK" : "KO",
                      tempOk ? "OK" : "KO",
                      balanceOk ? "OK" : "KO",
                      voltageOk ? "OK" : "KO",
                      realtimeTempOk ? "OK" : "KO");
    }

    return canActivate;
}

void updateVoltageDeltas()
{

    extern IndividualBatteryData individualBatteryMetrics[MAX_BATTERIES];

    // Trouver la tension la plus haute
    float maxVoltage = 0.0f;
    for (int i = 0; i < configuredBatteryCount; i++)
    {
        if (batteryStates[i].isResponding &&
            individualBatteryMetrics[i + 1].isValid)
        {
            float voltage = individualBatteryMetrics[i + 1].voltage;
            if (voltage > maxVoltage)
            {
                maxVoltage = voltage;
            }
        }
    }

    // Calculer les deltas
    for (int i = 0; i < configuredBatteryCount; i++)
    {
        if (batteryStates[i].isResponding &&
            individualBatteryMetrics[i + 1].isValid)
        {
            float voltage = individualBatteryMetrics[i + 1].voltage;
            batteryStates[i].voltageDelta = maxVoltage - voltage;
        }
        else
        {
            batteryStates[i].voltageDelta = 999.0f; // Valeur d'erreur
        }
    }
}

void manageBatteryLimitations()
{

    extern IndividualBatteryData individualBatteryMetrics[MAX_BATTERIES];

    Serial.println("=== GESTION LIMITATIONS INDIVIDUELLES ===");

    for (int i = 0; i < configuredBatteryCount; i++)
    {
        int batteryId = i + 2;
        BatteryState *state = &batteryStates[i];
        IndividualBatteryData *data = &individualBatteryMetrics[i + 1];

        if (!state->isResponding || !data->isValid)
            continue;

        // Vérifier les conditions de limitation selon vos consignes originales

        // 1. Limitation si MOS charge OFF
        if (!data->chargeMosfetStatus)
        {
            if (state->activeLimitation != MOSFET_LIMITATION)
            {
                state->activeLimitation = MOSFET_LIMITATION;
                Serial.printf("  ID=%d: LIMITATION (MOS charge OFF)\n", batteryId);
            }
            // Vérifier récupération si delta < 0.3V et MOS maintenant ON
            else if (data->chargeMosfetStatus && state->voltageDelta < VOLTAGE_TOLERANCE)
            {
                state->activeLimitation = NO_LIMITATION;
                Serial.printf("  ID=%d: RÉCUPÉRATION (MOS charge ON + delta OK)\n", batteryId);
            }
        }

        // 2. Limitation si MOS décharge OFF
        if (!data->dischargeMosfetStatus)
        {
            if (state->activeLimitation != MOSFET_LIMITATION)
            {
                state->activeLimitation = MOSFET_LIMITATION;
                Serial.printf("  ID=%d: LIMITATION (MOS décharge OFF)\n", batteryId);
            }
            // Vérifier récupération
            else if (data->dischargeMosfetStatus && state->voltageDelta < VOLTAGE_TOLERANCE)
            {
                state->activeLimitation = NO_LIMITATION;
                Serial.printf("  ID=%d: RÉCUPÉRATION (MOS décharge ON + delta OK)\n", batteryId);
            }
        }

        // 3. Limitation si delta tension > 0.4V (géré dans manageBatteryActivation)
        // 4. Mode dégradé si différence température > 10°C (géré dans checkDegradedModeConditions)
        // 5. Mode dégradé si différence cellules > 250mV (géré dans checkDegradedModeConditions)
    }
}

// ——————— CALCUL DES CONSIGNES ———————

float calculateChargeSetpoint()
{

    extern IndividualBatteryData individualBatteryMetrics[MAX_BATTERIES];

    float setpoint = 0.0f;
    static char reason[150] = "";

    // On vérifie si UN SEUL MOSFET de charge est ouvert.
    for (int i = 0; i < configuredBatteryCount; i++)
    {
        IndividualBatteryData *data = &individualBatteryMetrics[i + 1];
        if (batteryStates[i].isResponding && data->isValid && !data->chargeMosfetStatus)
        {
            // Si un MOSFET est OFF, la consigne de charge pour TOUT le système est 0.
            Serial.println("SÉCURITÉ: MOSFET Charge OFF détecté. Consigne de charge globale = 0A.");
            return 0.0f; // On arrête tout et on renvoie 0.
        }
    }

    float maxCellVoltageGlobal = 0.0f;
    float minCellVoltageGlobal = 5.0f;
    float highestAvgBatteryTemp = -100.0f; // Garde la T° moyenne la plus chaude

    for (int i = 0; i < configuredBatteryCount; i++)
    {
        IndividualBatteryData *data = &individualBatteryMetrics[i + 1];
        if (!batteryStates[i].isResponding || !data->isValid)
            continue;

        if (data->cellCount != 15)
        {
            batteryStates[i].status = BATTERY_BMS_FAULT;                                 // Marquer la batterie comme défectueuse
            addSystemError(ERROR_CELL_IMBALANCE, i + 2, "Erreur BMS: Cell count != 15"); // Ajouter une erreur système
            return 0.0f;                                                                 // Règle de sécurité prioritaire : arrêter toute charge
        }
        else if (batteryStates[i].status == BATTERY_BMS_FAULT)
        {
            // Si l'erreur a été résolue, on la retire
            removeSystemError(ERROR_CELL_IMBALANCE, i + 2);
        }
    }

    for (int i = 0; i < configuredBatteryCount; i++)
    {
        IndividualBatteryData *data = &individualBatteryMetrics[i + 1];
        if (!batteryStates[i].isResponding || !data->isValid)
            continue;

        // Suivi des tensions min/max globales
        if (data->maxCellVoltage > maxCellVoltageGlobal)
            maxCellVoltageGlobal = data->maxCellVoltage;
        if (data->minCellVoltage < minCellVoltageGlobal)
            minCellVoltageGlobal = data->minCellVoltage;

        // --- NOUVELLE LOGIQUE DE TEMPÉRATURE ---
        // 1. Calculer la T° moyenne de CETTE batterie
        float currentBatteryAvgTemp = (data->maxCellTemp + data->minCellTemp) / 2.0f;
        // 2. Garder la plus haute T° moyenne rencontrée
        if (currentBatteryAvgTemp > highestAvgBatteryTemp)
        {
            highestAvgBatteryTemp = currentBatteryAvgTemp;
        }
    }

    // --- MISE À JOUR DE LA LOGIQUE DE DÉCISION ---
    if (maxCellVoltageGlobal > MAX_CHARGE_CELL_VOLTAGE)
    {
        setpoint = 0.0f;
        snprintf(reason, sizeof(reason), "Cellule > %.3fV - ARRET CHARGE", MAX_CHARGE_CELL_VOLTAGE);
    }
    else if (highestAvgBatteryTemp > MAX_CHARGE_TEMP) // NOUVELLE RÈGLE PRIORITAIRE
    {
        setpoint = 0.0f;
        snprintf(reason, sizeof(reason), "T° Moy Max %.1f°C > %.0f°C - ARRET CHARGE", highestAvgBatteryTemp, MAX_CHARGE_TEMP);
    }
    else if (degradedMode)
    {
        setpoint = DEGRADED_MODE_CURRENT * activeBatteryCount;
        snprintf(reason, sizeof(reason), "Mode dégradé actif (%d batt)", activeBatteryCount);
    }
    else if (maxCellVoltageGlobal > HIGH_CHARGE_CELL_VOLTAGE)
    {
        setpoint = LIMITED_CURRENT_PER_BATTERY * activeBatteryCount;
        snprintf(reason, sizeof(reason), "Cellule > %.3fV - LIMITATION", HIGH_CHARGE_CELL_VOLTAGE);
    }
    else if (minCellVoltageGlobal < LOW_CELL_VOLTAGE)
    {
        setpoint = 10.0f * activeBatteryCount;
        snprintf(reason, sizeof(reason), "Cellule < %.3fV - LIMITATION", LOW_CELL_VOLTAGE);
    }
    else if (highestAvgBatteryTemp < MIN_CHARGE_TEMP) // RÈGLE MISE À JOUR
    {
        setpoint = 10.0f * activeBatteryCount;
        snprintf(reason, sizeof(reason), "T° Moy Max %.1f°C < %.0f°C - LIMITATION", highestAvgBatteryTemp, MIN_CHARGE_TEMP);
    }
    else
    {
        setpoint = NORMAL_CURRENT_PER_BATTERY * activeBatteryCount;
        snprintf(reason, sizeof(reason), "Conditions normales");
    }

    return setpoint;
}

float calculateDischargeSetpoint()
{

    extern IndividualBatteryData individualBatteryMetrics[MAX_BATTERIES];

    float setpoint = 0.0f;
    static char reason[150] = "";

    for (int i = 0; i < configuredBatteryCount; i++)
    {
        IndividualBatteryData *data = &individualBatteryMetrics[i + 1];
        if (batteryStates[i].isResponding && data->isValid)
        {
            // Si le bit 0 (key) n'est pas actif
            if ((data->wakeUpSource & 0x01) == 0)
            {
                Serial.println("SÉCURITÉ: Bouton OFF détecté. Consigne de décharge globale = 0A.");
                return 0.0f; // On arrête tout et on renvoie 0.
            }
        }
    }

    // On vérifie si UN SEUL MOSFET de décharge est ouvert.
    for (int i = 0; i < configuredBatteryCount; i++)
    {
        IndividualBatteryData *data = &individualBatteryMetrics[i + 1];
        if (batteryStates[i].isResponding && data->isValid && !data->dischargeMosfetStatus)
        {
            // Si un MOSFET est OFF, la consigne de décharge pour TOUT le système est 0.
            Serial.println("SÉCURITÉ: MOSFET Décharge OFF détecté. Consigne de décharge globale = 0A.");
            return 0.0f; // On arrête tout et on renvoie 0.
        }
    }

    float minCellVoltageGlobal = 5.0f;
    float highestAvgBatteryTemp = -100.0f; // Garde la T° moyenne la plus chaude

    for (int i = 0; i < configuredBatteryCount; i++)
    {
        IndividualBatteryData *data = &individualBatteryMetrics[i + 1];
        if (!batteryStates[i].isResponding || !data->isValid)
            continue;

        if (data->minCellVoltage < minCellVoltageGlobal)
            minCellVoltageGlobal = data->minCellVoltage;
        float currentBatteryAvgTemp = (data->maxCellTemp + data->minCellTemp) / 2.0f;
        if (currentBatteryAvgTemp > highestAvgBatteryTemp)
        {
            highestAvgBatteryTemp = currentBatteryAvgTemp;
        }
    }

    for (int i = 0; i < configuredBatteryCount; i++)
    {
        IndividualBatteryData *data = &individualBatteryMetrics[i + 1];
        if (!batteryStates[i].isResponding || !data->isValid)
            continue;

        if (data->minCellVoltage < minCellVoltageGlobal)
            minCellVoltageGlobal = data->minCellVoltage;

        // --- NOUVELLE LOGIQUE DE TEMPÉRATURE ---
        float currentBatteryAvgTemp = (data->maxCellTemp + data->minCellTemp) / 2.0f;
        if (currentBatteryAvgTemp > highestAvgBatteryTemp)
        {
            highestAvgBatteryTemp = currentBatteryAvgTemp;
        }
    }

    // --- MISE À JOUR DE LA LOGIQUE DE DÉCISION ---
    if (minCellVoltageGlobal < MIN_DISCHARGE_CELL_VOLTAGE)
    {
        setpoint = 0.0f;
        snprintf(reason, sizeof(reason), "Cellule < %.3fV - ARRET DECHARGE", MIN_DISCHARGE_CELL_VOLTAGE);
    }
    else if (highestAvgBatteryTemp > MAX_DISCHARGE_TEMP) // NOUVELLE RÈGLE PRIORITAIRE
    {
        setpoint = 0.0f;
        snprintf(reason, sizeof(reason), "T° Moy Max %.1f°C > %.0f°C - ARRET DECHARGE", highestAvgBatteryTemp, MAX_DISCHARGE_TEMP);
    }
    else if (degradedMode)
    {
        setpoint = DEGRADED_MODE_CURRENT * activeBatteryCount;
        snprintf(reason, sizeof(reason), "Mode dégradé actif (%d batt)", activeBatteryCount);
    }
    else if (minCellVoltageGlobal < LOW_CELL_VOLTAGE)
    {
        setpoint = 10.0f * activeBatteryCount;
        snprintf(reason, sizeof(reason), "Cellule < %.3fV - LIMITATION", LOW_CELL_VOLTAGE);
    }
    else if (highestAvgBatteryTemp < MIN_DISCHARGE_TEMP) // RÈGLE MISE À JOUR
    {
        setpoint = 10.0f * activeBatteryCount;
        snprintf(reason, sizeof(reason), "T° Moy Max %.1f°C < %.0f°C - LIMITATION", highestAvgBatteryTemp, MIN_DISCHARGE_TEMP);
    }
    else
    {
        setpoint = NORMAL_CURRENT_PER_BATTERY * activeBatteryCount;
        snprintf(reason, sizeof(reason), "Conditions normales");
    }

    return setpoint;
}
// ——————— MOYENNES ET AGRÉGATION ———————

void calculateAverages()
{

    extern IndividualBatteryData individualBatteryMetrics[MAX_BATTERIES];
    extern AggregateBatteryMetrics latestMetrics;

    Serial.println("=== CALCUL MOYENNES ET AGRÉGATION ===");

    // Variables d'accumulation
    float totalSoc = 0.0f;
    float totalSoh = 100.0f; // SOH par défaut (pas de registre dédié dans vos BMS)
    float totalVoltage = 0.0f;
    float totalCurrent = 0.0f;
    float totalTempSystem = 0.0f;
    int validBatteries = 0;

    // Parcourir les batteries actives et répondantes
    for (int i = 0; i < configuredBatteryCount; i++)
    {
        int batteryId = i + 2;
        BatteryState *state = &batteryStates[i];
        IndividualBatteryData *data = &individualBatteryMetrics[i + 1];

        // Inclure seulement les batteries répondantes pour les moyennes
        if (!state->isResponding || !data->isValid)
        {
            Serial.printf("  ID=%d: IGNORÉ (non répondante)\n", batteryId);
            continue;
        }

        validBatteries++;

        // Accumulation des valeurs
        totalSoc += data->soc;
        totalVoltage += data->voltage;
        totalCurrent += data->current; // Cumul des courants (selon vos consignes)
        float tempBatterie = (data->minCellTemp + data->maxCellTemp) / 2.0f;
        totalTempSystem += tempBatterie;

        Serial.printf("  ID=%d: SOC=%.1f%% V=%.2fV I=%.1fA TempBat=%.1f°C\n",
                      batteryId, data->soc, data->voltage, data->current,
                      tempBatterie);
    }

    // Calculer les moyennes et mettre à jour latestMetrics
    if (validBatteries > 0)
    {
        latestMetrics.averageSoc = totalSoc / validBatteries;
        latestMetrics.averageVoltage = totalVoltage / validBatteries;
        latestMetrics.totalCurrent = totalCurrent; // Cumul, pas moyenne

        latestMetrics.averageTemp = totalTempSystem / validBatteries;

        latestMetrics.isDataValid = true;

        // Logs des résultats
        Serial.printf("MOYENNES CALCULÉES (%d batteries):\n", validBatteries);
        Serial.printf("  SOC moyen: %.1f%%\n", latestMetrics.averageSoc);
        Serial.printf("  Tension moyenne: %.2fV\n", latestMetrics.averageVoltage);
        Serial.printf("  Courant total: %.1fA\n", latestMetrics.totalCurrent);
        Serial.printf("  Température système: %.1f°C\n", latestMetrics.averageTemp);
    }
    else
    {
        latestMetrics.isDataValid = false;
        Serial.println("ERREUR: Aucune batterie valide pour le calcul des moyennes");
    }
}

uint8_t calculateSystemAlarms()
{
    uint8_t alarms = 0x00;

    // Bit 2: Alarme générale (selon votre exemple)
    if (degradedMode ||
        respondingBatteryCount < configuredBatteryCount ||
        systemDiag.maxBatteryTemp > (MAX_DISCHARGE_TEMP - 5.0f)) // Pré-alarme à 45°C
    {
        alarms |= 0x04; // Bit 2 actif
    }

    // Bit 1: Alarme température
    if (systemDiag.maxBatteryTemp > MAX_CHARGE_TEMP)
    {
        alarms |= 0x02; // Bit 1 actif
    }

    // Bit 0: Alarme tension
    if (systemDiag.maxBatteryVoltage - systemDiag.minBatteryVoltage > VOLTAGE_DIFF_THRESHOLD)
    {
        alarms |= 0x01; // Bit 0 actif
    }

    return alarms;
}

uint8_t calculateSystemProtections()
{
    uint8_t protections = 0x00;

    extern IndividualBatteryData individualBatteryMetrics[MAX_BATTERIES];

    for (int i = 0; i < configuredBatteryCount; i++)
    {
        IndividualBatteryData *data = &individualBatteryMetrics[i + 1];
        if (!batteryStates[i].isResponding || !data->isValid)
            continue;

        if (data->maxCellVoltage > MAX_CHARGE_CELL_VOLTAGE)
        {
            protections |= 0x10; // Bit 4: Protection surtension
        }
        if (data->minCellVoltage < MIN_DISCHARGE_CELL_VOLTAGE)
        {
            protections |= 0x08; // Bit 3: Protection sous-tension
        }
        if (data->maxCellTemp > MAX_DISCHARGE_TEMP)
        {
            protections |= 0x04; // Bit 2: Protection température
        }
        if (!data->chargeMosfetStatus || !data->dischargeMosfetStatus)
        {
            protections |= 0x02; // Bit 1: Protection MOSFET
        }
    }

    if (degradedMode)
    {
        protections |= 0x01; // Bit 0: Protection système
    }
    return protections;
}
// ——————— FONCTIONS UTILITAIRES ———————

int findHighestVoltageBattery()
{

    extern IndividualBatteryData individualBatteryMetrics[MAX_BATTERIES];

    int highestIndex = -1;
    float highestVoltage = 0.0f;

    for (int i = 0; i < configuredBatteryCount; i++)
    {
        if (batteryStates[i].isResponding &&
            individualBatteryMetrics[i + 1].isValid)
        {
            float voltage = individualBatteryMetrics[i + 1].voltage;
            if (voltage > highestVoltage)
            {
                highestVoltage = voltage;
                highestIndex = i;
            }
        }
    }

    return highestIndex;
}

// ——————— GESTION DES LEDS D'ÉTAT ———————

void initStatusLeds()
{
    pinMode(LED_RED_PIN, OUTPUT);
    digitalWrite(LED_RED_PIN, LOW); // LED rouge éteinte au démarrage

    Serial.println("✓ LEDs d'état initialisées");
}

void updateStatusLeds()
{
    bool shouldShowAlarm = false;

    // Conditions d'alarme (selon votre logique système)
    if (degradedMode ||
        respondingBatteryCount < configuredBatteryCount ||
        systemDiag.maxBatteryTemp > MAX_DISCHARGE_TEMP ||
        (systemDiag.maxBatteryVoltage - systemDiag.minBatteryVoltage) > VOLTAGE_DIFF_THRESHOLD)
    {
        shouldShowAlarm = true;
    }

    // Vérifier les alarmes individuelles des batteries
    for (int i = 0; i < configuredBatteryCount; i++)
    {
        BatteryState *state = &batteryStates[i];
        if (state->status == BATTERY_MOSFET_ISSUE ||
            state->status == BATTERY_TEMP_ISSUE ||
            state->status == BATTERY_CELL_IMBALANCE ||
            state->activeLimitation != NO_LIMITATION)
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
    Serial.println("✓ Système de gestion des erreurs initialisé");
}

void addSystemError(ErrorType type, int batteryId, const char *description)
{
    unsigned long now = millis();

    // Chercher si l'erreur existe déjà
    for (int i = 0; i < MAX_SYSTEM_ERRORS; i++)
    {
        if (systemErrors[i].active &&
            systemErrors[i].type == type &&
            systemErrors[i].batteryId == batteryId)
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

            Serial.printf("NOUVELLE ERREUR: %s (Batt ID=%d)\n", description, batteryId);
            break;
        }
    }
}

void removeSystemError(ErrorType type, int batteryId)
{
    for (int i = 0; i < MAX_SYSTEM_ERRORS; i++)
    {
        if (systemErrors[i].active &&
            systemErrors[i].type == type &&
            systemErrors[i].batteryId == batteryId)
        {
            systemErrors[i].active = false;
            errorCount--;
            Serial.printf("ERREUR RÉSOLUE: %s (Batt ID=%d)\n", systemErrors[i].description, batteryId);
            break;
        }
    }
}

void checkWakeUpSource()
{

    extern IndividualBatteryData individualBatteryMetrics[MAX_BATTERIES];

    Serial.println("=== VÉRIFICATION SOURCE DE RÉVEIL (KEY) ===");

    for (int i = 0; i < configuredBatteryCount; i++)
    {
        int batteryId = i + 2;
        BatteryState *state = &batteryStates[i];
        IndividualBatteryData *data = &individualBatteryMetrics[i + 1];

        if (!state->isResponding || !data->isValid)
            continue;

        // On vérifie le bit 0 (valeur 0x01, "key") du registre Wake-up source
        bool isKeyOn = (data->wakeUpSource & 0x01) != 0;

        if (!isKeyOn) // <-- CORRIGÉ ICI
        {
            // La clé est sur OFF
            Serial.printf("ERREUR: Batterie ID=%d a sa clé (key) sur OFF.\n", batteryId);
            addSystemError(ERROR_WAKEUP_BUTTON_OFF, batteryId, "Cle (key) OFF");
        }
        else
        {
            // La clé est sur ON, on retire l'erreur si elle existait
            removeSystemError(ERROR_WAKEUP_BUTTON_OFF, batteryId);
        }
    }
}

void checkBmsFaults()
{
    Serial.println("=== VÉRIFICATION DÉFAUTS INTERNES BMS ===");

    for (int i = 0; i < configuredBatteryCount; i++)
    {
        int batteryId = i + 2;
        IndividualBatteryData *data = &individualBatteryMetrics[i + 1];

        if (!batteryStates[i].isResponding || !data->isValid)
            continue;

        // --- ÉTAPE 1 : On récupère toutes les valeurs de défauts ---
        uint16_t faultCode0_1 = data->faultCode0_1;
        uint16_t faultCode2_3 = data->faultCode2_3;
        uint16_t faultCode4_5 = data->faultCode4_5;
        uint16_t faultCode6_7 = data->faultCode6_7;
        uint16_t faultCode10_11 = data->faultCode10_11;
        uint16_t faultCode12_13 = data->faultCode12_13; // NOUVEAU

        // --- ÉTAPE 2 : On analyse les bits pour chaque type de problème ---

        // Problèmes de Tension Cellules (registre 0x6D)
        if ((faultCode0_1 & 0x08) != 0)
            addSystemError(ERROR_UNDERVOLTAGE, batteryId, "BMS: Cellule Sous-tension");
        else
            removeSystemError(ERROR_UNDERVOLTAGE, batteryId);
        if ((faultCode0_1 & 0x04) != 0)
            addSystemError(ERROR_OVERVOLTAGE, batteryId, "BMS: Cellule Surtension");
        else
            removeSystemError(ERROR_OVERVOLTAGE, batteryId);

        // Problèmes de Température
        bool isOverTemp = (faultCode0_1 >> 8 & 0x20) || (faultCode2_3 & 0x20) || (faultCode2_3 & 0x40) || (faultCode2_3 >> 8 & 0x40) || (faultCode6_7 >> 8 & 0x04);
        bool isUnderTemp = (faultCode2_3 & 0x04) || (faultCode2_3 >> 8 & 0x04);
        if (isOverTemp)
            addSystemError(ERROR_OVERTEMPERATURE, batteryId, "BMS: Surchauffe");
        else
            removeSystemError(ERROR_OVERTEMPERATURE, batteryId);
        if (isUnderTemp)
            addSystemError(ERROR_UNDERTEMPERATURE, batteryId, "BMS: Sous-chauffe");
        else
            removeSystemError(ERROR_UNDERTEMPERATURE, batteryId);

        // Problèmes de Courant et Sécurité (registre 0x6F)
        if ((faultCode4_5 & 0x40) != 0)
            addSystemError(ERROR_SHORT_CIRCUIT, batteryId, "BMS: Court-circuit");
        else
            removeSystemError(ERROR_SHORT_CIRCUIT, batteryId);
        if ((faultCode4_5 >> 8 & 0x04) != 0)
            addSystemError(ERROR_OVERCURRENT_CHARGE, batteryId, "BMS: Surintensite Charge");
        else
            removeSystemError(ERROR_OVERCURRENT_CHARGE, batteryId);
        if ((faultCode4_5 >> 8 & 0x20) != 0)
            addSystemError(ERROR_OVERCURRENT_DISCHARGE, batteryId, "BMS: Surintensite Decharge");
        else
            removeSystemError(ERROR_OVERCURRENT_DISCHARGE, batteryId);
        if ((faultCode4_5 >> 8 & 0x40) != 0)
            addSystemError(ERROR_CHARGE_PROHIBITED, batteryId, "BMS: Charge interdite (V basse)");
        else
            removeSystemError(ERROR_CHARGE_PROHIBITED, batteryId);
        if ((faultCode4_5 >> 8 & 0x80) != 0)
            addSystemError(ERROR_DISCHARGE_PROHIBITED, batteryId, "BMS: Decharge interdite (V haute)");
        else
            removeSystemError(ERROR_DISCHARGE_PROHIBITED, batteryId);

        // Pannes matérielles (registres 0x72 et 0x73)
        uint8_t high_byte_10_11 = faultCode10_11 >> 8;
        uint8_t low_byte_12_13 = faultCode12_13 & 0xFF; // NOUVEAU

        // Si n'importe quel bit de panne est actif, on lève l'alarme unique
        if (high_byte_10_11 != 0 || low_byte_12_13 != 0)
        {
            addSystemError(ERROR_BMS_INTERNAL_FAULT, batteryId, "BMS: Batterie en panne");
        }
        else
        {
            removeSystemError(ERROR_BMS_INTERNAL_FAULT, batteryId);
        }
    }
}
