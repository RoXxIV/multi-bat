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

// ——————— FONCTIONS DE LECTURE ———————

void readAggregateBatteryMetrics(uint8_t configuredBatteryCount, AggregateBatteryMetrics *metrics)
{
    metrics->isDataValid = false;
    if (configuredBatteryCount == 0)
        return;

    Serial.printf("\n--- LECTURE AGRÉGÉE DE %d BATTERIE(S) ---\n", configuredBatteryCount);

    float totalSoc = 0.0f, totalVoltage = 0.0f, totalCurrent = 0.0f, totalTemp = 0.0f;
    int successfulReads = 0;

    for (int i = 0; i < configuredBatteryCount; i++)
    {
        uint8_t currentBatteryId = i + 2;

        const uint16_t startAddr = REG_TOTAL_VOLTAGE; // 0x38
        const uint16_t endAddr = REG_MOS_TEMP;        // 0x5A
        const uint16_t regCount = endAddr - startAddr + 1;
        uint8_t payload[regCount * 2]; // Buffer pour stocker uniquement les données utiles

        // Remplacement de tout le code de communication par un seul appel à la lib
        int bytesRead = modbus_read_registers(currentBatteryId, startAddr, regCount, payload);

        if (bytesRead > 0)
        {
            // La lib a déjà validé le CRC. On peut décoder les données directement.
            // Les offsets sont calculés par rapport au début du bloc lu (0x38).
            totalVoltage += ((payload[0] << 8) | payload[1]) / 10.0f;
            totalCurrent += (((int16_t)((payload[2] << 8) | payload[3])) - 30000) * 0.1f;
            totalSoc += ((payload[4] << 8) | payload[5]) / 10.0f;

            const int tempOffset = (REG_MOS_TEMP - REG_TOTAL_VOLTAGE) * 2;
            totalTemp += (((payload[tempOffset] << 8) | payload[tempOffset + 1])) - 40.0f;

            successfulReads++;
        }
    }

    if (successfulReads > 0)
    {
        metrics->averageSoc = totalSoc / successfulReads;
        metrics->averageVoltage = totalVoltage / successfulReads;
        metrics->totalCurrent = totalCurrent;
        metrics->averageTemp = totalTemp / successfulReads;
        metrics->isDataValid = true;

        Serial.printf("--- RÉSULTAT AGRÉGÉ (%d/%d batteries) ---\n", successfulReads, configuredBatteryCount);
        Serial.printf("  - SOC Moyen: %.1f%%\n", metrics->averageSoc);
        Serial.printf("  - Tension Moyenne: %.1fV\n", metrics->averageVoltage);
        Serial.printf("  - Courant Total: %.1fA\n", metrics->totalCurrent);
        Serial.printf("  - Temp. Moyenne: %.1f°C\n", metrics->averageTemp);
    }
    else
    {
        Serial.println("ERREUR: Aucune batterie n'a répondu correctement à la lecture agrégée.");
    }
}

