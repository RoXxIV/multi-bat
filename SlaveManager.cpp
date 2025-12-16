#include "SlaveManager.h"

// Buffer de réception
static uint8_t slaveRxBuffer[256];
static int slaveRxIndex = 0;
static unsigned long lastByteTime = 0;

// Variables externes nécessaires pour les données
extern AggregateBatteryMetrics latestMetrics;
extern IndividualBatteryData individualBatteryMetrics[MAX_BATTERIES];
extern int configuredBatteryCount;
extern bool globalChargeMosfetOk;
extern bool globalDischargeMosfetOk;

void initSlaveModbus()
{
    // Configuration des pins de contrôle en sortie
    pinMode(SLAVE_DE_PIN, OUTPUT);
    pinMode(SLAVE_RE_PIN, OUTPUT);

    // Mode Réception par défaut (LOW)
    digitalWrite(SLAVE_DE_PIN, LOW);
    digitalWrite(SLAVE_RE_PIN, LOW);

    // Initialisation du Serial standard (UART0) pour le Modbus
    // Note: Cela remplace l'usage du moniteur série pour le debug
    Serial.begin(9600, SERIAL_8N1, SLAVE_RX_PIN, SLAVE_TX_PIN);
}

// Active l'émission (les deux pins HIGH)
void slave_enable_tx()
{
    digitalWrite(SLAVE_DE_PIN, HIGH);
    digitalWrite(SLAVE_RE_PIN, HIGH);
}

// Active la réception (les deux pins LOW)
void slave_enable_rx()
{
    digitalWrite(SLAVE_DE_PIN, LOW);
    digitalWrite(SLAVE_RE_PIN, LOW);
}

// Fonction pour lire un registre spécifique (mapping des adresses)
uint16_t getSlaveRegister(uint16_t addr)
{
    // Calculs temporaires
    float maxCV = 0;
    float minCV = 10.0; // Valeur initiale haute
    float maxCT = -100;
    float minCT = 100;

    // Si on demande des infos sur les cellules, on doit scanner les batteries valides
    if (addr >= 0x3E && addr <= 0x45)
    {
        for (int i = 0; i < configuredBatteryCount; i++)
        {
            IndividualBatteryData *data = &individualBatteryMetrics[i + 1]; // Index + 1
            if (data->isValid)
            {
                if (data->maxCellVoltage > maxCV)
                    maxCV = data->maxCellVoltage;
                if (data->minCellVoltage < minCV)
                    minCV = data->minCellVoltage;
                if (data->maxCellTemp > maxCT)
                    maxCT = data->maxCellTemp;
                if (data->minCellTemp < minCT)
                    minCT = data->minCellTemp;
            }
        }
        // Si aucune batterie valide, remettre des valeurs par défaut
        if (minCV > 5.0)
            minCV = 0;
    }

    switch (addr)
    {
    case 0x38: // Total battery voltage (0.1V)
        return (uint16_t)(latestMetrics.averageVoltage * 10.0f);

    case 0x39: // Current data (Offset 30000, 0.1A)
        return (uint16_t)((latestMetrics.totalCurrent * 10.0f) + 30000);

    case 0x3A: // SOC (0.1%)
        return (uint16_t)(latestMetrics.averageSoc * 10.0f);

    case 0x3B: // LIFE (SOH) - On suppose 100%
        return 100;

    case 0x3C: // Nombre de cellules (ex: 16)
        if (configuredBatteryCount > 0 && individualBatteryMetrics[1].isValid)
        {
            return individualBatteryMetrics[1].cellCount;
        }
        return 16; // Valeur par défaut

    case 0x3D:    // Nbr capteurs temp
        return 4; // Valeur par défaut

    case 0x3E: // Max cell voltage (mV)
        return (uint16_t)(maxCV * 1000.0f);

    case 0x3F:    // Highest cell voltage number
        return 1; // Pas implémenté, retour 1

    case 0x40: // Min cell voltage (mV)
        return (uint16_t)(minCV * 1000.0f);

    case 0x41:    // Min cell voltage number
        return 1; // Pas implémenté

    case 0x42: // Max/Min diff (mV)
        return (uint16_t)((maxCV - minCV) * 1000.0f);

    case 0x43: // Max cell temp (Offset 40 => T = Val - 40, donc Val = T + 40)
        return (uint16_t)(maxCT + 40.0f);

    case 0x44: // Max temp unit number
        return 1;

    case 0x45: // Min cell temp
        return (uint16_t)(minCT + 40.0f);

    case 0x52: // Charging MOS status
        return globalChargeMosfetOk ? 1 : 0;

    case 0x53: // Discharge MOS status
        return globalDischargeMosfetOk ? 1 : 0;

    default:
        return 0;
    }
}

