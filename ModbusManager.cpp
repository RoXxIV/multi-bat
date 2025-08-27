#include "ModbusManager.h"
#include "DisplayManager.h"
// ——————— VARIABLES GLOBALES ———————
HardwareSerial *modbusSerial = nullptr; // pointeur qui contiendra le port serie à utiliser
uint8_t sendBuffer[256];                // Tableau pour construire les commandes Modbus à envoyer
uint8_t receiveBuffer[256];             // Tableau pour stocker les réponses reçues des batteries

// ——————— FONCTIONS D'INITIALISATION ———————

void initModbus(HardwareSerial *serial)
{
    // Configuration du pin DE/RE pour RS485
    pinMode(MODBUS_DE_RE_PIN, OUTPUT);
    enableRS485Receive(); // Mode réception par défaut

    modbusSerial = serial;
    // modbusSerial->begin(BAUD_RATE, SERIAL_8E1, MODBUS_RX_PIN, MODBUS_TX_PIN); //vvv
    modbusSerial->begin(BAUD_RATE, SERIAL_8N1, MODBUS_RX_PIN, MODBUS_TX_PIN);
    // Initialiser les buffers
    memset(sendBuffer, 0, sizeof(sendBuffer));
    memset(receiveBuffer, 0, sizeof(receiveBuffer));

    Serial.println("Modbus initialisé - Baud: 9600 8E1");
    Serial.printf("Pins: RX=%d, TX=%d, DE/RE=%d\n", MODBUS_RX_PIN, MODBUS_TX_PIN, MODBUS_DE_RE_PIN);
    Serial.printf("Adresse maître: 0x%02X\n", MASTER_ADDR);
}

void enableRS485Transmit()
{
    digitalWrite(MODBUS_DE_RE_PIN, HIGH); // Mode émission
}

void enableRS485Receive()
{
    digitalWrite(MODBUS_DE_RE_PIN, LOW); // Mode réception
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
        int frameLength = buildReadCommand(currentBatteryId, startAddr, regCount);
        if (frameLength <= 0)
            continue;

        while (modbusSerial->available())
            modbusSerial->read();
        enableRS485Transmit();
        modbusSerial->write(sendBuffer, frameLength);
        modbusSerial->flush();
        delayMicroseconds(100);
        enableRS485Receive();

        // Étape 2 : Attente et réception de la réponse
        unsigned long timeout = millis() + 800;
        int responseLength = 0;
        while (millis() < timeout && responseLength < sizeof(receiveBuffer))
        {
            if (modbusSerial->available())
            {
                receiveBuffer[responseLength++] = modbusSerial->read();
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
                totalSoc += ((data[4] << 8) | data[5]) * 0.001f;

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
    printModbusBuffer(label, sendBuffer, 16);

    // Vider le buffer de réception simple
    while (modbusSerial->available())
    {
        modbusSerial->read();
    }

    // Envoi
    enableRS485Transmit();
    modbusSerial->write(sendBuffer, 16);
    modbusSerial->flush();
    // delay(10); vvv
    delayMicroseconds(100);
    enableRS485Receive();

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
    printModbusBuffer(label, sendBuffer, 8);

    // Vider le buffer de réception
    while (modbusSerial->available())
    {
        modbusSerial->read();
    }

    // Envoi
    enableRS485Transmit();
    modbusSerial->write(sendBuffer, 8);
    modbusSerial->flush();
    // delay(10);vvv
    delayMicroseconds(100);
    enableRS485Receive();

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
    printModbusBuffer(label, sendBuffer, 8);

    while (modbusSerial->available())
        modbusSerial->read();

    enableRS485Transmit();
    modbusSerial->write(sendBuffer, 8);
    modbusSerial->flush();
    // delay(10); vvv
    delayMicroseconds(100);
    enableRS485Receive();

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

int buildReadCommand(uint8_t batteryId, uint16_t startAddr, uint16_t regCount)
{
    // --- CORRECTION FINALE ---
    // On revient à un format Modbus standard où la trame est envoyée
    // directement à l'adresse de la batterie (ex: 0x82 pour ID 2).
    // C'est la même logique que pour les commandes d'écriture qui, elles, fonctionnent.

    sendBuffer[0] = 0x80 + batteryId;        // Adresse cible de la batterie
    sendBuffer[1] = CMD_READ_HOLDING;        // Fonction lecture (0x03)
    sendBuffer[2] = (startAddr >> 8) & 0xFF; // Adresse du premier registre (poids fort)
    sendBuffer[3] = startAddr & 0xFF;        // (poids faible)
    sendBuffer[4] = (regCount >> 8) & 0xFF;  // Nombre de registres à lire (poids fort)
    sendBuffer[5] = regCount & 0xFF;         // (poids faible)

    // Le calcul du CRC reste identique.
    uint16_t crc = calculateCRC16(sendBuffer, 6);
    sendBuffer[6] = crc & 0xFF;
    sendBuffer[7] = (crc >> 8) & 0xFF;

    return 8;
}

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
        while (modbusSerial->available())
        {
            uint8_t b = modbusSerial->read();
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
                            printModbusBuffer(label, &buf[i], 8);
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
                        printModbusBuffer("ACK_VALIDE_SHORT", &buf[i], 6);
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

uint16_t calculateCRC16(uint8_t *data, uint8_t length)
{
    uint16_t crc = 0xFFFF;

    for (int i = 0; i < length; i++)
    {
        crc ^= data[i];
        for (int j = 0; j < 8; j++)
        {
            if (crc & 0x0001)
            {
                crc >>= 1;
                crc ^= 0xA001;
            }
            else
            {
                crc >>= 1;
            }
        }
    }
    return crc;
}

void printModbusBuffer(const char *label, uint8_t *buffer, int length)
{
    Serial.printf("%s [%d bytes]: ", label, length);
    for (int i = 0; i < length; i++)
    {
        Serial.printf("%02X ", buffer[i]);
        if ((i + 1) % 8 == 0)
            Serial.print(" ");
    }
    Serial.println();
}
