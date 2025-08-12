#include "ModbusManager.h"

ModbusManager::ModbusManager(HardwareSerial *ser) : serial(ser)
{
    memset(sendBuffer, 0, sizeof(sendBuffer));
    memset(receiveBuffer, 0, sizeof(receiveBuffer));
}

void ModbusManager::begin()
{
    serial->begin(BAUD_RATE, SERIAL_8N1);
    Serial.println("ModbusManager initialisé - Baud: 9600 8N1");
    Serial.printf("Adresse maître: 0x%02X\n", MASTER_ADDR);
}

bool ModbusManager::sendDisplayIdCommand()
{
    Serial.println("\n=== ENVOI COMMANDE H=7 (Afficher ID) ===");

    // Construction de la trame pour H=7
    int frameLength = buildDisplayCommand(DISPLAY_ID_CMD);

    if (frameLength <= 0)
    {
        Serial.println("ERREUR: Construction trame échouée");
        return false;
    }

    // Debug : afficher la trame avant envoi
    printBuffer("ENVOI", sendBuffer, frameLength);

    // Vider le buffer de réception
    while (serial->available())
    {
        serial->read();
    }

    // Envoyer la trame
    serial->write(sendBuffer, frameLength);
    serial->flush();

    Serial.println("Commande H=7 envoyée !");
    Serial.println("-> Vérifiez l'affichage sur vos batteries");

    return true;
}

int ModbusManager::buildDisplayCommand(uint8_t displayValue)
{
    // ESSAI avec le format de votre documentation
    // 81 10 01 F1 00 04 37 00 00 00 XX 00 00 00

    sendBuffer[0] = MASTER_ADDR; // 81 = Adresse maître
    sendBuffer[1] = 0x10;        // 10 = Fonction écriture multiple registres
    sendBuffer[2] = 0x01;        // 01 F1 = Adresse de départ (high byte)
    sendBuffer[3] = 0xF1;        // 01 F1 = Adresse de départ (low byte)
    sendBuffer[4] = 0x00;        // 00 04 = Nombre de registres (high byte)
    sendBuffer[5] = 0x04;        // 00 04 = Nombre de registres (low byte)
    sendBuffer[6] = 0x37;        // 37 = selon votre doc (au lieu de 08)
    sendBuffer[7] = 0x00;        // Données : 00 00 00 XX
    sendBuffer[8] = 0x00;
    sendBuffer[9] = 0x00;
    sendBuffer[10] = displayValue; // XX = valeur H (7 ou 9)
    sendBuffer[11] = 0x00;         // Padding : 00 00 00 00
    sendBuffer[12] = 0x00;
    sendBuffer[13] = 0x00;

    // Calculer le CRC sur les 14 premiers bytes (trame plus courte)
    uint16_t crc = calculateCRC16(sendBuffer, 14);
    sendBuffer[14] = crc & 0xFF;        // CRC_L (low byte)
    sendBuffer[15] = (crc >> 8) & 0xFF; // CRC_H (high byte)

    return 16; // Longueur totale de la trame (plus courte)
}

uint16_t ModbusManager::calculateCRC16(uint8_t *data, uint8_t length)
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

void ModbusManager::printBuffer(const char *label, uint8_t *buffer, int length)
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

void ModbusManager::testModbus()
{
    Serial.println("\n=== TEST MODBUS BASIQUE ===");
    Serial.println("1. Vérifiez que vos batteries sont connectées");
    Serial.println("2. Toutes les batteries devraient afficher leur ID");
    Serial.println("3. Appuyez sur un bouton pour tester...");
}