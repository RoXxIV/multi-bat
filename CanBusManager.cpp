#include "CanBusManager.h"

// ——————— VARIABLES GLOBALES ———————
CanFrame canFrame;
unsigned long lastCanSend = 0;

// Variables de consignes (mode dégradé)
float chargeCurrentSetpoint = 10.0;    // 10A
float dischargeCurrentSetpoint = 10.0; // 10A

// Variables pour affichage des trames
char lastCanFrames[5][50];
bool canDisplayActive = false;

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
    Serial.printf("Consignes initiales: Charge=%.1fA, Décharge=%.1fA\n",
                  chargeCurrentSetpoint, dischargeCurrentSetpoint);
    return true;
}

// ——————— CONTRÔLE DES CONSIGNES ———————

void setChargeCurrentSetpoint(float currentA)
{
    if (currentA < 0)
        currentA = 0;
    if (currentA > MAX_CHARGE_CURRENT_A)
        currentA = MAX_CHARGE_CURRENT_A;

    chargeCurrentSetpoint = currentA;
    Serial.printf("Consigne charge mise à jour: %.1fA\n", currentA);
}

void setDischargeCurrentSetpoint(float currentA)
{
    if (currentA < 0)
        currentA = 0;
    if (currentA > MAX_DISCHARGE_CURRENT_A)
        currentA = MAX_DISCHARGE_CURRENT_A;

    dischargeCurrentSetpoint = currentA;
    Serial.printf("Consigne décharge mise à jour: %.1fA\n", currentA);
}

float getChargeCurrentSetpoint()
{
    return chargeCurrentSetpoint;
}

float getDischargeCurrentSetpoint()
{
    return dischargeCurrentSetpoint;
}

// ——————— FONCTIONS PRINCIPALES ———————

bool shouldSendCan()
{
    return (millis() - lastCanSend >= CAN_SEND_INTERVAL_MS);
}

void sendCanData()
{
    if (!shouldSendCan())
    {
        return;
    }

    lastCanSend = millis();

    // Envoyer toutes les trames
    sendChargeLimits();       // 0x351
    sendSocSoh();             // 0x355
    sendVoltageCurrentTemp(); // 0x356
    sendAlarms();             // 0x359
    sendRequests();           // 0x35C

    // ⭐ Mettre à jour l'affichage des trames si actif
    if (canDisplayActive)
    {
        updateCanFrameDisplay();
    }
    Serial.println("Trames CAN envoyées");
}

// ——————— ENVOI DE TRAMES SPÉCIFIQUES ———————

void sendChargeLimits()
{
    // Données Brutes : 04 02 64 00 64 00 C9 01

    canFrame = {0};
    canFrame.identifier = CAN_ID_LIMITS;
    canFrame.extd = 0;
    canFrame.data_length_code = 8;

    // Tension de charge max: 51.6V = 516 = 0x0204
    uint16_t vchg = 516;

    // Courants en 0.1A (consignes variables 0-600A)
    uint16_t ichg = (uint16_t)(chargeCurrentSetpoint * 10);    // x10 pour 0.1A
    uint16_t idis = (uint16_t)(dischargeCurrentSetpoint * 10); // x10 pour 0.1A

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
                  chargeCurrentSetpoint, dischargeCurrentSetpoint);
}

void sendSocSoh()
{
    // Données Brutes : 14 00 64 00 D0 07 00 00

    canFrame = {0};
    canFrame.identifier = CAN_ID_SOC_SOH;
    canFrame.extd = 0;
    canFrame.data_length_code = 8;

    // SOC: 20% = 20 = 0x0014, SOH: 100% = 100 = 0x0064
    uint16_t soc = 20;  // 20%
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
    // Données Brutes : 68 10 00 00 0E 01 00 00

    canFrame = {0};
    canFrame.identifier = CAN_ID_VOLTAGE_CURRENT;
    canFrame.extd = 0;
    canFrame.data_length_code = 8;

    // Tension: 42.00V = 4200 = 0x1068
    // Courant: 0.0A = 0 = 0x0000
    // Température: 27.0°C = 270 = 0x010E
    uint16_t voltage = 4200; // 42.00V en 0.01V
    uint16_t current = 0;    // 0.0A
    uint16_t temp = 270;     // 27.0°C en 0.1°C

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
    Serial.printf("CAN 0x356: V=%.2fV, I=%.1fA, T=%.1f°C\n",
                  voltage / 100.0, current / 100.0, temp / 10.0);
}

void sendAlarms()
{
    // Données Brutes : 00 00 04 00 01 00 00 00

    canFrame = {0};
    canFrame.identifier = CAN_ID_ALARMS;
    canFrame.extd = 0;
    canFrame.data_length_code = 8;

    canFrame.data[0] = 0x00; // Protections: aucune active
    canFrame.data[1] = 0x00; // Réservé
    canFrame.data[2] = 0x04; // Alarmes: bit 2 actif
    canFrame.data[3] = 0x00; // Réservé
    canFrame.data[4] = 0x01; // Nombre de modules: 1
    canFrame.data[5] = 0x00; // Signature
    canFrame.data[6] = 0x00; // Signature
    canFrame.data[7] = 0x00; // Réservé

    ESP32Can.writeFrame(canFrame);
    Serial.println("CAN 0x359: Alarmes envoyées");
}

void sendRequests()
{
    // FORMAT EXACT SELON CONSIGNE
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
    Serial.println("CAN 0x35C: Requêtes envoyées");
}

// ——————— FONCTIONS D'AFFICHAGE DES TRAMES ———————

void updateCanFrameDisplay()
{
    // Mettre à jour les textes des trames avec les valeurs actuelles
    uint16_t ichg = (uint16_t)(chargeCurrentSetpoint * 10);
    uint16_t idis = (uint16_t)(dischargeCurrentSetpoint * 10);

    sprintf(lastCanFrames[0], "351: 04 02 %02X %02X %02X %02X C9 01",
            lowByte(ichg), highByte(ichg), lowByte(idis), highByte(idis));

    strcpy(lastCanFrames[1], "355: 14 00 64 00 D0 07 00 00");
    strcpy(lastCanFrames[2], "356: 68 10 00 00 0E 01 00 00");
    strcpy(lastCanFrames[3], "359: 00 00 04 00 01 00 00 00");
    strcpy(lastCanFrames[4], "35C: C0 00 00 00 00 00 00 00");
}

void showCanFrames()
{
    extern void clearDisplay();
    extern void showDisplay();
    extern void drawText(int x, int y, const char *text, bool large, bool inverted);
    extern void drawTitle(const char *title);

    clearDisplay();
    drawTitle("TRAMES CAN");

    // Afficher les 4 trames sur 4 lignes
    for (int i = 0; i < 4; i++)
    {
        drawText(2, 25 + i * 10, lastCanFrames[i], false, false);
    }

    // Instructions
    drawText(2, 64, "BACK: retour", false, false);

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