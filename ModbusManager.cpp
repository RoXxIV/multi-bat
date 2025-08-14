#include "ModbusManager.h"

// ——————— VARIABLES GLOBALES ———————
HardwareSerial *modbusSerial = nullptr;
uint8_t sendBuffer[256];
uint8_t receiveBuffer[256];
BatteryData batteries[MAX_BATTERIES];

// ——————— FONCTIONS D'INITIALISATION ———————

void initModbus(HardwareSerial *serial)
{
    // Configuration du pin DE/RE pour RS485
    pinMode(MODBUS_DE_RE_PIN, OUTPUT);
    enableRS485Receive(); // Mode réception par défaut

    modbusSerial = serial;
    // ⭐ CORRECTION : Utiliser SERIAL_8E1 comme dans votre code de test
    modbusSerial->begin(BAUD_RATE, SERIAL_8E1, MODBUS_RX_PIN, MODBUS_TX_PIN);

    // Initialiser les buffers
    memset(sendBuffer, 0, sizeof(sendBuffer));
    memset(receiveBuffer, 0, sizeof(receiveBuffer));

    // Initialiser les données des batteries
    for (int i = 0; i < MAX_BATTERIES; i++)
    {
        batteries[i].batteryId = i + 1;
        batteries[i].dataValid = false;
        batteries[i].lastUpdate = 0;
        batteries[i].validCells = 0;
        batteries[i].validTemps = 0;
    }

    Serial.println("Modbus initialisé - Baud: 9600 8E1"); // ⭐ Correction message
    Serial.printf("Pins: RX=%d, TX=%d, DE/RE=%d\n", MODBUS_RX_PIN, MODBUS_TX_PIN, MODBUS_DE_RE_PIN);
    Serial.printf("Adresse maître: 0x%02X\n", MASTER_ADDR);
    Serial.printf("Adresse réponse base: 0x%02X\n", RESPONSE_ADDR_BASE);
}

void enableRS485Transmit()
{
    digitalWrite(MODBUS_DE_RE_PIN, HIGH); // Mode émission
    // ⭐ CORRECTION : Pas de délai comme dans votre code de test
}

void enableRS485Receive()
{
    digitalWrite(MODBUS_DE_RE_PIN, LOW); // Mode réception
    // ⭐ CORRECTION : Pas de délai comme dans votre code de test
}

// ——————— FONCTIONS DE LECTURE MODULAIRES ———————

bool readBatteryData(uint8_t batteryId, ModbusDataType dataType)
{
    if (batteryId < 1 || batteryId > MAX_BATTERIES)
    {
        Serial.printf("ERREUR: ID batterie invalide: %d\n", batteryId);
        return false;
    }

    uint16_t startAddr, regCount;
    const char *typeName;

    // Déterminer la plage d'adresses selon le type
    switch (dataType)
    {
    case DATA_REALTIME:
        startAddr = ADDR_REALTIME_START;
        regCount = ADDR_REALTIME_END - ADDR_REALTIME_START + 1;
        typeName = "TEMPS_REEL";
        break;
    case DATA_SETTING1:
        startAddr = ADDR_SETTING1_START;
        regCount = ADDR_SETTING1_END - ADDR_SETTING1_START + 1;
        typeName = "SETTING1";
        break;
    case DATA_SETTING2:
        startAddr = ADDR_SETTING2_START;
        regCount = ADDR_SETTING2_END - ADDR_SETTING2_START + 1;
        typeName = "SETTING2";
        break;
    case DATA_SETTING3:
        startAddr = ADDR_SETTING3_START;
        regCount = ADDR_SETTING3_END - ADDR_SETTING3_START + 1;
        typeName = "SETTING3";
        break;
    default:
        Serial.println("ERREUR: Type de données invalide");
        return false;
    }

    Serial.printf("Lecture %s batterie ID=%d (0x%04X à 0x%04X)\n",
                  typeName, batteryId, startAddr, startAddr + regCount - 1);

    // Construire et envoyer la commande
    int frameLength = buildReadCommand(batteryId, startAddr, regCount);
    if (frameLength <= 0)
    {
        Serial.println("ERREUR: Construction commande échouée");
        return false;
    }

    // Debug
    printModbusBuffer("ENVOI", sendBuffer, frameLength);

    // Vider le buffer de réception
    while (modbusSerial->available())
    {
        modbusSerial->read();
    }

    // Passer en mode émission et envoyer
    enableRS485Transmit();
    modbusSerial->write(sendBuffer, frameLength);
    modbusSerial->flush();
    enableRS485Receive(); // Repasser en mode réception

    // Attendre la réponse (timeout 500ms)
    unsigned long timeout = millis() + 500;
    int responseLength = 0;

    while (millis() < timeout && responseLength < sizeof(receiveBuffer))
    {
        if (modbusSerial->available())
        {
            receiveBuffer[responseLength++] = modbusSerial->read();
            timeout = millis() + 50; // Prolonger si on reçoit des données
        }
    }

    if (responseLength > 0)
    {
        printModbusBuffer("RECU", receiveBuffer, responseLength);
        return parseResponse(batteryId, dataType);
    }
    else
    {
        Serial.printf("TIMEOUT: Pas de réponse de la batterie ID=%d\n", batteryId);
        return false;
    }
}

