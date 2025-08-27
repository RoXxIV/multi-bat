#ifndef MODBUS_MANAGER_H
#define MODBUS_MANAGER_H

#include <Arduino.h>
#include <HardwareSerial.h>
#include "Config.h"

// ——————— CONSTANTES MODBUS ———————
// Paramètres de communication
#define BAUD_RATE 9600          // Vitesse de communication série
#define MASTER_ADDR 0x81        // Adresse de l'ESP32 en tant que maître vvv
#define RESPONSE_ADDR_BASE 0x50 // Base d'adresse des réponses BMS (0x50 + ID)

// Commandes Modbus standard
#define CMD_READ_HOLDING 0x03   // Lecture de registres de maintien
#define CMD_WRITE_SINGLE 0x06   // Écriture d'un registre simple
#define CMD_WRITE_MULTIPLE 0x10 // Écriture de registres multiples

// Plages d'adresses des registres BMS
#define ADDR_REALTIME_START 0x0000 // Début données temps réel
#define ADDR_REALTIME_END 0x007F   // Fin données temps réel
#define ADDR_SETTING1_START 0x0100 // Début paramètres groupe 1
#define ADDR_SETTING1_END 0x0177   // Fin paramètres groupe 1
#define ADDR_SETTING2_START 0x0178 // Début paramètres groupe 2
#define ADDR_SETTING2_END 0x01DF   // Fin paramètres groupe 2
#define ADDR_SETTING3_START 0x01E0 // Début paramètres groupe 3
#define ADDR_SETTING3_END 0x01FD   // Fin paramètres groupe 3

// Registres de données temps réel spécifiques
#define REG_CELL_VOLTAGES_START 0x00 // Début tensions cellules (0x00~0x2F)
#define REG_TEMPERATURES_START 0x30  // Début températures capteurs (0x30~0x37)
#define REG_TOTAL_VOLTAGE 0x38       // Tension totale de la batterie
#define REG_CURRENT 0x39             // Courant charge/décharge
#define REG_SOC 0x3A                 // SOC
#define REG_HEARTBEAT 0x3B           // Heartbeat
#define REG_CELL_COUNT 0x3C          // Nombre de cellules
#define REG_TEMP_SENSOR_COUNT 0x3D   // Nombre de capteurs de température
#define REG_CHARGE_MOSFET 0x52       // État MOSFET de charge
#define REG_DISCHARGE_MOSFET 0x53    // État MOSFET de décharge
#define REG_MOS_TEMP 0x5A            // Température des MOSFET
#define REG_FAULT_STATUS1 0x66       // État des défauts groupe 1
#define REG_FAULT_STATUS2 0x67       // État des défauts groupe 2
#define REG_FAULT_STATUS3 0x68       // État desodefauts groupe 3

// Registres de commande/configuration
#define REG_DISPLAY_CONTROL 0x01F1   // Contrôle affichage LCD batterie
#define REG_CHARGE_CONTROL 0x0121    // Contrôle MOSFET de charge
#define REG_DISCHARGE_CONTROL 0x0122 // Contrôle MOSFET de décharge

// Commandes d'affichage sur LCD batterie
#define DISPLAY_ID_CMD 7     // Commande pour afficher ID
#define DISPLAY_ID_CONFIRM 9 // ???
#define ASCII_7 0x37         // Valeur ASCII pour "7"
#define ASCII_9 0x39         // Valeur ASCII pour "9"

// ——————— ÉNUMÉRATIONS ———————
// Types de données à lire sur les batteries
enum ModbusDataType
{
    DATA_REALTIME = 0, // Données temps réel (tensions, courants, etc.)
    DATA_SETTING1 = 1, // Paramètres groupe 1
    DATA_SETTING2 = 2, // Paramètres groupe 2
    DATA_SETTING3 = 3  // Paramètres groupe 3
};
// Paramètres individuels lisibles sur une batterie
enum BatteryParam
{
    PARAM_SOC = 0,              // État de charge uniquement
    PARAM_VOLTAGE = 1,          // Tension totale uniquement
    PARAM_CURRENT = 2,          // Courant uniquement
    PARAM_TEMP_MOS = 3,         // Température MOSFET uniquement
    PARAM_CHARGE_MOSFET = 4,    // État MOSFET charge uniquement
    PARAM_DISCHARGE_MOSFET = 5, // État MOSFET décharge uniquement
    PARAM_CELL_VOLTAGES = 6,    // Toutes les tensions cellules
    PARAM_TEMPERATURES = 7,     // Toutes les températures capteurs
    PARAM_FAULT_STATUS = 8      // Tous les états de défaut
};