void processSlaveMessage(uint8_t *frame, int len)
{
    // Structure trame requête : [ID] [FUNC] [ADDR_H] [ADDR_L] [QTY_H] [QTY_L] [CRC_L] [CRC_H]
    if (len < 8)
        return;

    uint8_t id = frame[0];
    uint8_t func = frame[1];

    // Vérification ID et Fonction (On gère seulement 0x03 Read Holding)
    if (id != SLAVE_ID || func != 0x03)
        return;

    // Vérification CRC
    uint16_t receivedCrc = (frame[len - 1] << 8) | frame[len - 2];
    uint16_t calculatedCrc = calculateCRC16(frame, len - 2);

    if (receivedCrc != calculatedCrc)
        return;

    // Décodage adresse et quantité
    uint16_t startAddr = (frame[2] << 8) | frame[3];
    uint16_t quantity = (frame[4] << 8) | frame[5];

    // Limite de sécurité sur la quantité
    if (quantity > 32)
        quantity = 32;

    // Préparation réponse
    // Structure réponse : [ID] [FUNC] [BYTES] [DATA...] [CRC_L] [CRC_H]
    uint8_t response[128];
    response[0] = id;
    response[1] = func;
    response[2] = quantity * 2; // Nombre d'octets de données

    int index = 3;
    for (int i = 0; i < quantity; i++)
    {
        uint16_t val = getSlaveRegister(startAddr + i);
        response[index++] = (val >> 8) & 0xFF;
        response[index++] = val & 0xFF;
    }

    // Calcul CRC Réponse
    uint16_t crcRes = calculateCRC16(response, index);
    response[index++] = crcRes & 0xFF;        // CRC Low
    response[index++] = (crcRes >> 8) & 0xFF; // CRC High

    // Envoi
    slave_enable_tx();
    Serial.write(response, index);
    Serial.flush(); // Attendre fin envoi
    // Petite pause pour s'assurer que le dernier bit est parti avant de couper le driver
    // Sur ESP32 flush retourne parfois un peu trop vite pour le RS485
    delayMicroseconds(500);
    slave_enable_rx();
}

void handleSlaveModbus()
{
    // Gestion du timeout inter-trame (3.5 caractères à 9600 bauds ~= 4ms)
    // Si on reçoit des données et qu'il y a une pause, on traite.
    while (Serial.available())
    {
        uint8_t b = Serial.read();

        // Si le buffer est plein ou si trop de temps passé, reset
        if (slaveRxIndex > 0 && (millis() - lastByteTime > 10))
        {
            slaveRxIndex = 0;
        }

        if (slaveRxIndex < sizeof(slaveRxBuffer))
        {
            slaveRxBuffer[slaveRxIndex++] = b;
            lastByteTime = millis();
        }
    }

    // Si on a reçu quelque chose et que le silence est établi (> 5ms)
    if (slaveRxIndex >= 8 && (millis() - lastByteTime > 5))
    {
        processSlaveMessage(slaveRxBuffer, slaveRxIndex);
        slaveRxIndex = 0; // Reset après traitement
    }
}