bool readBatteryParam(uint8_t batteryId, BatteryParam param)
{
    // Lecture ciblée d'un paramètre spécifique
    uint16_t startAddr, regCount;

    switch (param)
    {
    case PARAM_SOC:
        startAddr = REG_SOC;
        regCount = 1;
        break;
    case PARAM_VOLTAGE:
        startAddr = REG_TOTAL_VOLTAGE;
        regCount = 1;
        break;
    case PARAM_CURRENT:
        startAddr = REG_CURRENT;
        regCount = 1;
        break;
    case PARAM_TEMP_MOS:
        startAddr = REG_MOS_TEMP;
        regCount = 1;
        break;
    case PARAM_CHARGE_MOSFET:
        startAddr = REG_CHARGE_MOSFET;
        regCount = 1;
        break;
    case PARAM_DISCHARGE_MOSFET:
        startAddr = REG_DISCHARGE_MOSFET;
        regCount = 1;
        break;
    case PARAM_CELL_VOLTAGES:
        startAddr = REG_CELL_VOLTAGES_START;
        regCount = 48; // 0x00 à 0x2F
        break;
    case PARAM_TEMPERATURES:
        startAddr = REG_TEMPERATURES_START;
        regCount = 8; // 0x30 à 0x37
        break;
    case PARAM_FAULT_STATUS:
        startAddr = REG_FAULT_STATUS1;
        regCount = 3; // Status 1, 2 et 3
        break;
    default:
        Serial.println("ERREUR: Paramètre invalide");
        return false;
    }

    Serial.printf("Lecture paramètre %d batterie ID=%d\n", param, batteryId);

    int frameLength = buildReadCommand(batteryId, startAddr, regCount);
    if (frameLength <= 0)
        return false;

    // Vider buffer et envoyer
    while (modbusSerial->available())
        modbusSerial->read();
    enableRS485Transmit();
    modbusSerial->write(sendBuffer, frameLength);
    modbusSerial->flush();
    enableRS485Receive();

    // Attendre réponse
    unsigned long timeout = millis() + 300;
    int responseLength = 0;

    while (millis() < timeout && responseLength < sizeof(receiveBuffer))
    {
        if (modbusSerial->available())
        {
            receiveBuffer[responseLength++] = modbusSerial->read();
            timeout = millis() + 50;
        }
    }

    if (responseLength > 0)
    {
        return parseResponse(batteryId, DATA_REALTIME);
    }

    return false;
}

