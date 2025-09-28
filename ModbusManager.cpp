#include "ModbusManager.h"
#include "ModbusLib.h"
#include "DisplayManager.h"
// ——————— VARIABLES GLOBALES ———————
extern HardwareSerial modbusSerial; // pointeur qui contiendra le port serie à utiliser
uint8_t sendBuffer[256];            // Tableau pour construire les commandes Modbus à envoyer
uint8_t receiveBuffer[256];         // Tableau pour stocker les réponses reçues des batteries

// --- Nouveaux buffers simplifiés pour 2 lectures ---
static uint8_t payload_realtime[((REG_FAULT_CODE_12_13 - REG_TOTAL_VOLTAGE) + 1) * 2];
static uint8_t payload_config[4 * 2]; // Pour les 4 registres de capacité

static int bytesRead_realtime;
static int bytesRead_config;

// ——————— FONCTIONS D'INITIALISATION ———————

void initModbus()
{
    // On passe l'adresse de l'objet global à notre librairie
    modbus_init(&modbusSerial, MODBUS_DE_RE_PIN);

    // On initialise directement l'objet global
    modbusSerial.begin(BAUD_RATE, SERIAL_8N1, MODBUS_RX_PIN, MODBUS_TX_PIN);

    memset(sendBuffer, 0, sizeof(sendBuffer));
    memset(receiveBuffer, 0, sizeof(receiveBuffer));

    Serial.println("Modbus initialisé via ModbusLib - Baud: 9600 8N1");
}

void startModbus()
{
    if (!modbusSerial)
    {
        modbusSerial.begin(BAUD_RATE, SERIAL_8N1, MODBUS_RX_PIN, MODBUS_TX_PIN);
        Serial.println("Modbus redémarré");
    }
}

void stopModbus()
{
    if (modbusSerial)
    {
        modbusSerial.end();
        Serial.println("Modbus arrêté");
    }
}
// ——————— FONCTIONS DE LECTURE ———————
// ——————— NOUVELLES FONCTIONS DE LECTURE ———————

// Lit un seul gros bloc pour toutes les données temps réel (regroupe vos anciens blocs 1, 2, 3)
void updateBatteryMetrics_RealtimeData(uint8_t batteryId)
{
    const uint16_t startAddr = REG_TOTAL_VOLTAGE;
    // On lit tout d'un coup de 0x38 (Total Voltage) à 0x73 (Fault Code 12-13)
    const uint16_t regCount = (REG_FAULT_CODE_12_13 - REG_TOTAL_VOLTAGE) + 1;
    bytesRead_realtime = modbus_read_registers(batteryId, startAddr, regCount, payload_realtime);
}

