#include "CanBusManager.h"
#include "ModbusManager.h"
#include "BatteryLogic.h"

// ——————— VARIABLES GLOBALES ———————
CanFrame canFrame;
char lastCanFrames[5][50];     // Stockage texte des trames pour affichage
bool canDisplayActive = false; // État affichage temps réel des trames

// ——————— FONCTIONS D'INITIALISATION ———————
bool initCanBus()
{
    Serial.println("Initialisation CAN Bus...");

    // Configuration des pins et paramètres
    ESP32Can.setPins(CAN_TX_PIN, CAN_RX_PIN);
    ESP32Can.setRxQueueSize(5);
    ESP32Can.setTxQueueSize(5);
    ESP32Can.setSpeed(ESP32Can.convertSpeed(CAN_SPEED_KBPS));

    // Démarrage du CAN
    if (!ESP32Can.begin())
    {
        Serial.println("ERREUR: Échec initialisation CAN!");
        return false;
    }

    Serial.printf("CAN Bus initialisé - Speed: %d kbps, TX: %d, RX: %d\n",
                  CAN_SPEED_KBPS, CAN_TX_PIN, CAN_RX_PIN);
    return true;
}

// ——————— ENVOI DES TRAMES CAN ———————
void sendCanData()
{
    // Envoyer toutes les trames
    sendChargeLimits();       // 0x351
    sendSocSoh();             // 0x355
    sendVoltageCurrentTemp(); // 0x356
    sendAlarms();             // 0x359
    sendRequests();           // 0x35C

    // Mettre à jour l'affichage des trames si actif
    if (canDisplayActive)
    {
        updateCanFrameDisplay();
    }
}

void sendChargeLimits()
{
    canFrame = {0};
    canFrame.identifier = CAN_ID_LIMITS;
    canFrame.extd = 0;
    canFrame.data_length_code = 8;

    // Tension de charge max: 51.6V = 516 = 0x0204
    uint16_t vchg = 516;

    // Courants en 0.1A (consignes variables 0-600A)
    uint16_t ichg = (uint16_t)(currentChargeSetpoint * 10);
    uint16_t idis = (uint16_t)(currentDischargeSetpoint * 10);

    // Format little-endian selon la doc
    canFrame.data[0] = lowByte(vchg);  // 0x04
    canFrame.data[1] = highByte(vchg); // 0x02
    canFrame.data[2] = lowByte(ichg);  // Variable selon consigne
    canFrame.data[3] = highByte(ichg); // Variable selon consigne
    canFrame.data[4] = lowByte(idis);  // Variable selon consigne
    canFrame.data[5] = highByte(idis); // Variable selon consigne
    canFrame.data[6] = 0xC9;           // Fixe
    canFrame.data[7] = 0x01;           // Fixe

    ESP32Can.writeFrame(canFrame);
    Serial.printf("CAN 0x351: V=51.6V, Ich=%.1fA, Idch=%.1fA\n",
                  currentChargeSetpoint, currentDischargeSetpoint);
}

void sendSocSoh()
{
    canFrame = {0};
    canFrame.identifier = CAN_ID_SOC_SOH;
    canFrame.extd = 0;
    canFrame.data_length_code = 8;

    // SOC DYNAMIQUE: Utilise la moyenne calculée des batteries
    extern AggregateBatteryMetrics latestMetrics;
    uint16_t soc = 0;

    if (latestMetrics.isDataValid)
    {
        // Conversion du SOC moyen vers entier avec arrondi
        soc = (uint16_t)round(latestMetrics.averageSoc);

        // Sécurité: limiter entre 0 et 100%
        if (soc > 100)
            soc = 100;
    }
    else
    {
        // Valeur par défaut si pas de données valides
        soc = 0;
    }
    uint16_t soh = 100; // 100%

    // Format little-endian selon la doc
    canFrame.data[0] = lowByte(soc);  // 0x14
    canFrame.data[1] = highByte(soc); // 0x00
    canFrame.data[2] = lowByte(soh);  // 0x64
    canFrame.data[3] = highByte(soh); // 0x00
    canFrame.data[4] = 0xD0;          // Fixe
    canFrame.data[5] = 0x07;          // Fixe
    canFrame.data[6] = 0x00;          // Fixe
    canFrame.data[7] = 0x00;          // Fixe

    ESP32Can.writeFrame(canFrame);
    Serial.printf("CAN 0x355: SOC=%d%%, SOH=%d%%\n", soc, soh);
}

