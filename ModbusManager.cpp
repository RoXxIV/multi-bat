#include "ModbusManager.h"
#include "ModbusLib.h"
#include "DisplayManager.h"
// ——————— VARIABLES GLOBALES ———————
extern HardwareSerial modbusSerial; // pointeur qui contiendra le port serie à utiliser
uint8_t sendBuffer[256];            // Tableau pour construire les commandes Modbus à envoyer
uint8_t receiveBuffer[256];         // Tableau pour stocker les réponses reçues des batteries

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

void updateIndividualBatteryMetrics(uint8_t batteryId)
{
    IndividualBatteryData *batteryData = &individualBatteryMetrics[batteryId - 1];
    batteryData->isValid = false;

    // --- LECTURE BLOC 1 (Données principales : 0x38 -> 0x45) ---
    const uint16_t startAddr1 = REG_TOTAL_VOLTAGE;
    const uint16_t regCount1 = (REG_MIN_CELL_TEMP - REG_TOTAL_VOLTAGE) + 1;
    uint8_t payload1[regCount1 * 2];
    int bytesRead1 = modbus_read_registers(batteryId, startAddr1, regCount1, payload1);

    // --- LECTURE BLOC 2 (MOSFETs : 0x52 -> 0x53) ---
    const uint16_t startAddr2 = REG_CHARGE_MOSFET;
    const uint16_t regCount2 = 2;
    uint8_t payload2[regCount2 * 2];
    int bytesRead2 = modbus_read_registers(batteryId, startAddr2, regCount2, payload2);

    // --- LECTURE BLOC 3 (Codes défaut : 0x6B -> 0x73) ---
    const uint16_t startAddr3 = REG_WAKE_UP_SOURCE;
    const uint16_t regCount3 = (REG_FAULT_CODE_12_13 - REG_WAKE_UP_SOURCE) + 1;
    uint8_t payload3[regCount3 * 2];
    int bytesRead3 = modbus_read_registers(batteryId, startAddr3, regCount3, payload3);

    // --- DÉCODAGE ET VALIDATION ---
    // Si une batterie repond correctement aux trois blocs, elle renvoie forcement le double de bits
    if (bytesRead1 == (regCount1 * 2) && bytesRead2 == (regCount2 * 2) && bytesRead3 == (regCount3 * 2))
    {
        int offset;

        // --- Décodage du BLOC 1 (Données principales) ---
        offset = (REG_TOTAL_VOLTAGE - startAddr1) * 2;
        batteryData->voltage = ((payload1[offset] << 8) | payload1[offset + 1]) / 100.0f;
        offset = (REG_CURRENT - startAddr1) * 2;
        batteryData->current = (((int16_t)((payload1[offset] << 8) | payload1[offset + 1])) - 30000) * 0.1f;
        offset = (REG_SOC - startAddr1) * 2;
        batteryData->soc = ((payload1[offset] << 8) | payload1[offset + 1]) / 10.0f;
        offset = (REG_CELL_COUNT - startAddr1) * 2;
        batteryData->cellCount = (payload1[offset] << 8) | payload1[offset + 1];
        offset = (REG_MAX_CELL_VOLTAGE - startAddr1) * 2;
        batteryData->maxCellVoltage = ((payload1[offset] << 8) | payload1[offset + 1]) / 1000.0f;
        offset = (REG_MIN_CELL_VOLTAGE - startAddr1) * 2;
        batteryData->minCellVoltage = ((payload1[offset] << 8) | payload1[offset + 1]) / 1000.0f;
        offset = (REG_CELL_V_DIFF - startAddr1) * 2;
        batteryData->cellVoltageDifference = ((payload1[offset] << 8) | payload1[offset + 1]) / 1000.0f;
        offset = (REG_MAX_CELL_TEMP - startAddr1) * 2;
        batteryData->maxCellTemp = (((payload1[offset] << 8) | payload1[offset + 1])) - 40.0f;
        offset = (REG_MIN_CELL_TEMP - startAddr1) * 2;
        batteryData->minCellTemp = (((payload1[offset] << 8) | payload1[offset + 1])) - 40.0f;

        // --- Décodage du BLOC 2 (MOSFETs) ---
        batteryData->chargeMosfetStatus = (payload2[1] & 0x01) != 0;
        batteryData->dischargeMosfetStatus = (payload2[3] & 0x01) != 0;

        // --- Décodage du BLOC 3 (Codes défaut) ---
        offset = (REG_WAKE_UP_SOURCE - startAddr3) * 2;
        batteryData->wakeUpSource = (payload3[offset] << 8) | payload3[offset + 1];
        offset = (REG_FAULT_CODE_0_1 - startAddr3) * 2;
        batteryData->faultCode0_1 = (payload3[offset] << 8) | payload3[offset + 1];
        offset = (REG_FAULT_CODE_2_3 - startAddr3) * 2;
        batteryData->faultCode2_3 = (payload3[offset] << 8) | payload3[offset + 1];
        offset = (REG_FAULT_CODE_4_5 - startAddr3) * 2;
        batteryData->faultCode4_5 = (payload3[offset] << 8) | payload3[offset + 1];
        offset = (REG_FAULT_CODE_6_7 - startAddr3) * 2;
        batteryData->faultCode6_7 = (payload3[offset] << 8) | payload3[offset + 1];
        offset = (REG_FAULT_CODE_8_9 - startAddr3) * 2;
        batteryData->faultCode8_9 = (payload3[offset] << 8) | payload3[offset + 1];
        offset = (REG_FAULT_CODE_10_11 - startAddr3) * 2;
        batteryData->faultCode10_11 = (payload3[offset] << 8) | payload3[offset + 1];
        offset = (REG_FAULT_CODE_12_13 - startAddr3) * 2;
        batteryData->faultCode12_13 = (payload3[offset] << 8) | payload3[offset + 1];

        batteryData->isValid = true;
    }
    else
    {
        Serial.printf("ERREUR: Échec lecture des blocs de données pour ID=%d (B1:%d, B2:%d, B3:%d)\n", batteryId, bytesRead1, bytesRead2, bytesRead3);
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