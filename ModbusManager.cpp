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
    // On initialise la structure pointée avec des valeurs par défaut.
    metrics->isDataValid = false;

    if (configuredBatteryCount == 0)
    {
        return; // On sort de la fonction si aucune batterie n'est configurée.
    }

    Serial.printf("\n--- LECTURE AGRÉGÉE DE %d BATTERIE(S) ---\n", configuredBatteryCount);

    // Variables pour accumuler les totaux.
    float totalSoc = 0.0f, totalVoltage = 0.0f, totalCurrent = 0.0f, totalTemp = 0.0f;
    int successfulReads = 0; // Compteur des batteries qui ont répondu correctement.

    // Boucle sur chaque batterie configurée. Les ID commencent à 2.
    for (int i = 0; i < configuredBatteryCount; i++)
    {
        uint8_t currentBatteryId = i + 2; // ID = 2, 3, 4...

        // On lit un grand bloc de registres en une seule fois pour être efficace.
        const uint16_t startAddr = REG_TOTAL_VOLTAGE; // 0x38
        const uint16_t endAddr = REG_MOS_TEMP;        // 0x5A
        const uint16_t regCount = endAddr - startAddr + 1;

        // Étape 1 : Envoi de la commande de lecture
        int frameLength = modbus_build_read_frame(sendBuffer, currentBatteryId, startAddr, regCount);
        if (frameLength <= 0)
            continue;

        while (modbusSerial.available())
            modbusSerial.read();
        modbus_enable_transmit();
        modbusSerial.write(sendBuffer, frameLength);
        modbusSerial.flush();
        // delayMicroseconds(100);
        delay(2);
        modbus_enable_receive();

        // Étape 2 : Attente et réception de la réponse
        unsigned long timeout = millis() + 800;
        int responseLength = 0;
        while (millis() < timeout && responseLength < sizeof(receiveBuffer))
        {
            if (modbusSerial.available())
            {
                receiveBuffer[responseLength++] = modbusSerial.read();
                timeout = millis() + 50;
            }
        }

        // Étape 3 : Analyse de la réponse
        if (responseLength > 0)
        {
            uint8_t expectedAddr = 0x50 + currentBatteryId;
            uint16_t receivedCrc = (receiveBuffer[responseLength - 1] << 8) | receiveBuffer[responseLength - 2];
            uint16_t calculatedCrc = calculateCRC16(receiveBuffer, responseLength - 2);

            if (receiveBuffer[0] == expectedAddr && receivedCrc == calculatedCrc && receiveBuffer[1] == CMD_READ_HOLDING)
            {
                uint8_t *data = &receiveBuffer[3];

                // On décode chaque valeur en utilisant son offset par rapport à l'adresse de début (0x38).
                totalVoltage += ((data[0] << 8) | data[1]) / 10.0f;
                totalCurrent += (((int16_t)((data[2] << 8) | data[3])) - 30000) * 0.1f;
                totalSoc += ((data[4] << 8) | data[5]) / 10.0f;

                const int tempOffset = (REG_MOS_TEMP - REG_TOTAL_VOLTAGE) * 2;
                totalTemp += (((data[tempOffset] << 8) | data[tempOffset + 1])) - 40.0f;

                successfulReads++;
            }
        }
    }

    // --- Étape 4 : Calcul des moyennes et mise à jour de la structure globale ---
    if (successfulReads > 0)
    {
        // On utilise l'opérateur "->" pour modifier directement la structure via le pointeur.
        metrics->averageSoc = totalSoc / successfulReads;
        metrics->averageVoltage = totalVoltage / successfulReads;
        metrics->totalCurrent = totalCurrent; // Le courant s'additionne.
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
    // Cible la bonne structure dans notre tableau global
    IndividualBatteryData *batteryData = &individualBatteryMetrics[batteryId - 1];
    batteryData->isValid = false; // On invalide les données avant la lecture

    // On lit un grand bloc de registres en une seule fois, de 0x00 à 0x5A.
    const uint16_t startAddr = 0x00;       // Début des tensions de cellules
    const uint16_t endAddr = REG_MOS_TEMP; // Fin à la temp. des MOSFETs
    const uint16_t regCount = endAddr - startAddr + 1;

    // Étape 1 : Envoi de la commande de lecture
    int frameLength = modbus_build_read_frame(sendBuffer, batteryId, startAddr, regCount);
    if (frameLength <= 0)
        return;

    while (modbusSerial.available())
        modbusSerial.read();
    modbus_enable_transmit();
    modbusSerial.write(sendBuffer, frameLength);
    modbusSerial.flush();
    // delayMicroseconds(100);
    delay(2);
    modbus_enable_receive();

    // Étape 2 : Réception de la réponse
    unsigned long timeout = millis() + 800;
    int responseLength = 0;
    while (millis() < timeout && responseLength < sizeof(receiveBuffer))
    {
        if (modbusSerial.available())
        {
            receiveBuffer[responseLength++] = modbusSerial.read();
            timeout = millis() + 50;
        }
    }

    // Étape 3 : Analyse et décodage de la réponse
    if (responseLength > 0)
    {
        uint8_t expectedAddr = 0x50 + batteryId;
        uint16_t receivedCrc = (receiveBuffer[responseLength - 1] << 8) | receiveBuffer[responseLength - 2];
        uint16_t calculatedCrc = calculateCRC16(receiveBuffer, responseLength - 2);

        if (receiveBuffer[0] == expectedAddr && receivedCrc == calculatedCrc)
        {
            uint8_t *data = &receiveBuffer[3];

            // Tensions des 15 cellules (Registres 0x00 à 0x0E)
            for (int i = 0; i < 15; i++)
            {
                batteryData->cellVoltages[i] = ((data[i * 2] << 8) | data[i * 2 + 1]) / 1000.0f;
            }

            // Températures 1 et 2 (Registres 0x30 et 0x31)
            int tempOffset = (REG_TEMPERATURES_START - startAddr) * 2;
            batteryData->temp1 = (((data[tempOffset] << 8) | data[tempOffset + 1])) - 40.0f;
            batteryData->temp2 = (((data[tempOffset + 2] << 8) | data[tempOffset + 3])) - 40.0f;

            // Tension, Courant, SOC (Registres 0x38 à 0x3A)
            int mainMetricsOffset = (REG_TOTAL_VOLTAGE - startAddr) * 2;
            batteryData->voltage = ((data[mainMetricsOffset] << 8) | data[mainMetricsOffset + 1]) / 10.0f;
            batteryData->current = (((int16_t)((data[mainMetricsOffset + 2] << 8) | data[mainMetricsOffset + 3])) - 30000) * 0.1f;
            batteryData->soc = ((data[mainMetricsOffset + 4] << 8) | data[mainMetricsOffset + 5]) / 10.0f;

            // Heartbeat (Registre 0x3B)
            int heartbeatOffset = (REG_HEARTBEAT - startAddr) * 2;
            batteryData->heartbeat = (data[heartbeatOffset] << 8) | data[heartbeatOffset + 1];

            // Statut des MOSFETs (Registres 0x52 et 0x53)
            int mosfetOffset = (REG_CHARGE_MOSFET - startAddr) * 2;
            batteryData->chargeMosfetStatus = (data[mosfetOffset + 1] & 0x01) != 0;
            batteryData->dischargeMosfetStatus = (data[mosfetOffset + 3] & 0x01) != 0;

            // Température MOS (Registre 0x5A)
            int mosTempOffset = (REG_MOS_TEMP - startAddr) * 2;
            batteryData->mosTemp = (((data[mosTempOffset] << 8) | data[mosTempOffset + 1])) - 40.0f;

            batteryData->isValid = true;
            Serial.printf("INFO: Données détaillées de la batterie ID=%d mises à jour.\n", batteryId);
        }
    }
}
void updateBatteryStaticInfo(uint8_t batteryId)
{
    // Cible la bonne structure dans notre tableau global
    IndividualBatteryData *batteryData = &individualBatteryMetrics[batteryId - 1];
    // On ne change pas isValid ici, car cette fonction ne fait que compléter les infos.

    // Étape 1 : Envoi de la commande de lecture pour le numéro de série
    int frameLength = modbus_build_read_frame(sendBuffer, batteryId, REG_SERIAL_NUMBER_START, REG_SERIAL_NUMBER_COUNT);
    if (frameLength <= 0)
        return;

    modbus_print_buffer("ENVOI (Info statique)", sendBuffer, frameLength);

    while (modbusSerial.available())
        modbusSerial.read();
    modbus_enable_transmit();
    modbusSerial.write(sendBuffer, frameLength);
    modbusSerial.flush();
    // delayMicroseconds(100);
    delay(2);
    modbus_enable_receive();

    // Étape 2 : Réception de la réponse
    unsigned long timeout = millis() + 800;
    int responseLength = 0;
    while (millis() < timeout && responseLength < sizeof(receiveBuffer))
    {
        if (modbusSerial.available())
        {
            receiveBuffer[responseLength++] = modbusSerial.read();
            timeout = millis() + 50;
        }
    }

    // Étape 3 : Analyse et décodage de la réponse
    if (responseLength > 0)
    {
        modbus_print_buffer("RECU (Info statique)", receiveBuffer, responseLength);
        uint8_t expectedAddr = 0x50 + batteryId;
        uint16_t receivedCrc = (receiveBuffer[responseLength - 1] << 8) | receiveBuffer[responseLength - 2];
        uint16_t calculatedCrc = calculateCRC16(receiveBuffer, responseLength - 2);

        if (receiveBuffer[0] == expectedAddr && receivedCrc == calculatedCrc)
        {
            uint8_t *data = &receiveBuffer[3];

            // Le numéro de série est encodé sur 32 octets (16 registres).
            // On le copie directement dans notre structure.
            memcpy(batteryData->serialNumber, data, 32);
            batteryData->serialNumber[32] = '\0'; // On ajoute le caractère de fin de chaîne.

            Serial.printf("INFO: S/N de la batterie ID=%d lu avec succès: %s\n", batteryId, batteryData->serialNumber);
        }
    }
}