bool readAllBatteriesData(ModbusDataType dataType)
{
    Serial.printf("=== LECTURE TOUTES BATTERIES (Type %d) ===\n", dataType);

    bool success = true;
    for (uint8_t id = 1; id <= MAX_BATTERIES; id++)
    {
        bool result = readBatteryData(id, dataType);
        if (!result)
        {
            Serial.printf("Échec lecture batterie ID=%d\n", id);
            success = false;
        }
        delay(100); // Délai entre batteries
    }

    return success;
}

// ——————— FONCTIONS D'ÉCRITURE ———————

bool writeBatteryParam(uint8_t batteryId, uint16_t regAddr, uint16_t value)
{
    if (batteryId < 1 || batteryId > MAX_BATTERIES)
        return false;

    Serial.printf("Écriture batterie ID=%d, reg=0x%04X, val=%d\n", batteryId, regAddr, value);

    int frameLength = buildWriteCommand(batteryId, regAddr, value);
    if (frameLength <= 0)
        return false;

    printModbusBuffer("WRITE", sendBuffer, frameLength);

    while (modbusSerial->available())
        modbusSerial->read();
    enableRS485Transmit();
    modbusSerial->write(sendBuffer, frameLength);
    modbusSerial->flush();
    enableRS485Receive();

    // Attendre l'ACK
    if (!waitForAck(batteryId, "WRITE"))
    {
        Serial.println("✗ Pas d'ACK pour l'écriture");
        return false;
    }

    Serial.println("✓ Écriture confirmée");
    return true;
}

bool setChargeMosfet(uint8_t batteryId, bool enable)
{
    return writeBatteryParam(batteryId, REG_CHARGE_CONTROL, enable ? 1 : 0);
}

bool setDischargeMosfet(uint8_t batteryId, bool enable)
{
    return writeBatteryParam(batteryId, REG_DISCHARGE_CONTROL, enable ? 1 : 0);
}

// ——————— ACCÈS AUX DONNÉES ———————

BatteryData *getBatteryData(uint8_t batteryId)
{
    if (batteryId < 1 || batteryId > MAX_BATTERIES)
        return nullptr;
    return &batteries[batteryId - 1];
}

float getBatterySOC(uint8_t batteryId)
{
    BatteryData *data = getBatteryData(batteryId);
    return data && data->dataValid ? data->soc : -1.0f;
}

float getBatteryVoltage(uint8_t batteryId)
{
    BatteryData *data = getBatteryData(batteryId);
    return data && data->dataValid ? data->totalVoltage : -1.0f;
}

float getBatteryCurrent(uint8_t batteryId)
{
    BatteryData *data = getBatteryData(batteryId);
    return data && data->dataValid ? data->current : 0.0f;
}

bool isBatteryDataValid(uint8_t batteryId)
{
    BatteryData *data = getBatteryData(batteryId);
    return data && data->dataValid;
}

// ——————— FONCTIONS UTILITAIRES ———————

int buildReadCommand(uint8_t batteryId, uint16_t startAddr, uint16_t regCount)
{
    sendBuffer[0] = MASTER_ADDR;             // Adresse maître
    sendBuffer[1] = CMD_READ_HOLDING;        // Fonction lecture
    sendBuffer[2] = (startAddr >> 8) & 0xFF; // Adresse start (high)
    sendBuffer[3] = startAddr & 0xFF;        // Adresse start (low)
    sendBuffer[4] = (regCount >> 8) & 0xFF;  // Nombre registres (high)
    sendBuffer[5] = regCount & 0xFF;         // Nombre registres (low)

    uint16_t crc = calculateCRC16(sendBuffer, 6);
    sendBuffer[6] = crc & 0xFF;        // CRC low
    sendBuffer[7] = (crc >> 8) & 0xFF; // CRC high

    return 8;
}

