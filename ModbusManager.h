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
    char serialNumber[33]; // Pour stocker le S/N (16 registres * 2 chars/registre + 1 nul)
    float voltage;
    float current;
    float soc;
    // Le SOH n'est pas dans les registres standards que nous avons identifiés.
    bool chargeMosfetStatus;
    bool dischargeMosfetStatus;
    float temp1;
    float temp2;
    float mosTemp;
    uint16_t heartbeat;
    float currentLimit;
    float cellVoltages[16]; // Conçu pour 16 cellules, à ajuster si besoin.
    float cellVoltageDifference;
    // Les autres données (volt diff, S/N, etc.) ne sont pas dans les registres standards.
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