// ——————— FONCTIONS D'ÉCRITURE  ———————
bool sendDisplayIdToBattery(uint8_t batteryId, uint8_t asciiValue)
{
    if (!modbusSerial || batteryId < 1 || batteryId > MAX_BATTERIES)
    {
        Serial.println("ERREUR: Paramètres invalides pour affichage ID");
        return false;
    }
    Serial.printf("Envoi H%c à batterie ID=%d\n", asciiValue, batteryId);

    // Construction de la trame
    sendBuffer[0] = 0x80 + batteryId; // ID de la batterie
    sendBuffer[1] = 0x10;             // Fonction écriture multiple
    sendBuffer[2] = 0x01;             // Adresse registre 0x01F1 (high)
    sendBuffer[3] = 0xF1;             // Adresse registre 0x01F1 (low)
    sendBuffer[4] = 0x00;             // Nombre de registres (high)
    sendBuffer[5] = 0x04;             // Nombre de registres (low)
    sendBuffer[6] = asciiValue;       // Valeur en ASCII
    sendBuffer[7] = 0x00;             // Reste à zéro
    sendBuffer[8] = 0x00;
    sendBuffer[9] = 0x00;
    sendBuffer[10] = 0x00;
    sendBuffer[11] = 0x00;
    sendBuffer[12] = 0x00;
    sendBuffer[13] = 0x00;

    // Calculer le CRC sur les 14 premiers bytes
    uint16_t crc = calculateCRC16(sendBuffer, 14);
    sendBuffer[14] = crc & 0xFF;        // CRC low
    sendBuffer[15] = (crc >> 8) & 0xFF; // CRC high

    char label[40];
    sprintf(label, "DISPLAY_ASCII_%d ID=%d", asciiValue, batteryId);
    modbus_print_buffer(label, sendBuffer, 16);

    // Vider le buffer de réception simple
    while (modbusSerial.available())
    {
        modbusSerial.read();
    }

    // Envoi
    modbus_enable_transmit();
    modbusSerial.write(sendBuffer, 16);
    modbusSerial.flush();
    // delay(10); vvv
    // delayMicroseconds(100);
    delay(2);
    modbus_enable_receive();

    // Attendre l'ACK
    sprintf(label, "DISPLAY_ASCII_%d", asciiValue);
    bool ackReceived = waitForAck(batteryId, label);

    if (ackReceived)
    {
        Serial.printf("✓ ASCII=%d envoyé et confirmé batterie ID=%d\n", asciiValue, batteryId);
        return true;
    }
    else
    {
        Serial.printf("✗ Échec envoi ASCII=%d batterie ID=%d\n", asciiValue, batteryId);
        return false;
    }
}

