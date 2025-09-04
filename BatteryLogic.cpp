#include "BatteryLogic.h"

BatteryState batteryStates[MAX_BATTERIES];
SystemDiagnostics systemDiag;
float currentChargeSetpoint = 0.0f;
float currentDischargeSetpoint = 0.0f;
int activeBatteryCount = 0;
int respondingBatteryCount = 0;
bool degradedMode = false;
bool systemInitialized = false;
extern int configuredBatteryCount;
// Variables globales pour les erreurs
SystemError systemErrors[MAX_SYSTEM_ERRORS];
int errorCount = 0;

// ——————— INITIALISATION ET SYSTÈME ———————

void initBatteryManagement()
{
    Serial.println("=== INITIALISATION GESTION BATTERIES ===");

    // Initialisation des états des batteries
    for (int i = 0; i < MAX_BATTERIES; i++)
    {
        batteryStates[i].isResponding = false;
        batteryStates[i].lastResponseTime = 0;
        batteryStates[i].status = BATTERY_NOT_RESPONDING;
        batteryStates[i].activeLimitation = NO_LIMITATION;
        batteryStates[i].isActive = false;
        batteryStates[i].voltageDelta = 0.0f;
        batteryStates[i].lastMosfetCheck = 0;
    }

    // Initialisation du diagnostic système
    systemDiag.degradedMode = false;
    systemDiag.activeBatteryCount = 0;
    systemDiag.respondingBatteryCount = 0;
    systemDiag.maxBatteryVoltage = 0.0f;
    systemDiag.minBatteryVoltage = 999.0f;
    systemDiag.maxBatteryTemp = -100.0f;
    systemDiag.minBatteryTemp = 999.0f;
    systemDiag.lastDiagnostic = 0;

    // Initialisation des consignes
    currentChargeSetpoint = 0.0f;
    currentDischargeSetpoint = 0.0f;

    // Initialisation des compteurs
    activeBatteryCount = 0;
    respondingBatteryCount = 0;

    // Flags
    degradedMode = false;
    systemInitialized = false;

    initErrorSystem();

    Serial.println("✓ Structures de gestion batteries initialisées");
}

void runBatteryManagementCycle()
{
    Serial.println("\n=== CYCLE GESTION INTELLIGENTE BATTERIES ===");

    // Mise à jour des deltas de tension
    updateVoltageDeltas();

    // Gestion des limitations individuelles
    manageBatteryLimitations();

    // Gestion de l'activation des batteries
    manageBatteryActivation();

    // Calcul des moyennes et agrégation
    calculateAverages();

    // Calcul des nouvelles consignes
    currentChargeSetpoint = calculateChargeSetpoint();
    currentDischargeSetpoint = calculateDischargeSetpoint();

    // Mise à jour des LEDs d'état
    updateStatusLeds();

    Serial.printf("CYCLE TERMINÉ - Consignes: Charge=%.1fA Décharge=%.1fA\n",
                  currentChargeSetpoint, currentDischargeSetpoint);
    Serial.println("================================================\n");
}

void printSystemStatus()
{
    extern int configuredBatteryCount;

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
    Serial.println("================================\n");
}

void updateSystemMetrics()
{
    extern int configuredBatteryCount;
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
        int batteryId = i + 2; // ID commence à 2
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

            // Mise à jour min/max températures (on prend la plus haute des deux sondes)
            float maxTemp = max(data->temp1, data->temp2);
            if (maxTemp > systemDiag.maxBatteryTemp)
                systemDiag.maxBatteryTemp = maxTemp;
            if (maxTemp < systemDiag.minBatteryTemp)
                systemDiag.minBatteryTemp = maxTemp;
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
    extern int configuredBatteryCount;
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
    Serial.println("================================");

    return currentRespondingCount;
}

void updateBatteryState(int batteryIndex, bool isResponding)
{
    if (batteryIndex < 0 || batteryIndex >= MAX_BATTERIES)
        return;

    BatteryState *state = &batteryStates[batteryIndex];

    if (isResponding)
    {
        state->isResponding = true;
        state->lastResponseTime = millis();
        state->status = BATTERY_OK; // On affinera plus tard avec les autres vérifications
    }
    else
    {
        state->isResponding = false;
        state->status = BATTERY_NOT_RESPONDING;
    }
}

// ——————— DIAGNOSTIC ET DÉTECTION DES PANNES ———————