void sendVoltageCurrentTemp()
{
    canFrame = {0};
    canFrame.identifier = CAN_ID_VOLTAGE_CURRENT;
    canFrame.extd = 0;
    canFrame.data_length_code = 8;

    // Tension: 42.00V = 4200 = 0x1068
    // Courant: 0.0A = 0 = 0x0000
    // Température: 27.0°C = 270 = 0x010E
    uint16_t voltage = 4200;  // 42.00V en 0.01V
    uint16_t current = 30000; // 0.0A
    uint16_t temp = 270;      // 27.0°C en 0.1°C

    extern AggregateBatteryMetrics latestMetrics;

    if (latestMetrics.isDataValid)
    {
        // Tension: Conversion V vers 0.01V (ex: 48.5V → 4850)
        voltage = (uint16_t)(latestMetrics.averageVoltage * 100);

        // Courant: Conversion A vers 0.01A avec offset 30000
        // Positif = charge, Négatif = décharge
        int32_t currentRaw = (int32_t)(latestMetrics.totalCurrent * 10);
        current = (uint16_t)(currentRaw + 30000);

        // Température: Conversion °C vers 0.1°C (ex: 35.2°C → 352)
        temp = (uint16_t)(latestMetrics.averageTemp * 10);

        // Sécurités pour éviter les valeurs aberrantes
        if (voltage > 6000)
            voltage = 6000; // Max 60V
        if (temp > 1000)
            temp = 1000; // Max 100°C
    }

    // Format little-endian selon la doc
    canFrame.data[0] = lowByte(voltage);  // 0x68
    canFrame.data[1] = highByte(voltage); // 0x10
    canFrame.data[2] = lowByte(current);  // 0x00
    canFrame.data[3] = highByte(current); // 0x00
    canFrame.data[4] = lowByte(temp);     // 0x0E
    canFrame.data[5] = highByte(temp);    // 0x01
    canFrame.data[6] = 0x00;              // Fixe
    canFrame.data[7] = 0x00;              // Fixe

    ESP32Can.writeFrame(canFrame);
    Serial.printf("CAN 0x356: V=%.2fV, I=%.1fA, T=%.1f°C (données %s)\n",
                  voltage / 100.0, (current - 30000) / 100.0, temp / 10.0,
                  latestMetrics.isDataValid ? "valides" : "défaut");
}

void sendAlarms()
{
    canFrame = {0};
    canFrame.identifier = CAN_ID_ALARMS;
    canFrame.extd = 0;
    canFrame.data_length_code = 8;

    uint8_t protections = calculateSystemProtections();
    uint8_t alarms = calculateSystemAlarms();

    // Nombre de modules réel
    extern int configuredBatteryCount;
    uint8_t moduleCount = (configuredBatteryCount > 0) ? configuredBatteryCount : 1;

    canFrame.data[0] = protections; // Protections calculées
    canFrame.data[1] = 0x00;        // Réservé
    canFrame.data[2] = alarms;      // Alarmes calculées
    canFrame.data[3] = 0x00;        // Réservé
    canFrame.data[4] = moduleCount; // Nombre réel de batteries
    canFrame.data[5] = 0x00;        // Signature
    canFrame.data[6] = 0x00;        // Signature
    canFrame.data[7] = 0x00;        // Réservé

    ESP32Can.writeFrame(canFrame);

    Serial.printf("CAN 0x359: Protections=0x%02X, Alarmes=0x%02X, Modules=%d\n",
                  protections, alarms, moduleCount);
}