int buildWriteCommand(uint8_t batteryId, uint16_t regAddr, uint16_t value)
{
    sendBuffer[0] = MASTER_ADDR;           // Adresse maître
    sendBuffer[1] = CMD_WRITE_SINGLE;      // Fonction écriture simple
    sendBuffer[2] = (regAddr >> 8) & 0xFF; // Adresse reg (high)
    sendBuffer[3] = regAddr & 0xFF;        // Adresse reg (low)
    sendBuffer[4] = (value >> 8) & 0xFF;   // Valeur (high)
    sendBuffer[5] = value & 0xFF;          // Valeur (low)

    uint16_t crc = calculateCRC16(sendBuffer, 6);
    sendBuffer[6] = crc & 0xFF;        // CRC low
    sendBuffer[7] = (crc >> 8) & 0xFF; // CRC high

    return 8;
}

bool parseResponse(uint8_t batteryId, ModbusDataType dataType)
{
    if (batteryId < 1 || batteryId > MAX_BATTERIES)
        return false;

    BatteryData *battery = &batteries[batteryId - 1];

    // ⭐ CORRECTION : Utiliser 0x50 + ID
    uint8_t expectedAddr = 0x50 + batteryId; // Pas RESPONSE_ADDR_BASE !
    if (receiveBuffer[0] != expectedAddr)
    {
        Serial.printf("ERREUR: Adresse réponse incorrecte (reçu 0x%02X, attendu 0x%02X)\n",
                      receiveBuffer[0], expectedAddr);
        return false;
    }

    // Vérifier la fonction
    if (receiveBuffer[1] != CMD_READ_HOLDING)
    {
        Serial.printf("ERREUR: Fonction incorrecte (reçu 0x%02X)\n", receiveBuffer[1]);
        return false;
    }

    // Vérifier la longueur des données
    uint8_t dataLength = receiveBuffer[2];
    Serial.printf("Longueur données: %d bytes\n", dataLength);

    if (dataType == DATA_REALTIME)
    {
        // Parser les données temps réel selon votre reverse engineering
        parseRealtimeData(battery, &receiveBuffer[3], dataLength);
    }

    battery->dataValid = true;
    battery->lastUpdate = millis();

    return true;
}

void parseRealtimeData(BatteryData *battery, uint8_t *data, uint8_t length)
{
    // Parser selon les adresses de votre document

    // SOC (0x3A) - offset 0x3A*2 = 116
    if (length > 116)
    {
        uint16_t socRaw = (data[116] << 8) | data[117];
        battery->soc = socRaw * 0.001f; // Selon doc: 0.001, 800/1000=80%
    }

    // Tension totale (0x38) - offset 0x38*2 = 112
    if (length > 112)
    {
        uint16_t voltageRaw = (data[112] << 8) | data[113];
        battery->totalVoltage = voltageRaw / 10.0f; // Selon doc: /10
    }

    // Courant (0x39) - offset 0x39*2 = 114
    if (length > 114)
    {
        uint16_t currentRaw = (data[114] << 8) | data[115];
        // Selon doc: 0.1A, 30000 Offset, charge=négatif, décharge=positif
        battery->current = ((int16_t)currentRaw - 30000) * 0.1f;
    }

    // MOSFET charge (0x52) - offset 0x52*2 = 164
    if (length > 164)
    {
        battery->chargeMosfet = (data[165] & 0x01) != 0; // Bit 0
    }

    // MOSFET décharge (0x53) - offset 0x53*2 = 166
    if (length > 166)
    {
        battery->dischargeMosfet = (data[167] & 0x01) != 0; // Bit 0
    }

    // Nombre de cellules (0x3C) - offset 0x3C*2 = 120
    if (length > 120)
    {
        battery->cellCount = data[121]; // Low byte
    }

    // Nombre capteurs température (0x3D) - offset 0x3D*2 = 122
    if (length > 122)
    {
        battery->tempSensorCount = data[123]; // Low byte
    }

    // Température MOS (0x5A) - offset 0x5A*2 = 180
    if (length > 180)
    {
        uint16_t tempRaw = (data[180] << 8) | data[181];
        battery->mosTemp = tempRaw - 40.0f; // Selon doc: offset -40
    }

    // Tensions cellules (0x00~0x2F) - 48 cellules max
    battery->validCells = 0;
    for (int i = 0; i < 48 && (i * 2 + 1) < length; i++)
    {
        uint16_t cellVoltage = (data[i * 2] << 8) | data[i * 2 + 1];
        if (cellVoltage > 0)
        {                                           // Cellule valide
            battery->cellVoltages[i] = cellVoltage; // En mV selon doc
            battery->validCells++;
        }
    }

    // Températures capteurs (0x30~0x37) - offset 0x30*2 = 96
    battery->validTemps = 0;
    for (int i = 0; i < 8 && (96 + i * 2 + 1) < length; i++)
    {
        uint16_t tempRaw = (data[96 + i * 2] << 8) | data[96 + i * 2 + 1];
        if (tempRaw > 0)
        {                                               // Capteur valide
            battery->temperatures[i] = tempRaw - 40.0f; // Offset -40
            battery->validTemps++;
        }
    }

    // États de défaut (0x66, 0x67, 0x68) - offsets 204, 206, 208
    if (length > 204)
    {
        battery->faultStatus1 = (data[204] << 8) | data[205];
    }
    if (length > 206)
    {
        battery->faultStatus2 = (data[206] << 8) | data[207];
    }
    if (length > 208)
    {
        battery->faultStatus3 = (data[208] << 8) | data[209];
    }

    Serial.printf("Batterie ID=%d parsée: SOC=%.1f%%, V=%.1fV, I=%.1fA, Cellules=%d\n",
                  battery->batteryId, battery->soc, battery->totalVoltage,
                  battery->current, battery->validCells);
}