bool checkMosfetStatus()
{
    extern int configuredBatteryCount;
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

    Serial.println("================================");
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
    bool tempOk = (data->temp1 < MAX_MOS_TEMP) && (data->temp2 < MAX_MOS_TEMP) &&
                  (data->mosTemp < MAX_MOS_TEMP);

    return mosOk && voltageOk && tempOk;
}

bool checkDegradedModeConditions()
{
    extern int configuredBatteryCount;
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
        if (checkCanExitDegradedMode())
        {
            disableDegradedMode();
        }
    }
    else if (!shouldActivateDegraded)
    {
        Serial.println("✅ Toutes les conditions sont normales");
    }

    Serial.println("================================");
    return shouldActivateDegraded;
}

bool checkCanExitDegradedMode()
{
    /*
    Pour sortir du mode dégradé, toutes les conditions de sécurité
    doivent être revenues à la normale pour garantir un fonctionnement optimal.
    1. Toutes les batteries configurées répondent
    2. Pas de surtempérature
    3. Pas de déséquilibrage critique
    4. Delta température acceptable
    */
    bool allBatteriesRespond = (respondingBatteryCount == configuredBatteryCount);
    bool tempOk = (systemDiag.maxBatteryTemp <= MAX_DISCHARGE_TEMP);
    bool tempDeltaOk = (systemDiag.maxBatteryTemp - systemDiag.minBatteryTemp <= TEMP_DIFF_THRESHOLD);
    bool noCriticalImbalance = true;

    // Vérifier l'équilibrage de toutes les batteries
    extern int configuredBatteryCount;
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

    bool canExit = allBatteriesRespond && tempOk && tempDeltaOk && noCriticalImbalance;

    if (!canExit)
    {
        Serial.printf("⏳ Mode dégradé maintenu: Batt:%s Temp:%s DeltaT:%s Equil:%s\n",
                      allBatteriesRespond ? "OK" : "KO",
                      tempOk ? "OK" : "KO",
                      tempDeltaOk ? "OK" : "KO",
                      noCriticalImbalance ? "OK" : "KO");
    }

    return canExit;
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
    extern int configuredBatteryCount;
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
        batteryStates[highestBatteryIndex].isActive = true;
        batteryStates[highestBatteryIndex].activeLimitation = NO_LIMITATION;

        if (batteryStates[highestBatteryIndex].voltageDelta == 0)
        {
            newActiveBatteryCount++; // Pas déjà comptée
        }

        Serial.printf("  ID=%d: ACTIVE (tension max - forcée)\n",
                      highestBatteryIndex + 2);
    }

    // Mettre à jour le compteur global
    activeBatteryCount = newActiveBatteryCount;
    systemDiag.activeBatteryCount = activeBatteryCount;

    Serial.printf("RÉSULTAT: %d batteries actives sur %d configurées\n",
                  activeBatteryCount, configuredBatteryCount);
    Serial.println("================================");
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

    // Vérification température en temps réel
    float maxTemp = max(data->temp1, data->temp2);
    bool realtimeTempOk = (maxTemp >= MIN_DISCHARGE_TEMP && maxTemp <= MAX_DISCHARGE_TEMP);

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
    extern int configuredBatteryCount;
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
    extern int configuredBatteryCount;
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

    Serial.println("================================");
}

// ——————— CALCUL DES CONSIGNES ———————