// Lit le bloc de configuration, valide les deux lectures et décode tout
bool updateAndValidate_ConfigData(uint8_t batteryId)
{
    IndividualBatteryData *batteryData = &individualBatteryMetrics[batteryId - 1];
    batteryData->isValid = false;

    // Lit le bloc 2 (configuration pour le SOH)
    const uint16_t startAddr_config = REG_RATED_CAPACITY_START;
    const uint16_t regCount_config = 4;
    bytesRead_config = modbus_read_registers(batteryId, startAddr_config, regCount_config, payload_config);

    // Valide les deux lectures
    const uint16_t expected_realtime_count = (REG_FAULT_CODE_12_13 - REG_TOTAL_VOLTAGE) + 1;
    if (bytesRead_realtime == (expected_realtime_count * 2) && bytesRead_config == (regCount_config * 2))
    {
        int offset;
        const uint16_t startAddr_realtime = REG_TOTAL_VOLTAGE;

        // --- Décodage des données temps réel ---
        offset = (REG_TOTAL_VOLTAGE - startAddr_realtime) * 2;
        batteryData->voltage = ((payload_realtime[offset] << 8) | payload_realtime[offset + 1]) / 10.0f;
        offset = (REG_CURRENT - startAddr_realtime) * 2;
        batteryData->current = (((int16_t)((payload_realtime[offset] << 8) | payload_realtime[offset + 1])) - 30000) * 0.1f;
        offset = (REG_SOC - startAddr_realtime) * 2;
        batteryData->soc = ((payload_realtime[offset] << 8) | payload_realtime[offset + 1]) / 10.0f;
        offset = (REG_CELL_COUNT - startAddr_realtime) * 2;
        batteryData->cellCount = (payload_realtime[offset] << 8) | payload_realtime[offset + 1];
        offset = (REG_MAX_CELL_VOLTAGE - startAddr_realtime) * 2;
        batteryData->maxCellVoltage = ((payload_realtime[offset] << 8) | payload_realtime[offset + 1]) / 1000.0f;
        offset = (REG_MIN_CELL_VOLTAGE - startAddr_realtime) * 2;
        batteryData->minCellVoltage = ((payload_realtime[offset] << 8) | payload_realtime[offset + 1]) / 1000.0f;
        offset = (REG_CELL_V_DIFF - startAddr_realtime) * 2;
        batteryData->cellVoltageDifference = ((payload_realtime[offset] << 8) | payload_realtime[offset + 1]) / 1000.0f;
        offset = (REG_MAX_CELL_TEMP - startAddr_realtime) * 2;
        batteryData->maxCellTemp = (((payload_realtime[offset] << 8) | payload_realtime[offset + 1])) - 40.0f;
        offset = (REG_MIN_CELL_TEMP - startAddr_realtime) * 2;
        batteryData->minCellTemp = (((payload_realtime[offset] << 8) | payload_realtime[offset + 1])) - 40.0f;
        offset = (REG_CHARGE_MOSFET - startAddr_realtime) * 2;
        batteryData->chargeMosfetStatus = (payload_realtime[offset + 1] & 0x01) != 0;
        offset = (REG_DISCHARGE_MOSFET - startAddr_realtime) * 2;
        batteryData->dischargeMosfetStatus = (payload_realtime[offset + 1] & 0x01) != 0;
        offset = (REG_WAKE_UP_SOURCE - startAddr_realtime) * 2;
        batteryData->wakeUpSource = (payload_realtime[offset] << 8) | payload_realtime[offset + 1];
        offset = (REG_FAULT_CODE_0_1 - startAddr_realtime) * 2;
        batteryData->faultCode0_1 = (payload_realtime[offset] << 8) | payload_realtime[offset + 1];
        offset = (REG_FAULT_CODE_2_3 - startAddr_realtime) * 2;
        batteryData->faultCode2_3 = (payload_realtime[offset] << 8) | payload_realtime[offset + 1];
        offset = (REG_FAULT_CODE_4_5 - startAddr_realtime) * 2;
        batteryData->faultCode4_5 = (payload_realtime[offset] << 8) | payload_realtime[offset + 1];
        offset = (REG_FAULT_CODE_6_7 - startAddr_realtime) * 2;
        batteryData->faultCode6_7 = (payload_realtime[offset] << 8) | payload_realtime[offset + 1];
        offset = (REG_FAULT_CODE_8_9 - startAddr_realtime) * 2;
        batteryData->faultCode8_9 = (payload_realtime[offset] << 8) | payload_realtime[offset + 1];
        offset = (REG_FAULT_CODE_10_11 - startAddr_realtime) * 2;
        batteryData->faultCode10_11 = (payload_realtime[offset] << 8) | payload_realtime[offset + 1];
        offset = (REG_FAULT_CODE_12_13 - startAddr_realtime) * 2;
        batteryData->faultCode12_13 = (payload_realtime[offset] << 8) | payload_realtime[offset + 1];

        // --- Décodage des capacités et CALCUL DU SOH ---
        uint16_t rated_high = (payload_config[0] << 8) | payload_config[1];
        uint16_t rated_low = (payload_config[2] << 8) | payload_config[3];
        uint32_t rated_capacity_raw = ((uint32_t)rated_high << 16) | rated_low;

        uint16_t actual_high = (payload_config[4] << 8) | payload_config[5];
        uint16_t actual_low = (payload_config[6] << 8) | payload_config[7];
        uint32_t actual_capacity_raw = ((uint32_t)actual_high << 16) | actual_low;

        float rated_capacity = rated_capacity_raw / 1000.0f;
        float actual_capacity = actual_capacity_raw / 1000.0f;

        if (rated_capacity > 0)
        {
            batteryData->soh = (actual_capacity / rated_capacity) * 100.0f;
        }
        else
        {
            batteryData->soh = 0;
        }

        batteryData->isValid = true;
        return true;
    }
    else
    {
        Serial.printf("ERREUR: Échec lecture des blocs pour ID=%d (Temps Réel:%d, Config:%d)\n", batteryId, bytesRead_realtime, bytesRead_config);
        return false;
    }
}

void updateBatteryStaticInfo(uint8_t batteryId)
{
    IndividualBatteryData *batteryData = &individualBatteryMetrics[batteryId - 1];

    // On utilise un buffer local pour la réponse
    uint8_t data_payload[REG_SERIAL_NUMBER_COUNT * 2];

    int bytesRead = modbus_read_registers(
        batteryId,
        REG_SERIAL_NUMBER_START,
        REG_SERIAL_NUMBER_COUNT,
        data_payload);

    if (bytesRead > 0)
    {
        // La librairie a déjà tout validé, on peut utiliser les données directement !
        memcpy(batteryData->serialNumber, data_payload, bytesRead);
        batteryData->serialNumber[bytesRead] = '\0';

        Serial.printf("INFO: S/N de la batterie ID=%d lu avec succès: %s\n", batteryId, batteryData->serialNumber);
    }
    else
    {
        Serial.printf("ERREUR: Impossible de lire le S/N de la batterie ID=%d\n", batteryId);
    }
}