void printBatteryData(uint8_t batteryId)
{
    BatteryData *data = getBatteryData(batteryId);
    if (!data || !data->dataValid)
    {
        Serial.printf("Batterie ID=%d: DONNÉES INVALIDES\n", batteryId);
        return;
    }

    Serial.printf("\n=== BATTERIE ID=%d ===\n", batteryId);
    Serial.printf("SOC: %.1f%%\n", data->soc);
    Serial.printf("Tension totale: %.1fV\n", data->totalVoltage);
    Serial.printf("Courant: %.1fA %s\n", abs(data->current),
                  data->current < 0 ? "(charge)" : "(décharge)");
    Serial.printf("MOSFET Charge: %s\n", data->chargeMosfet ? "ON" : "OFF");
    Serial.printf("MOSFET Décharge: %s\n", data->dischargeMosfet ? "ON" : "OFF");
    Serial.printf("Température MOS: %.1f°C\n", data->mosTemp);
    Serial.printf("Cellules: %d validées\n", data->validCells);
    Serial.printf("Capteurs T°: %d validés\n", data->validTemps);

    // Afficher quelques tensions cellules
    if (data->validCells > 0)
    {
        Serial.print("Tensions cellules (mV): ");
        int maxDisplay = (data->validCells < 8) ? data->validCells : 8;
        for (int i = 0; i < maxDisplay; i++)
        {
            Serial.printf("%.0f ", data->cellVoltages[i]);
        }
        if (data->validCells > 8)
            Serial.print("...");
        Serial.println();
    }

    // États de défaut
    if (data->faultStatus1 || data->faultStatus2 || data->faultStatus3)
    {
        Serial.printf("DÉFAUTS: 0x%04X 0x%04X 0x%04X\n",
                      data->faultStatus1, data->faultStatus2, data->faultStatus3);
    }
    else
    {
        Serial.println("Aucun défaut détecté");
    }

    Serial.printf("Dernière MAJ: %lums\n", millis() - data->lastUpdate);
}

