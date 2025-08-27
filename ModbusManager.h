#ifndef MODBUS_MANAGER_H
#define MODBUS_MANAGER_H

#include <Arduino.h>
#include <HardwareSerial.h>
#include "Config.h"
// ——————— DONNÉES DES BATTERIES ———————
// Structure pour stocker les données consolidées de l'ensemble du parc de batteries.
struct AggregateBatteryMetrics
{
    float averageSoc;     // SOC moyen de toutes les batteries
    float averageVoltage; // Tension moyenne
    float totalCurrent;   // Courant total (somme des courants)
    float averageTemp;    // Température moyenne des MOSFETs
    bool isDataValid;     // Indique si les données sont fiables
};

// ——————— CONSTANTES MODBUS ———————
// Paramètres de communication
#define BAUD_RATE 9600   // Vitesse de communication série
#define MASTER_ADDR 0x81 // Adresse de l'ESP32 en tant que maître vvv

// Commandes Modbus standard
#define CMD_READ_HOLDING 0x03   // Lecture de registres de maintien
#define CMD_WRITE_SINGLE 0x06   // Écriture d'un registre simple
#define CMD_WRITE_MULTIPLE 0x10 // Écriture de registres multiples

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

// Commandes d'affichage sur LCD batterie
#define DISPLAY_ID_CMD 7 // Commande pour afficher ID
#define ASCII_7 0x37     // Valeur ASCII pour "7"

// ——————— VARIABLES GLOBALES ———————
extern HardwareSerial *modbusSerial; // Pointeur vers port série Modbus
extern uint8_t sendBuffer[256];      // Buffer d'émission des trames
extern uint8_t receiveBuffer[256];   // Buffer de réception des trames

// --- FONCTIONS D'INITIALISATION ---
void initModbus(HardwareSerial *serial); // Initialise le système Modbus avec le port série spécifié
void enableRS485Transmit();              // Active le mode émission RS485 (DE/RE = HIGH)
void enableRS485Receive();               // Active le mode réception RS485 (DE/RE = LOW)

// --- FONCTION DE LECTURE PRINCIPALE ---
void readAggregateBatteryMetrics(uint8_t configuredBatteryCount, AggregateBatteryMetrics *metrics);

// --- FONCTIONS D'ÉCRITURE (pour l'appairage) ---
// Envoie commande d'affichage H=7 à une batterie spécifique
bool sendDisplayIdToBattery(uint8_t batteryId, uint8_t asciiValue);
// Change l'ID d'une batterie vers ID=1 (utilisé avant appairage)
bool changeBatteryIdTo1(uint8_t batteryId);
// Change l'ID de toutes les batteries vers 1 (reset complet)
bool changeAllBatteriesToId1();
// Change l'ID d'une batterie de 1 vers un nouvel ID (appairage)
bool changeBatteryIdFrom1To(uint8_t newId);

// --- FONCTIONS UTILITAIRES MODBUS ---
// Calcule le CRC16 Modbus d'un buffer de données
uint16_t calculateCRC16(uint8_t *data, uint8_t length);
// Construit une trame de lecture Modbus
int buildReadCommand(uint8_t batteryId, uint16_t startAddr, uint16_t regCount);
// Affiche un buffer Modbus en hexadécimal pour debug
void printModbusBuffer(const char *label, uint8_t *buffer, int length);
// Attend et vérifie la réception d'un ACK d'une batterie
bool waitForAck(uint8_t batteryId, const char *operation);
#endif