bool changeBatteryIdTo1(uint8_t batteryId)
{
    if (!modbusSerial || batteryId < 1 || batteryId > MAX_BATTERIES)
    {
        Serial.println("ERREUR: Paramètres invalides pour changement ID");
        return false;
    }

    Serial.printf("Changement ID batterie %d vers ID=1\n", batteryId);

    // Construction de la trame pour changer l'ID vers 1
    // Format similaire à sendDisplayIdToBattery mais pour un registre d'ID
    sendBuffer[0] = 0x80 + batteryId; // Adresse avec correction
    sendBuffer[1] = 0x06;             // Fonction écriture simple (0x06)
    sendBuffer[2] = 0x01;             // Adresse registre ID (high) - À ajuster
    sendBuffer[3] = 0x00;             // Adresse registre ID (low) - À ajuster
    sendBuffer[4] = 0x00;             // Nouvelle valeur ID=1 (high)
    sendBuffer[5] = 0x01;             // Nouvelle valeur ID=1 (low)

    // Calculer le CRC sur les 6 premiers bytes
    uint16_t crc = calculateCRC16(sendBuffer, 6);
    sendBuffer[6] = crc & 0xFF;        // CRC low
    sendBuffer[7] = (crc >> 8) & 0xFF; // CRC high

    char label[40];
    sprintf(label, "CHANGE_ID_%d_TO_1", batteryId);
    modbus_print_buffer(label, sendBuffer, 8);

    // Vider le buffer de réception
    while (modbusSerial.available())
    {
        modbusSerial.read();
    }

    // Envoi
    modbus_enable_transmit();
    modbusSerial.write(sendBuffer, 8);
    modbusSerial.flush();
    // delay(10);vvv
    // delayMicroseconds(100);
    delay(2);
    modbus_enable_receive();

    // Attendre l'ACK
    bool ackReceived = waitForAck(batteryId, label);

    if (ackReceived)
    {
        Serial.printf("✓ ID batterie %d changé vers 1 et confirmé\n", batteryId);
        return true;
    }
    else
    {
        Serial.printf("✗ Échec changement ID batterie %d vers 1\n", batteryId);
        return false;
    }
}