float calculateChargeSetpoint()
{
    extern int configuredBatteryCount;
    extern IndividualBatteryData individualBatteryMetrics[MAX_BATTERIES];

    float setpoint = 0.0f;
    static char reason[150] = "";

    // Variables pour l'analyse des conditions
    float maxCellVoltage = 0.0f;
    float minCellVoltage = 5.0f;
    float totalBatteryTemp = 0.0f;
    float maxMosTemp = 0.0f;
    int validBatteries = 0;
    bool hasCellAbove3480 = false;
    bool hasCellAbove3450 = false;
    bool hasCellBelow3000 = false;

    // Analyser toutes les batteries répondantes pour trouver les conditions critiques
    for (int i = 0; i < configuredBatteryCount; i++)
    {
        BatteryState *state = &batteryStates[i];
        IndividualBatteryData *data = &individualBatteryMetrics[i + 1];

        if (!state->isResponding || !data->isValid)
            continue;

        validBatteries++;

        // Analyser les tensions des 15 cellules de cette batterie
        for (int cell = 0; cell < 15; cell++)
        {
            float cellVoltage = data->cellVoltages[cell];

            // Vérifier les seuils critiques
            if (cellVoltage > 3.480f)
                hasCellAbove3480 = true;
            if (cellVoltage > 3.450f)
                hasCellAbove3450 = true;
            if (cellVoltage < 3.000f)
                hasCellBelow3000 = true;

            // Garder les valeurs min/max pour debug
            if (cellVoltage > maxCellVoltage)
                maxCellVoltage = cellVoltage;
            if (cellVoltage < minCellVoltage)
                minCellVoltage = cellVoltage;
        }

        // Accumuler les températures des batteries
        totalBatteryTemp += (data->temp1 + data->temp2) / 2.0f;

        // Température MOS maximum
        if (data->mosTemp > maxMosTemp)
            maxMosTemp = data->mosTemp;
    }

    // Calculer la température moyenne des batteries
    float avgBatteryTemp = (validBatteries > 0) ? totalBatteryTemp / validBatteries : 0.0f;

    // ————————— LOGIQUE ELSE-IF EXACTE SELON VOS CONSIGNES —————————

    if (hasCellAbove3480) // Dès qu'une cellule > 3480mV
    {
        setpoint = 0.0f;
        snprintf(reason, sizeof(reason),
                 "Cellule > 3.480V - ARRÊT CHARGE (max trouvée: %.3fV)",
                 maxCellVoltage);
    }
    else if (degradedMode) // Si le mode dégradé est vrai
    {
        setpoint = DEGRADED_MODE_CURRENT; // 10.0f;
        snprintf(reason, sizeof(reason), "Mode dégradé actif");
    }
    else if (hasCellAbove3450) // Dès qu'une cellule > 3450mV
    {
        setpoint = LIMITED_CURRENT_PER_BATTERY * activeBatteryCount; // 10.0f;
        snprintf(reason, sizeof(reason),
                 "Cellule > 3.450V - LIMITATION (max: %.3fV, %d batt actives)",
                 maxCellVoltage, activeBatteryCount);
    }
    else if (hasCellBelow3000) // Dès qu'une cellule < 3000mV
    {
        setpoint = 10.0f * activeBatteryCount;
        snprintf(reason, sizeof(reason),
                 "Cellule < 3.000V - LIMITATION (min: %.3fV, %d batt actives)",
                 minCellVoltage, activeBatteryCount);
    }
    else if (avgBatteryTemp < 8.0f || avgBatteryTemp > 45.0f) // Température hors plage
    {
        setpoint = 10.0f * activeBatteryCount;
        snprintf(reason, sizeof(reason),
                 "Temp batteries %.1f°C hors [8-45°C] (%d batt actives)",
                 avgBatteryTemp, activeBatteryCount);
    }
    else if (maxMosTemp > 80.0f) // Temp MOS > 80°C
    {
        setpoint = 10.0f * configuredBatteryCount; // "nb de batteries" dans consignes
        snprintf(reason, sizeof(reason),
                 "Temp MOS %.1f°C > 80°C (%d batteries total)",
                 maxMosTemp, configuredBatteryCount);
    }
    else // Conditions normales
    {
        setpoint = NORMAL_CURRENT_PER_BATTERY * activeBatteryCount;
        snprintf(reason, sizeof(reason),
                 "Conditions normales (%d batt actives)",
                 activeBatteryCount);
    }

    // Logs détaillés
    Serial.printf("CONSIGNE CHARGE: %.1fA (%s)\n", setpoint, reason);
    Serial.printf("  Analyse: MaxCell=%.3fV MinCell=%.3fV TempMoy=%.1f°C MaxMOS=%.1f°C\n",
                  maxCellVoltage, minCellVoltage, avgBatteryTemp, maxMosTemp);
    Serial.printf("  Flags: >3480=%s >3450=%s <3000=%s Dégradé=%s\n",
                  hasCellAbove3480 ? "OUI" : "NON",
                  hasCellAbove3450 ? "OUI" : "NON",
                  hasCellBelow3000 ? "OUI" : "NON",
                  degradedMode ? "OUI" : "NON");

    return setpoint;
}