// ——————— STRUCTURES ———————
// Structure contenant toutes les données d'une batterie
struct BatteryData
{
    uint8_t batteryId;        // ID de la batterie (1-9)
    bool dataValid;           // Indicateur de validité des données
    unsigned long lastUpdate; // Timestamp dernière mise à jour
    // Données principales de monitoring
    float soc;               // État de charge en %
    float totalVoltage;      // Tension totale en V
    float current;           // Courant en A (+ = décharge, - = charge)
    uint8_t cellCount;       // Nombre de cellules détectées
    uint8_t tempSensorCount; // Nombre de capteurs température
    // États des MOSFET de puissance
    bool chargeMosfet;    // État MOSFET de charge (ON/OFF)
    bool dischargeMosfet; // État MOSFET de décharge (ON/OFF)
    // Données de température
    float mosTemp;     // Température MOSFET en °C
    float ambientTemp; // Température ambiante en °C
    // Tensions individuelles des cellules
    float cellVoltages[48]; // Max 48 cellules selon registres 0x00~0x2F
    uint8_t validCells;     // Nombre de cellules valides
    // Températures des capteurs individuels
    float temperatures[8]; // Max 8 capteurs selon registres 0x30~0x37
    uint8_t validTemps;    // Nombre de capteurs valides
    // États de défaut et alarmes
    uint16_t faultStatus1; // Défauts groupe 1 (bitfield)
    uint16_t faultStatus2; // Défauts groupe 2 (bitfield)
    uint16_t faultStatus3; // Défauts groupe 3 (bitfield)
};

// ——————— VARIABLES GLOBALES ———————
extern HardwareSerial *modbusSerial;         // Pointeur vers port série Modbus
extern uint8_t sendBuffer[256];              // Buffer d'émission des trames
extern uint8_t receiveBuffer[256];           // Buffer de réception des trames
extern BatteryData batteries[MAX_BATTERIES]; // Données de toutes les batteries

// ——————— FONCTIONS PUBLIQUES ———————
void initModbus(HardwareSerial *serial); // Initialise le système Modbus avec le port série spécifié
void enableRS485Transmit();              // Active le mode émission RS485 (DE/RE = HIGH)
void enableRS485Receive();               // Active le mode réception RS485 (DE/RE = LOW)

// ——————— FONCTIONS DE LECTURE ———————
// Lit un type de données complet d'une batterie (temps réel ou config)
bool readBatteryData(uint8_t batteryId, ModbusDataType dataType = DATA_REALTIME);
// Lit un paramètre spécifique d'une batterie (SOC, tension, etc.)
bool readBatteryParam(uint8_t batteryId, BatteryParam param);
// Lit les données de toutes les batteries connectées (scan complet)
bool readAllBatteriesData(ModbusDataType dataType = DATA_REALTIME);

// ——————— FONCTIONS D'ÉCRITURE ———————
// Écrit une valeur dans un registre spécifique d'une batterie
bool writeBatteryParam(uint8_t batteryId, uint16_t regAddr, uint16_t value);
// Envoie commande d'affichage H=7 à une batterie spécifique
bool sendDisplayIdToBattery(uint8_t batteryId, uint8_t asciiValue);
// Change l'ID d'une batterie vers ID=1 (utilisé avant appairage)
bool changeBatteryIdTo1(uint8_t batteryId);
// Change l'ID de toutes les batteries vers 1 (reset complet)
bool changeAllBatteriesToId1();
// Change l'ID d'une batterie de 1 vers un nouvel ID (appairage)
bool changeBatteryIdFrom1To(uint8_t newId);
// Contrôle manuel du MOSFET de charge (activation/désactivation)
bool setChargeMosfet(uint8_t batteryId, bool enable);
// Contrôle manuel du MOSFET de décharge (activation/désactivation)
bool setDischargeMosfet(uint8_t batteryId, bool enable);

// ——————— FONCTIONS D'ACCÈS AUX DONNÉES ———————
BatteryData *getBatteryData(uint8_t batteryId); // Retourne un pointeur vers les données d'une batterie
float getBatterySOC(uint8_t batteryId);         // Retourne le SOC d'une batterie (-1 si invalide)
float getBatteryVoltage(uint8_t batteryId);     // Retourne la tension d'une batterie (-1 si invalide)
float getBatteryCurrent(uint8_t batteryId);     // Retourne le courant d'une batterie (0 si invalide)
bool isBatteryDataValid(uint8_t batteryId);     // Vérifie si les données d'une batterie sont valides

// ——————— FONCTIONS UTILITAIRES ———————
// Calcule le CRC16 Modbus d'un buffer de données
uint16_t calculateCRC16(uint8_t *data, uint8_t length);
// Construit une trame de lecture Modbus
int buildReadCommand(uint8_t batteryId, uint16_t startAddr, uint16_t regCount);
// Construit une trame d'écriture Modbus
int buildWriteCommand(uint8_t batteryId, uint16_t regAddr, uint16_t value);
// Parse une réponse Modbus reçue d'une batterie
bool parseResponse(uint8_t batteryId, ModbusDataType dataType);
// Affiche un buffer Modbus en hexadécimal pour debug
void printModbusBuffer(const char *label, uint8_t *buffer, int length);
// Affiche toutes les données d'une batterie sur le port série
void printBatteryData(uint8_t batteryId);
// Attend et vérifie la réception d'un ACK d'une batterie
bool waitForAck(uint8_t batteryId, const char *operation);
// Parse spécifiquement les données temps réel d'une batterie
void parseRealtimeData(BatteryData *battery, uint8_t *data, uint8_t length);

#endif