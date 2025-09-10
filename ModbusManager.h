#ifndef MODBUS_MANAGER_H
#define MODBUS_MANAGER_H

#include <Arduino.h>
#include <HardwareSerial.h>
#include "Config.h"
#include "ModbusLib.h"

// ——————— DONNÉES DES BATTERIES ———————
struct IndividualBatteryData
{
    bool isValid;
    char serialNumber[33];
    float voltage;
    float current;
    float soc;
    uint16_t cellCount;
    float maxCellVoltage;        // en V
    float minCellVoltage;        // en V
    float cellVoltageDifference; // en V
    float maxCellTemp;           // en °C
    float minCellTemp;           // en °C

    bool chargeMosfetStatus;
    bool dischargeMosfetStatus;
    float mosTemp;
    float currentLimit;
    float temp1;
    float temp2;

    uint16_t wakeUpSource;
    uint16_t faultCode0_1;
    uint16_t faultCode2_3;
    uint16_t faultCode4_5;
    uint16_t faultCode6_7;
    uint16_t faultCode8_9;
    uint16_t faultCode10_11;
    uint16_t faultCode12_13;
};
// Structure pour stocker les données consolidées de l'ensemble du parc de batteries.
struct AggregateBatteryMetrics
{
    float averageSoc;     // SOC moyen de toutes les batteries
    float averageVoltage; // Tension moyenne
    float totalCurrent;   // Courant total (somme des courants)
    float averageTemp;    // Température moyenne des MOSFETs
    bool isDataValid;     // Indique si les données sont fiables
};
extern IndividualBatteryData individualBatteryMetrics[MAX_BATTERIES];

// ——————— CONSTANTES MODBUS ———————
// Paramètres de communication
#define BAUD_RATE 9600   // Vitesse de communication série
#define MASTER_ADDR 0x81 // Adresse de l'ESP32 en tant que maître

// --- FONCTIONS D'INITIALISATION ---
void initModbus();
void startModbus();
void stopModbus();

// --- FONCTION DE LECTURE PRINCIPALE ---
void readAggregateBatteryMetrics(uint8_t configuredBatteryCount, AggregateBatteryMetrics *metrics);
void updateIndividualBatteryMetrics(uint8_t batteryId);
void updateBatteryStaticInfo(uint8_t batteryId);
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
void printIndividualBatteryData(uint8_t batteryId);
#endif