void sendRequests()
{
    // Données Brutes : C0 00 00 00 00 00 00 00

    canFrame = {0};
    canFrame.identifier = CAN_ID_REQUESTS;
    canFrame.extd = 0;
    canFrame.data_length_code = 8;

    canFrame.data[0] = 0xC0; // Bits 7+6: Charge+Discharge enable
    canFrame.data[1] = 0x00; // Réservé
    canFrame.data[2] = 0x00; // Réservé
    canFrame.data[3] = 0x00; // Réservé
    canFrame.data[4] = 0x00; // Réservé
    canFrame.data[5] = 0x00; // Réservé
    canFrame.data[6] = 0x00; // Réservé
    canFrame.data[7] = 0x00; // Réservé

    ESP32Can.writeFrame(canFrame);
}

// ——————— AFFICHAGE TEMPS RÉEL DES TRAMES ———————

void updateCanFrameDisplay()
{
    // Mettre à jour les textes des trames avec les valeurs RÉELLES actuelles
    uint16_t ichg = (uint16_t)(currentChargeSetpoint * 10);
    uint16_t idis = (uint16_t)(currentDischargeSetpoint * 10);

    // Trame 0x351 - Limites avec consignes dynamiques
    sprintf(lastCanFrames[0], "351: 04 02 %02X %02X %02X %02X C9 01",
            lowByte(ichg), highByte(ichg), lowByte(idis), highByte(idis));

    // Trame 0x355 - SOC/SOH avec données réelles
    extern AggregateBatteryMetrics latestMetrics;
    uint16_t soc_display = latestMetrics.isDataValid ? (uint16_t)round(latestMetrics.averageSoc) : 0;
    uint16_t soh_display = 100; // Fixe comme convenu
    sprintf(lastCanFrames[1], "355: %02X %02X %02X %02X D0 07 00 00",
            lowByte(soc_display), highByte(soc_display),
            lowByte(soh_display), highByte(soh_display));

    // Trame 0x356 - Tension/Courant/Température avec données réelles
    uint16_t volt_display = latestMetrics.isDataValid ? (uint16_t)(latestMetrics.averageVoltage * 100) : 4200;
    int16_t curr_raw = latestMetrics.isDataValid ? (int16_t)(latestMetrics.totalCurrent * 100) : 0;
    uint16_t curr_display = (uint16_t)(curr_raw + 30000);
    uint16_t temp_display = latestMetrics.isDataValid ? (uint16_t)(latestMetrics.averageTemp * 10) : 270;
    sprintf(lastCanFrames[2], "356: %02X %02X %02X %02X %02X %02X 00 00",
            lowByte(volt_display), highByte(volt_display),
            lowByte(curr_display), highByte(curr_display),
            lowByte(temp_display), highByte(temp_display));

    // Trame 0x359 - Protections/Alarmes avec calcul dynamique
    uint8_t protections = calculateSystemProtections();
    uint8_t alarms = calculateSystemAlarms();
    extern int configuredBatteryCount;
    uint8_t modules = (configuredBatteryCount > 0) ? configuredBatteryCount : 1;
    sprintf(lastCanFrames[3], "359: %02X 00 %02X 00 %02X 00 00 00",
            protections, alarms, modules);

    // Trame 0x35C - Requêtes (toujours fixe)
    strcpy(lastCanFrames[4], "35C: C0 00 00 00 00 00 00 00");
}

void showCanFrames()
{
    extern void clearDisplay();
    extern void showDisplay();
    extern void drawText(int x, int y, const char *text, bool large, bool inverted);
    extern void drawTitle(const char *title);

    clearDisplay();
    drawTitle("TRAMES CAN TEMPS REEL");

    // Afficher les 5 trames sur 5 lignes (ajusté pour tenir)
    for (int i = 0; i < 5; i++)
    {
        drawText(2, 20 + i * 9, lastCanFrames[i], false, false); // Espacement réduit
    }

    // Indicateurs d'état en bas
    char statusLine[30];
    extern bool degradedMode;
    extern int configuredBatteryCount;

    sprintf(statusLine, "%s | %dBatt | %dA/%dA",
            degradedMode ? "DEGRADE" : "NORMAL",
            configuredBatteryCount,
            (int)currentChargeSetpoint,
            (int)currentDischargeSetpoint);

    // Instructions
    drawText(100, 62, "BACK", false, false);

    showDisplay();
}

void setCanDisplayActive(bool active)
{
    canDisplayActive = active;
    if (active)
    {
        updateCanFrameDisplay(); // Initialiser l'affichage
    }
}