// ——————— FONCTIONS D'ÉCRITURE  ———————
bool sendDisplayIdToBattery(uint8_t batteryId, uint8_t asciiValue)
{
    Serial.printf("Envoi H%c à batterie ID=%d\n", asciiValue, batteryId);

    // Préparation des données (4 registres = 8 octets)
    uint8_t payload[8] = {0};
    payload[0] = asciiValue; // On met la valeur ASCII dans le premier octet

    // Appel à la librairie pour l'écriture de 4 registres à l'adresse 0x01F1
    bool success = modbus_write_multiple_registers(batteryId, 0x01F1, 4, payload);

    if (success)
    {
        Serial.printf("✓ ASCII=%d envoyé et confirmé batterie ID=%d\n", asciiValue, batteryId);
    }
    else
    {
        Serial.printf("✗ Échec envoi ASCII=%d batterie ID=%d\n", asciiValue, batteryId);
    }
    return success;
}

bool changeBatteryIdTo1(uint8_t batteryId)
{
    Serial.printf("Changement ID batterie %d vers ID=1\n", batteryId);

    // On appelle la librairie pour écrire la valeur 1 dans le registre 0x0100
    bool success = modbus_write_register(batteryId, 0x0100, 1);

    if (success)
    {
        Serial.printf("✓ ID batterie %d changé vers 1 et confirmé\n", batteryId);
    }
    else
    {
        Serial.printf("✗ Échec changement ID batterie %d vers 1\n", batteryId);
    }
    return success;
}

bool changeBatteryIdFrom1To(uint8_t newId)
{
    Serial.printf("Changement batterie ID=1 vers ID=%d\n", newId);
    // L'adresse du registre de l'ID est 0x0100
    bool success = modbus_write_register(1, 0x0100, newId);

    if (success)
    {
        Serial.printf("✓ Batterie changée de ID=1 vers ID=%d\n", newId);
    }
    else
    {
        Serial.printf("✗ Échec changement ID=1 vers ID=%d\n", newId);
    }
    return success;
}

bool changeAllBatteriesToId1()
{
    Serial.println("=== CHANGEMENT TOUS IDs VERS 1 ===");

    bool globalSuccess = true;

    for (uint8_t batteryId = 1; batteryId <= MAX_BATTERIES; batteryId++)
    {
        Serial.printf("Tentative changement batterie ID=%d vers 1...\n", batteryId);

        bool result = changeBatteryIdTo1(batteryId);
        if (!result)
        {
            Serial.printf("Échec pour batterie ID=%d\n", batteryId);
            globalSuccess = false;
        }

        delay(2000); // Délai entre chaque batterie
    }

    if (globalSuccess)
    {
        Serial.println("✓ Tous les changements d'ID terminés avec succès");
    }
    else
    {
        Serial.println("✗ Certains changements d'ID ont échoué");
    }

    return globalSuccess;
}

// ——————— FONCTIONS UTILITAIRES ———————
void printIndividualBatteryData(uint8_t batteryId)
{
    IndividualBatteryData *batteryData = &individualBatteryMetrics[batteryId - 1];
    Serial.printf("\n--- DEBUG: Données Batterie ID=%d ---\n", batteryId);

    if (!batteryData->isValid)
    {
        Serial.println("  /!\\ Données invalides ou non encore lues.");
        return;
    }

    Serial.printf("  - Statut           : VALIDE\n");
    Serial.printf("  - Tension          : %.2f V\n", batteryData->voltage);
    Serial.printf("  - Courant          : %.2f A\n", batteryData->current);
    Serial.printf("  - SOC              : %.1f %%\n", batteryData->soc);
    Serial.printf("  - MOSFET Charge    : %s\n", batteryData->chargeMosfetStatus ? "ON" : "OFF");
    Serial.printf("  - MOSFET Décharge  : %s\n", batteryData->dischargeMosfetStatus ? "ON" : "OFF");
    Serial.printf("  - Temp Max/Min     : %.1f C / %.1f C\n", batteryData->maxCellTemp, batteryData->minCellTemp);
    Serial.printf("  - Cell V Max/Min   : %.3f V / %.3f V\n", batteryData->maxCellVoltage, batteryData->minCellVoltage);
    Serial.printf("  - Cell V Diff      : %.3f V\n", batteryData->cellVoltageDifference);
    Serial.println("------------------------------------");
}