void updateIndividualBatteryMetrics(uint8_t batteryId)
{
    IndividualBatteryData *batteryData = &individualBatteryMetrics[batteryId - 1];
    batteryData->isValid = false;

    const uint16_t startAddr = 0x00;
    const uint16_t endAddr = 0x60;
    const uint16_t regCount = endAddr - startAddr + 1;
    uint8_t payload[regCount * 2]; // Buffer assez grand pour toutes les données

    // Un seul appel à la librairie pour lire toutes les données
    int bytesRead = modbus_read_registers(batteryId, startAddr, regCount, payload);

    if (bytesRead > 0)
    {
        // La librairie a fait tout le travail de validation. On décode.
        // Les offsets sont calculés par rapport à startAddr (0x00), donc ils fonctionnent directement.

        // Tensions des 15 cellules
        for (int i = 0; i < 15; i++)
        {
            batteryData->cellVoltages[i] = ((payload[i * 2] << 8) | payload[i * 2 + 1]) / 1000.0f;
        }

        // Températures 1 et 2
        int tempOffset = (REG_TEMPERATURES_START - startAddr) * 2;
        batteryData->temp1 = (((payload[tempOffset] << 8) | payload[tempOffset + 1])) - 40.0f;
        batteryData->temp2 = (((payload[tempOffset + 2] << 8) | payload[tempOffset + 3])) - 40.0f;

        // Tension, Courant, SOC
        int mainMetricsOffset = (REG_TOTAL_VOLTAGE - startAddr) * 2;
        batteryData->voltage = ((payload[mainMetricsOffset] << 8) | payload[mainMetricsOffset + 1]) / 10.0f;
        batteryData->current = (((int16_t)((payload[mainMetricsOffset + 2] << 8) | payload[mainMetricsOffset + 3])) - 30000) * 0.1f;
        batteryData->soc = ((payload[mainMetricsOffset + 4] << 8) | payload[mainMetricsOffset + 5]) / 10.0f;

        // Heartbeat
        int heartbeatOffset = (REG_HEARTBEAT - startAddr) * 2;
        batteryData->heartbeat = (payload[heartbeatOffset] << 8) | payload[heartbeatOffset + 1];

        // Différence de tension entre cellules
        const int diffOffset = (0x42 - startAddr) * 2;
        batteryData->cellVoltageDifference = ((payload[diffOffset] << 8) | payload[diffOffset + 1]) / 1000.0f;

        // Statut des MOSFETs
        int mosfetOffset = (REG_CHARGE_MOSFET - startAddr) * 2;
        batteryData->chargeMosfetStatus = (payload[mosfetOffset + 1] & 0x01) != 0;
        batteryData->dischargeMosfetStatus = (payload[mosfetOffset + 3] & 0x01) != 0;

        // Température MOS
        int mosTempOffset = (REG_MOS_TEMP - startAddr) * 2;
        batteryData->mosTemp = (((payload[mosTempOffset] << 8) | payload[mosTempOffset + 1])) - 40.0f;

        // Limite de courant
        const int limitOffset = (0x60 - startAddr) * 2;
        batteryData->currentLimit = (((int16_t)((payload[limitOffset] << 8) | payload[limitOffset + 1])) - 30000) * 0.1f;

        batteryData->isValid = true;
        Serial.printf("INFO: Données détaillées de la batterie ID=%d mises à jour.\n", batteryId);
    }
    else
    {
        Serial.printf("ERREUR: Impossible de lire les données individuelles de la batterie ID=%d\n", batteryId);
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
    // Cible la bonne structure dans le tableau global
    IndividualBatteryData *batteryData = &individualBatteryMetrics[batteryId - 1];

    // Affiche un en-tête pour identifier la batterie
    Serial.printf("\n--- DEBUG: Données Batterie ID=%d ---\n", batteryId);

    // Vérifie si les données sont valides avant de les afficher
    if (!batteryData->isValid)
    {
        Serial.println("  /!\\ Données invalides ou non encore lues.");
        return;
    }

    // Affiche chaque information avec son unité
    Serial.printf("  - Statut           : VALIDE\n");
    Serial.printf("  - Tension          : %.2f V\n", batteryData->voltage);
    Serial.printf("  - Courant          : %.2f A\n", batteryData->current);
    Serial.printf("  - SOC              : %.1f %%\n", batteryData->soc);
    Serial.printf("  - MOSFET Charge    : %s\n", batteryData->chargeMosfetStatus ? "ON" : "OFF");
    Serial.printf("  - MOSFET Décharge  : %s\n", batteryData->dischargeMosfetStatus ? "ON" : "OFF");
    Serial.printf("  - Température 1    : %.1f C\n", batteryData->temp1);
    Serial.printf("  - Température 2    : %.1f C\n", batteryData->temp2);
    Serial.printf("  - Température MOS  : %.1f C\n", batteryData->mosTemp);
    Serial.printf("  - Heartbeat        : %u\n", batteryData->heartbeat);

    // Affiche les tensions des 4 premières cellules pour un aperçu rapide
    Serial.printf("  - Cellules (1-4)   : %.3fV | %.3fV | %.3fV | %.3fV\n",
                  batteryData->cellVoltages[0],
                  batteryData->cellVoltages[1],
                  batteryData->cellVoltages[2],
                  batteryData->cellVoltages[3]);
    Serial.println("------------------------------------");
}