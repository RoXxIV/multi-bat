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
  float soh; // valeur fixee a 100%
  float ratedCapacity;
  float chargeCurrentLimitL1;
  float dischargeCurrentLimitL1;
  uint16_t cellCount;
  float maxCellVoltage;        // en V
  float minCellVoltage;        // en V
  float cellVoltageDifference; // en V
  float maxCellTemp;           // en °C
  float minCellTemp;           // en °C
  bool chargeMosfetStatus;
  bool dischargeMosfetStatus;
  uint16_t wakeUpSource;
  uint16_t faultCode0_1;
  uint16_t faultCode2_3;
  uint16_t overVoltL2Threshold_mV;  // Seuil de Surtension L2 (lu depuis 0x0132)
  uint16_t underVoltL2Threshold_mV; // Seuil de Sous-tension L2 (lu depuis 0x0136)
};
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
#define BAUD_RATE 9600   // Vitesse de communication série
#define MASTER_ADDR 0x81 // Adresse de l'ESP32 en tant que maître

// --- FONCTIONS D'INITIALISATION ---
void initModbus();
void startModbus();
void stopModbus();

// --- NOUVELLE FONCTION DE LECTURE PAS-À-PAS ---
/**
 * @brief Lit la prochaine valeur Modbus pour une batterie donnée, selon l'étape (readStep).
 * @param batteryId L'ID de la batterie (ex: 2, 3...).
 * @param readStep L'étape de lecture (0 à 13). Cette variable est incrémentée par la fonction si la lecture réussit.
 * @return true si la lecture a réussi, false en cas d'échec (timeout/CRC).
 */
bool processNextModbusRead(uint8_t batteryId, int &readStep);

// --- LECTURE INFOS STATIQUES ---
void updateBatteryStaticInfo(uint8_t batteryId);

// --- FONCTIONS D'ÉCRITURE (pour l'appairage) ---
bool sendDisplayIdToBattery(uint8_t batteryId, uint8_t asciiValue);
bool changeBatteryIdTo1(uint8_t batteryId);
bool changeAllBatteriesToId1();
bool changeBatteryIdFrom1To(uint8_t newId);
#endif