bool changeBatteryIdFrom1To(uint8_t newId)
{
    if (!modbusSerial || newId < 2 || newId > 9)
    {
        Serial.println("ERREUR: Paramètres invalides pour changement ID");
        return false;
    }

    Serial.printf("Changement batterie ID=1 vers ID=%d\n", newId);

    // Construction de la trame (même logique que changeBatteryIdTo1)
    sendBuffer[0] = 0x81;  // Adresse maître
    sendBuffer[1] = 0x06;  // Fonction écriture simple
    sendBuffer[2] = 0x01;  // Adresse registre ID (high) - À ajuster selon votre doc
    sendBuffer[3] = 0x00;  // Adresse registre ID (low) - À ajuster selon votre doc
    sendBuffer[4] = 0x00;  // Nouvelle valeur ID (high)
    sendBuffer[5] = newId; // Nouvelle valeur ID (low)

    uint16_t crc = calculateCRC16(sendBuffer, 6);
    sendBuffer[6] = crc & 0xFF;
    sendBuffer[7] = (crc >> 8) & 0xFF;

    char label[40];
    sprintf(label, "CHANGE_ID_1_TO_%d", newId);
    modbus_print_buffer(label, sendBuffer, 8);

    while (modbusSerial.available())
        modbusSerial.read();

    modbus_enable_transmit();
    modbusSerial.write(sendBuffer, 8);
    modbusSerial.flush();
    // delay(10); vvv
    // delayMicroseconds(100);
    delay(2);
    modbus_enable_receive();

    bool ackReceived = waitForAck(1, label); // On attend l'ACK de l'ancienne adresse (1)

    if (ackReceived)
    {
        Serial.printf("✓ Batterie changée de ID=1 vers ID=%d\n", newId);
        return true;
    }
    else
    {
        Serial.printf("✗ Échec changement ID=1 vers ID=%d\n", newId);
        return false;
    }
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
bool waitForAck(uint8_t batteryId, const char *operation)
{
    const uint8_t ADDR = 0x50 + batteryId;
    const unsigned long T_DEADLINE = millis() + 400;
    const unsigned long T_INTER = 12; // un peu plus large que 5 ms

    static uint8_t buf[128];
    int n = 0;
    unsigned long lastByte = millis();

    while (millis() < T_DEADLINE)
    {
        while (modbusSerial.available())
        {
            uint8_t b = modbusSerial.read();
            lastByte = millis();
            if (n < (int)sizeof(buf))
                buf[n++] = b;

            // --- recherche ACK FC06 ou FC10 (8 octets)
            if (n >= 8)
            {
                for (int i = 0; i <= n - 8; ++i)
                {
                    if (buf[i] == ADDR && (buf[i + 1] == 0x06 || buf[i + 1] == 0x10))
                    {
                        uint16_t crc_calc = calculateCRC16(&buf[i], 6);
                        uint16_t crc_recv = (uint16_t)buf[i + 6] | ((uint16_t)buf[i + 7] << 8);
                        if (crc_calc == crc_recv)
                        {
                            char label[32];
                            sprintf(label, "ACK_VALIDE_FC%02X", buf[i + 1]);
                            modbus_print_buffer(label, &buf[i], 8);
                            return true;
                        }
                    }
                }
            }

            // --- recherche “short-ACK” Daly (6 octets): addr 50 00 01 ??
            if (n >= 6)
            {
                for (int i = 0; i <= n - 6; ++i)
                {
                    if (buf[i] == ADDR && buf[i + 1] == 0x50 && buf[i + 2] == 0x00 && buf[i + 3] == 0x01)
                    {
                        // Pas de vérif CRC : les 2 derniers octets ne sont pas CRC Modbus sur certains firmwares Daly.
                        modbus_print_buffer("ACK_VALIDE_SHORT", &buf[i], 6);
                        return true;
                    }
                }
            }

            // garder la fin si le buffer sature
            if (n > (int)sizeof(buf) - 16)
            {
                memmove(buf, &buf[n / 2], n - n / 2);
                n -= n / 2;
            }
        }

        if ((millis() - lastByte) > T_INTER && n > 0)
        {
            Serial.printf("DEBUG: Buffer brut [%d octets]: ", n);
            for (int i = 0; i < n; i++)
                Serial.printf("%02X ", buf[i]);
            Serial.println();
        }
    }

    Serial.printf("✗ Timeout: Aucune trame ACK valide reçue pour %s\n", operation);
    return false;
}

// DEBUG
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