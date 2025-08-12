#ifndef MODBUS_MANAGER_H
#define MODBUS_MANAGER_H

#include <Arduino.h>
#include <HardwareSerial.h>

class ModbusManager
{
private:
    HardwareSerial *serial;

    // Configuration Modbus Daly
    static const uint8_t MASTER_ADDR = 0x81;
    static const uint32_t BAUD_RATE = 9600;

    // Registres importants
    static const uint16_t DISPLAY_REGISTER = 0x01F1; // Contrôle affichage
    static const uint16_t ID_REGISTER = 0x0100;      // ID de la batterie

    // Commandes d'affichage
    static const uint8_t DISPLAY_ID_CMD = 7;     // H=7 pour afficher ID
    static const uint8_t DISPLAY_ID_CONFIRM = 9; // H=9 pour confirmer

    // Buffer pour les trames
    uint8_t sendBuffer[32];
    uint8_t receiveBuffer[32];

    // CRC16 Modbus
    uint16_t calculateCRC16(uint8_t *data, uint8_t length);

    // Construction de trame
    int buildDisplayCommand(uint8_t displayValue);

    // Debug
    void printBuffer(const char *label, uint8_t *buffer, int length);

public:
    ModbusManager(HardwareSerial *ser);

    void begin();

    // Commande simple : afficher l'ID sur toutes les batteries
    bool sendDisplayIdCommand();

    // NOUVEAU : Envoyer H=7 à toutes les batteries (ID 1-9)
    bool sendDisplayIdToAllBatteries();

    // NOUVEAU : Envoyer H=7 à une batterie spécifique
    bool sendDisplayIdToBattery(uint8_t batteryId);

    // Test de base
    void testModbus();
};

#endif