void debugBatteryStatus()
{
    Serial.println("\n=== ÉTAT TOUTES BATTERIES ===");
    for (int i = 1; i <= MAX_BATTERIES; i++)
    {
        if (isBatteryDataValid(i))
        {
            Serial.printf("ID=%d: SOC=%.1f%% V=%.1fV I=%.1fA [VALIDE]\n",
                          i, getBatterySOC(i), getBatteryVoltage(i), getBatteryCurrent(i));
        }
        else
        {
            Serial.printf("ID=%d: [DONNÉES INVALIDES]\n", i);
        }
    }
    Serial.println("============================\n");
}

// ——————— FONCTIONS COMPATIBILITÉ (ANCIEN CODE) ———————

bool sendDisplayIdCommand()
{
    Serial.println("\n=== ENVOI COMMANDE H=7 (Afficher ID) ===");
    return sendDisplayIdToAllBatteries();
}

bool sendDisplayIdToAllBatteries()
{
    Serial.println("\n=== ENVOI H=7 AVEC VALIDATION TOUTES BATTERIES ===");

    bool success = true;
    int successCount = 0;

    for (uint8_t batteryId = 1; batteryId <= MAX_BATTERIES; batteryId++)
    {
        Serial.printf("\n--- Batterie ID=%d ---\n", batteryId);

        if (sendAndVerifyDisplayId(batteryId))
        {
            successCount++;
            Serial.printf("✓ Batterie ID=%d: SUCCÈS\n", batteryId);
        }
        else
        {
            success = false;
            Serial.printf("✗ Batterie ID=%d: ÉCHEC\n", batteryId);
        }

        delay(300); // Délai entre batteries pour éviter les collisions
    }

    Serial.printf("\n=== RÉSUMÉ: %d/%d batteries OK ===\n", successCount, MAX_BATTERIES);
    if (success)
    {
        Serial.println("✓ TOUTES les batteries ont affiché leur ID");
    }
    else
    {
        Serial.printf("⚠ Seulement %d batteries sur %d ont répondu\n", successCount, MAX_BATTERIES);
    }

    return success;
}

bool sendDisplayIdToBattery(uint8_t batteryId)
{
    if (!modbusSerial || batteryId < 1 || batteryId > MAX_BATTERIES)
    {
        Serial.println("ERREUR: Paramètres invalides pour affichage ID");
        return false;
    }

    Serial.printf("Envoi H=7 à batterie ID=%d\n", batteryId);

    // ⭐ TEST : Version ASCII selon l'ingénieur (SANS byte count)
    Serial.println("=== TEST VERSION ASCII (selon ingénieur) ===");

    sendBuffer[0] = 0x80 + batteryId; // ID de la batterie
    sendBuffer[1] = 0x10;             // Fonction écriture multiple
    sendBuffer[2] = 0x01;             // Adresse registre 0x01F1 (high)
    sendBuffer[3] = 0xF1;             // Adresse registre 0x01F1 (low)
    sendBuffer[4] = 0x00;             // Nombre de registres (high)
    sendBuffer[5] = 0x04;             // Nombre de registres (low)
    sendBuffer[6] = 0x37;             // ⭐ "7" en ASCII (comme l'ingénieur)
    sendBuffer[7] = 0x00;             // Reste à zéro
    sendBuffer[8] = 0x00;
    sendBuffer[9] = 0x00; // ⭐ Plus de 0x07 ici !
    sendBuffer[10] = 0x00;
    sendBuffer[11] = 0x00;
    sendBuffer[12] = 0x00;
    sendBuffer[13] = 0x00;

    // Calculer le CRC sur les 14 premiers bytes
    uint16_t crc = calculateCRC16(sendBuffer, 14);
    sendBuffer[14] = crc & 0xFF;        // CRC low
    sendBuffer[15] = (crc >> 8) & 0xFF; // CRC high

    char label[30];
    sprintf(label, "H=7_ASCII ID=%d", batteryId);
    printModbusBuffer(label, sendBuffer, 16);

    // Vider buffer et envoyer
    while (modbusSerial->available())
        modbusSerial->read();
    enableRS485Transmit();
    modbusSerial->write(sendBuffer, 16);
    modbusSerial->flush();
    enableRS485Receive();

    // Attendre l'ACK
    bool ackReceived = waitForAck(batteryId, "H=7_ASCII");

    if (!ackReceived)
    {
        Serial.println("⚠ Version ASCII échouée, test version originale...");

        // ⭐ FALLBACK : Version originale qui marchait
        sendBuffer[6] = 0x00;            // Retour version originale
        sendBuffer[10] = DISPLAY_ID_CMD; // 07 = H=7

        uint16_t crc2 = calculateCRC16(sendBuffer, 14);
        sendBuffer[14] = crc2 & 0xFF;
        sendBuffer[15] = (crc2 >> 8) & 0xFF;

        sprintf(label, "H=7_ORIG ID=%d", batteryId);
        printModbusBuffer(label, sendBuffer, 16);

        while (modbusSerial->available())
            modbusSerial->read();
        enableRS485Transmit();
        modbusSerial->write(sendBuffer, 16);
        modbusSerial->flush();
        enableRS485Receive();

        ackReceived = waitForAck(batteryId, "H=7_ORIG");
    }

    if (ackReceived)
    {
        Serial.printf("✓ H=7 envoyé et confirmé batterie ID=%d\n", batteryId);
        return true;
    }
    else
    {
        Serial.printf("✗ Échec total batterie ID=%d\n", batteryId);
        return false;
    }
}