float calculateDischargeSetpoint()
{
    extern int configuredBatteryCount;
    extern IndividualBatteryData individualBatteryMetrics[MAX_BATTERIES];

    float setpoint = 0.0f;
    static char reason[150] = "";

    // Variables pour l'analyse des conditions
    float minCellVoltage = 5.0f;
    float maxCellVoltage = 0.0f;
    float totalBatteryTemp = 0.0f;
    float maxMosTemp = 0.0f;
    int validBatteries = 0;
    bool hasCellBelow2750 = false;
    bool hasCellBelow3000 = false;

    // Analyser toutes les batteries répondantes
    for (int i = 0; i < configuredBatteryCount; i++)
    {
        BatteryState *state = &batteryStates[i];
        IndividualBatteryData *data = &individualBatteryMetrics[i + 1];

        if (!state->isResponding || !data->isValid)
            continue;

        validBatteries++;

        // Analyser les tensions des 15 cellules de cette batterie
        for (int cell = 0; cell < 15; cell++)
        {
            float cellVoltage = data->cellVoltages[cell];

            // Vérifier les seuils critiques
            if (cellVoltage < 2.750f)
                hasCellBelow2750 = true;
            if (cellVoltage < 3.000f)
                hasCellBelow3000 = true;

            // Garder les valeurs min/max pour debug
            if (cellVoltage < minCellVoltage)
                minCellVoltage = cellVoltage;
            if (cellVoltage > maxCellVoltage)
                maxCellVoltage = cellVoltage;
        }

        // Accumuler les températures des batteries
        totalBatteryTemp += (data->temp1 + data->temp2) / 2.0f;

        // Température MOS maximum
        if (data->mosTemp > maxMosTemp)
            maxMosTemp = data->mosTemp;
    }

    // Calculer la température moyenne des batteries
    float avgBatteryTemp = (validBatteries > 0) ? totalBatteryTemp / validBatteries : 0.0f;

    // ————————— LOGIQUE ELSE-IF EXACTE SELON VOS CONSIGNES —————————

    if (hasCellBelow2750) // Dès qu'une cellule < 2750mV
    {
        setpoint = 0.0f;
        snprintf(reason, sizeof(reason),
                 "Cellule < 2.750V - ARRÊT DÉCHARGE (min trouvée: %.3fV)",
                 minCellVoltage);
    }
    else if (degradedMode) // Si le mode dégradé est vrai
    {
        setpoint = 10.0f;
        snprintf(reason, sizeof(reason), "Mode dégradé actif");
    }
    else if (hasCellBelow3000) // Dès qu'une cellule < 3000mV
    {
        setpoint = 10.0f * activeBatteryCount;
        snprintf(reason, sizeof(reason),
                 "Cellule < 3.000V - LIMITATION (min: %.3fV, %d batt actives)",
                 minCellVoltage, activeBatteryCount);
    }
    else if (avgBatteryTemp < 3.0f || avgBatteryTemp > 50.0f) // Température hors plage
    {
        setpoint = 10.0f * activeBatteryCount;
        snprintf(reason, sizeof(reason),
                 "Temp batteries %.1f°C hors [3-50°C] (%d batt actives)",
                 avgBatteryTemp, activeBatteryCount);
    }
    else if (maxMosTemp > 80.0f) // Temp MOS > 80°C
    {
        setpoint = 10.0f * configuredBatteryCount; // "nb de batteries" dans consignes
        snprintf(reason, sizeof(reason),
                 "Temp MOS %.1f°C > 80°C (%d batteries total)",
                 maxMosTemp, configuredBatteryCount);
    }
    else // Conditions normales
    {
        setpoint = 150.0f * activeBatteryCount;
        snprintf(reason, sizeof(reason),
                 "Conditions normales (%d batt actives)",
                 activeBatteryCount);
    }

    // Logs détaillés
    Serial.printf("CONSIGNE DÉCHARGE: %.1fA (%s)\n", setpoint, reason);
    Serial.printf("  Analyse: MinCell=%.3fV MaxCell=%.3fV TempMoy=%.1f°C MaxMOS=%.1f°C\n",
                  minCellVoltage, maxCellVoltage, avgBatteryTemp, maxMosTemp);
    Serial.printf("  Flags: <2750=%s <3000=%s Dégradé=%s\n",
                  hasCellBelow2750 ? "OUI" : "NON",
                  hasCellBelow3000 ? "OUI" : "NON",
                  degradedMode ? "OUI" : "NON");

    return setpoint;
}

// ——————— MOYENNES ET AGRÉGATION ———————

