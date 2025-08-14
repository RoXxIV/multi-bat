#ifndef MODBUS_MANAGER_H
#define MODBUS_MANAGER_H

#include <Arduino.h>
#include <HardwareSerial.h>
#include "Config.h"

// ——————— CONSTANTES MODBUS ———————
#define BAUD_RATE 9600
#define MASTER_ADDR 0x81
#define RESPONSE_ADDR_BASE 0x50 // Les BMS répondent avec 0x50 + ID

// Commandes Modbus
#define CMD_READ_HOLDING 0x03
#define CMD_WRITE_SINGLE 0x06
#define CMD_WRITE_MULTIPLE 0x10

// Plages d'adresses selon votre reverse engineering
#define ADDR_REALTIME_START 0x0000
#define ADDR_REALTIME_END 0x007F
#define ADDR_SETTING1_START 0x0100
#define ADDR_SETTING1_END 0x0177
#define ADDR_SETTING2_START 0x0178
#define ADDR_SETTING2_END 0x01DF
#define ADDR_SETTING3_START 0x01E0
#define ADDR_SETTING3_END 0x01FD

// Registres importants (temps réel)
#define REG_CELL_VOLTAGES_START 0x00 // 0x00~0x2F
#define REG_TEMPERATURES_START 0x30  // 0x30~0x37
#define REG_TOTAL_VOLTAGE 0x38
#define REG_CURRENT 0x39
#define REG_SOC 0x3A
#define REG_HEARTBEAT 0x3B
#define REG_CELL_COUNT 0x3C
#define REG_TEMP_SENSOR_COUNT 0x3D
#define REG_CHARGE_MOSFET 0x52
#define REG_DISCHARGE_MOSFET 0x53
#define REG_MOS_TEMP 0x5A
#define REG_FAULT_STATUS1 0x66
#define REG_FAULT_STATUS2 0x67
#define REG_FAULT_STATUS3 0x68

// Registres de commande
#define REG_DISPLAY_CONTROL 0x01F1
#define REG_CHARGE_CONTROL 0x0121
#define REG_DISCHARGE_CONTROL 0x0122

// Commandes d'affichage
#define DISPLAY_ID_CMD 7
#define DISPLAY_ID_CONFIRM 9

// ——————— ÉNUMÉRATIONS ———————
enum ModbusDataType
{
    DATA_REALTIME = 0,
    DATA_SETTING1 = 1,
    DATA_SETTING2 = 2,
    DATA_SETTING3 = 3
};

enum BatteryParam
{
    PARAM_SOC = 0,
    PARAM_VOLTAGE = 1,
    PARAM_CURRENT = 2,
    PARAM_TEMP_MOS = 3,
    PARAM_CHARGE_MOSFET = 4,
    PARAM_DISCHARGE_MOSFET = 5,
    PARAM_CELL_VOLTAGES = 6,
    PARAM_TEMPERATURES = 7,
    PARAM_FAULT_STATUS = 8
};

// ——————— STRUCTURES ———————
struct BatteryData
{
    uint8_t batteryId;
    bool dataValid;
    unsigned long lastUpdate;

    // Données principales
    float soc;          // %
    float totalVoltage; // V
    float current;      // A (+ = décharge, - = charge)
    uint8_t cellCount;
    uint8_t tempSensorCount;

    // États MOSFET
    bool chargeMosfet;
    bool dischargeMosfet;

    // Températures
    float mosTemp;     // °C
    float ambientTemp; // °C

    // Tensions cellules (max 48 selon 0x00~0x2F)
    float cellVoltages[48];
    uint8_t validCells;

    // Températures capteurs (max 8 selon 0x30~0x37)
    float temperatures[8];
    uint8_t validTemps;

    // États de défaut
    uint16_t faultStatus1;
    uint16_t faultStatus2;
    uint16_t faultStatus3;
};

// ——————— VARIABLES GLOBALES ———————
extern HardwareSerial *modbusSerial;
extern uint8_t sendBuffer[256];
extern uint8_t receiveBuffer[256];
extern BatteryData batteries[MAX_BATTERIES];

// ——————— FONCTIONS PUBLIQUES ———————

// Initialisation
void initModbus(HardwareSerial *serial);

// Fonctions de lecture modulaires
bool readBatteryData(uint8_t batteryId, ModbusDataType dataType = DATA_REALTIME);
bool readBatteryParam(uint8_t batteryId, BatteryParam param);
bool readAllBatteriesData(ModbusDataType dataType = DATA_REALTIME);

// Fonctions d'écriture
bool writeBatteryParam(uint8_t batteryId, uint16_t regAddr, uint16_t value);
bool sendDisplayIdToBattery(uint8_t batteryId);
bool sendDisplayIdToAllBatteries();

// Contrôles MOSFET
bool setChargeMosfet(uint8_t batteryId, bool enable);
bool setDischargeMosfet(uint8_t batteryId, bool enable);

// Accès aux données
BatteryData *getBatteryData(uint8_t batteryId);
float getBatterySOC(uint8_t batteryId);
float getBatteryVoltage(uint8_t batteryId);
float getBatteryCurrent(uint8_t batteryId);
bool isBatteryDataValid(uint8_t batteryId);

// Fonctions utilitaires
uint16_t calculateCRC16(uint8_t *data, uint8_t length);
int buildReadCommand(uint8_t batteryId, uint16_t startAddr, uint16_t regCount);
int buildWriteCommand(uint8_t batteryId, uint16_t regAddr, uint16_t value);
bool parseResponse(uint8_t batteryId, ModbusDataType dataType);
void printModbusBuffer(const char *label, uint8_t *buffer, int length);
void printBatteryData(uint8_t batteryId);

// Debug et test
void testModbus();
void debugBatteryStatus();

// Déclaration de la fonction de parsing
void parseRealtimeData(BatteryData *battery, uint8_t *data, uint8_t length);

#endif