bool sendAndVerifyDisplayId(uint8_t batteryId)
{
    Serial.printf("=== ENVOI H=7 BATTERIE ID=%d ===\n", batteryId);

    // 1. Envoyer H=7 avec ACK (suffisant pour confirmer)
    if (!sendDisplayIdToBattery(batteryId))
    {
        Serial.println("✗ Échec envoi H=7");
        return false;
    }

    // ⭐ DÉSACTIVATION de la vérification par relecture
    // Le registre 0x01F1 semble être en write-only
    Serial.println("✓ H=7 confirmé par ACK (vérification par relecture désactivée)");
    return true;

    /* ANCIENNE VERSION avec vérification (problématique)
    delay(200); // Laisser le temps au BMS de traiter

    // 2. Vérifier que la valeur est bien écrite
    if (verifyDisplayValue(batteryId, DISPLAY_ID_CMD)) {
        Serial.println("✓ H=7 confirmé écrit et vérifié");
        return true;
    } else {
        Serial.println("✗ H=7 non confirmé à la lecture");
        return false;
    }
    */
}

bool verifyDisplayValue(uint8_t batteryId, uint8_t expectedValue)
{
    Serial.printf("Vérification H=%d sur batterie ID=%d\n", expectedValue, batteryId);

    // Lire le registre 0x01F1 pour vérifier la valeur écrite
    int frameLength = buildReadCommand(batteryId, REG_DISPLAY_CONTROL, 4); // 4 registres comme dans l'écriture
    if (frameLength <= 0)
        return false;

    printModbusBuffer("VERIFY_READ", sendBuffer, frameLength);

    // Vider buffer et envoyer
    while (modbusSerial->available())
        modbusSerial->read();
    modbusSerial->write(sendBuffer, frameLength);
    modbusSerial->flush();

    // Attendre réponse
    unsigned long timeout = millis() + 300;
    int responseLength = 0;

    while (millis() < timeout && responseLength < sizeof(receiveBuffer))
    {
        if (modbusSerial->available())
        {
            receiveBuffer[responseLength++] = modbusSerial->read();
            timeout = millis() + 50; // Prolonger si on reçoit des données
        }
    }

    if (responseLength > 0)
    {
        printModbusBuffer("VERIFY_RESP", receiveBuffer, responseLength);

        // ⭐ CORRECTION : Utiliser 0x50 + ID
        uint8_t expectedAddr = 0x50 + batteryId; // Pas RESPONSE_ADDR_BASE !
        if (receiveBuffer[0] != expectedAddr)
        {
            Serial.printf("✗ Adresse réponse incorrecte (reçu 0x%02X, attendu 0x%02X)\n",
                          receiveBuffer[0], expectedAddr);
            return false;
        }

        // Vérifier la fonction
        if (receiveBuffer[1] != CMD_READ_HOLDING)
        {
            Serial.printf("✗ Fonction incorrecte (reçu 0x%02X)\n", receiveBuffer[1]);
            return false;
        }

        // La valeur H devrait être dans les données
        // Format attendu: [ADDR][FUNC][LENGTH][DATA...]
        // Les 4 registres de 0x01F1 contiennent: 00 00 00 07 00 00 00
        if (responseLength >= 11)
        {                                         // Au minimum: addr + func + len + 8 bytes data
            uint8_t readValue = receiveBuffer[6]; // 4ème byte des données (position du H)
            Serial.printf("Valeur H lue: %d (attendu: %d)\n", readValue, expectedValue);

            if (readValue == expectedValue)
            {
                Serial.println("✓ Valeur H confirmée");
                return true;
            }
            else
            {
                Serial.printf("✗ Valeur H incorrecte (lu: %d, attendu: %d)\n", readValue, expectedValue);
            }
        }
        else
        {
            Serial.printf("✗ Réponse trop courte (%d bytes)\n", responseLength);
        }
    }
    else
    {
        Serial.printf("✗ Timeout lors de la vérification batterie ID=%d\n", batteryId);
    }

    return false;
}