void calculateAverages()
{
    extern int configuredBatteryCount;
    extern IndividualBatteryData individualBatteryMetrics[MAX_BATTERIES];
    extern AggregateBatteryMetrics latestMetrics;

    Serial.println("=== CALCUL MOYENNES ET AGRÉGATION ===");

    // Variables d'accumulation
    float totalSoc = 0.0f;
    float totalSoh = 100.0f; // SOH par défaut (pas de registre dédié dans vos BMS)
    float totalVoltage = 0.0f;
    float totalCurrent = 0.0f;
    float totalTemp1 = 0.0f;
    float totalTemp2 = 0.0f;
    float totalMosTemp = 0.0f;
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
        totalTemp1 += data->temp1;
        totalTemp2 += data->temp2;
        totalMosTemp += data->mosTemp;

        Serial.printf("  ID=%d: SOC=%.1f%% V=%.2fV I=%.1fA T1=%.1f°C T2=%.1f°C\n",
                      batteryId, data->soc, data->voltage, data->current,
                      data->temp1, data->temp2);
    }

    // Calculer les moyennes et mettre à jour latestMetrics
    if (validBatteries > 0)
    {
        latestMetrics.averageSoc = totalSoc / validBatteries;
        latestMetrics.averageVoltage = totalVoltage / validBatteries;
        latestMetrics.totalCurrent = totalCurrent; // Cumul, pas moyenne

        // Température moyenne des batteries (moyenne des sondes T1 et T2)
        float avgTemp1 = totalTemp1 / validBatteries;
        float avgTemp2 = totalTemp2 / validBatteries;
        latestMetrics.averageTemp = (avgTemp1 + avgTemp2) / 2.0f;

        // Température moyenne des MOSFETs
        float avgMosTemp = totalMosTemp / validBatteries;

        latestMetrics.isDataValid = true;

        // Logs des résultats
        Serial.printf("MOYENNES CALCULÉES (%d batteries):\n", validBatteries);
        Serial.printf("  SOC moyen: %.1f%%\n", latestMetrics.averageSoc);
        Serial.printf("  Tension moyenne: %.2fV\n", latestMetrics.averageVoltage);
        Serial.printf("  Courant total: %.1fA\n", latestMetrics.totalCurrent);
        Serial.printf("  Température moyenne: %.1f°C\n", latestMetrics.averageTemp);
        Serial.printf("  Température MOS moyenne: %.1f°C\n", avgMosTemp);
    }
    else
    {
        latestMetrics.isDataValid = false;
        Serial.println("ERREUR: Aucune batterie valide pour le calcul des moyennes");
    }

    Serial.println("================================");
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

    // Analyser les batteries pour détecter les protections actives
    extern int configuredBatteryCount;
    extern IndividualBatteryData individualBatteryMetrics[MAX_BATTERIES];

    for (int i = 0; i < configuredBatteryCount; i++)
    {
        BatteryState *state = &batteryStates[i];
        IndividualBatteryData *data = &individualBatteryMetrics[i + 1];

        if (!state->isResponding || !data->isValid)
            continue;

        // Vérifier les tensions cellules pour protections
        for (int cell = 0; cell < 15; cell++)
        {
            float cellVoltage = data->cellVoltages[cell];

            // Protection surtension cellule
            if (cellVoltage > MAX_CHARGE_CELL_VOLTAGE)
            {
                protections |= 0x10; // Bit 4: Protection surtension
            }

            // Protection sous-tension cellule
            if (cellVoltage < MIN_DISCHARGE_CELL_VOLTAGE)
            {
                protections |= 0x08; // Bit 3: Protection sous-tension
            }
        }

        // Protection surtempérature
        if (data->temp1 > MAX_DISCHARGE_TEMP || data->temp2 > MAX_DISCHARGE_TEMP)
        {
            protections |= 0x04; // Bit 2: Protection température
        }

        // Protection MOSFET
        if (!data->chargeMosfetStatus || !data->dischargeMosfetStatus)
        {
            protections |= 0x02; // Bit 1: Protection MOSFET
        }
    }

    // Protection mode dégradé global
    if (degradedMode)
    {
        protections |= 0x01; // Bit 0: Protection système
    }

    return protections;
}

// ——————— FONCTIONS UTILITAIRES ———————

int findHighestVoltageBattery()
{
    extern int configuredBatteryCount;
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

int getActiveErrorCount()
{
    return errorCount;
}