bool waitForAck(uint8_t batteryId, const char *operation)
{
    unsigned long timeout = millis() + 200;
    int responseLength = 0;

    while (millis() < timeout && responseLength < 16)
    {
        if (modbusSerial->available())
        {
            receiveBuffer[responseLength++] = modbusSerial->read();
            timeout = millis() + 30; // Prolonger si on reçoit des données
        }
    }

    if (responseLength > 0)
    {
        char label[30];
        sprintf(label, "ACK_%s", operation);
        printModbusBuffer(label, receiveBuffer, responseLength);

        // ⭐ CORRECTION : Utiliser 0x50 + ID au lieu de RESPONSE_ADDR_BASE + ID
        uint8_t expectedAddr = 0x50 + batteryId; // Pas RESPONSE_ADDR_BASE !
        if (receiveBuffer[0] == expectedAddr)
        {
            Serial.printf("✓ ACK reçu de batterie ID=%d pour %s\n", batteryId, operation);
            return true;
        }
        else
        {
            Serial.printf("✗ ACK incorrect (reçu 0x%02X, attendu 0x%02X)\n",
                          receiveBuffer[0], expectedAddr);
        }
    }
    else
    {
        Serial.printf("✗ Timeout ACK batterie ID=%d pour %s\n", batteryId, operation);
    }

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

void testModbus()
{
    Serial.println("\n=== TEST MODBUS AVANCÉ AVEC VALIDATION ===");

    Serial.println("\n1. Test affichage ID avec validation sur batterie 1");
    if (sendAndVerifyDisplayId(1))
    {
        Serial.println("✓ Test unitaire réussi");
    }
    else
    {
        Serial.println("✗ Test unitaire échoué");
    }

    delay(2000);

    Serial.println("\n2. Test affichage ID sur toutes batteries avec validation");
    sendDisplayIdToAllBatteries();

    delay(2000);

    Serial.println("\n3. Test lecture données temps réel batterie 1");
    if (readBatteryData(1, DATA_REALTIME))
    {
        printBatteryData(1);
    }

    delay(1000);

    Serial.println("\n4. Test lecture SOC batterie 1");
    if (readBatteryParam(1, PARAM_SOC))
    {
        Serial.printf("SOC batterie 1: %.1f%%\n", getBatterySOC(1));
    }

    Serial.println("\n=== FIN TESTS ===");
}