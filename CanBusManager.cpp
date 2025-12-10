#include "CanBusManager.h"
#include "ModbusManager.h"
#include "BatteryLogic.h"

// ——————— VARIABLES GLOBALES ———————
CanFrame canFrame;

// ——————— FONCTIONS D'INITIALISATION ———————
bool initCanBus()
{
  // Configuration des pins et paramètres
  ESP32Can.setPins(CAN_TX_PIN, CAN_RX_PIN);
  ESP32Can.setRxQueueSize(5);
  ESP32Can.setTxQueueSize(5);
  ESP32Can.setSpeed(ESP32Can.convertSpeed(CAN_SPEED_KBPS));
  // Démarrage du CAN
  if (!ESP32Can.begin())
  {
    return false;
  }

  return true;
}

// ——————— ENVOI DES TRAMES CAN ———————
void sendCanData()
{
  // Envoyer toutes les trames
  sendChargeLimits();       // 0x351
  sendSocSoh();             // 0x355
  sendVoltageCurrentTemp(); // 0x356
  sendAlarms();             // 0x359
  sendRequests();           // 0x35C
}

void sendChargeLimits()
{
  canFrame = {0};
  canFrame.identifier = CAN_ID_LIMITS;
  canFrame.extd = 0;
  canFrame.data_length_code = 8;

  // Tension de charge max: 51.6V = 516 = 0x0204
  uint16_t vchg = 516;

  // Courants en 0.1A (consignes variables 0-600A)
  uint16_t ichg = (uint16_t)(currentChargeSetpoint * 10);
  uint16_t idis = (uint16_t)(currentDischargeSetpoint * 10);

  // Format little-endian selon la doc
  canFrame.data[0] = lowByte(vchg);  // 0x04
  canFrame.data[1] = highByte(vchg); // 0x02
  canFrame.data[2] = lowByte(ichg);  // Variable selon consigne
  canFrame.data[3] = highByte(ichg); // Variable selon consigne
  canFrame.data[4] = lowByte(idis);  // Variable selon consigne
  canFrame.data[5] = highByte(idis); // Variable selon consigne
  canFrame.data[6] = 0xC9;           // Fixe
  canFrame.data[7] = 0x01;           // Fixe

  ESP32Can.writeFrame(canFrame);
}

void sendSocSoh()
{
  canFrame = {0};
  canFrame.identifier = CAN_ID_SOC_SOH;
  canFrame.extd = 0;
  canFrame.data_length_code = 8;

  // SOC DYNAMIQUE: Utilise la moyenne calculée des batteries
  extern AggregateBatteryMetrics latestMetrics;
  uint16_t soc = 0;

  if (latestMetrics.isDataValid)
  {
    // Conversion du SOC moyen vers entier avec arrondi
    soc = (uint16_t)latestMetrics.averageSoc;

    // Sécurité: limiter entre 0 et 100%
    if (soc > 100)
      soc = 100;
  }
  else
  {
    // Valeur par défaut si pas de données valides
    soc = 0;
  }
  uint16_t soh = 100; // 100%

  // Format little-endian selon la doc
  canFrame.data[0] = lowByte(soc);  // 0x14
  canFrame.data[1] = highByte(soc); // 0x00
  canFrame.data[2] = lowByte(soh);  // 0x64
  canFrame.data[3] = highByte(soh); // 0x00
  canFrame.data[4] = 0xD0;          // Fixe
  canFrame.data[5] = 0x07;          // Fixe
  canFrame.data[6] = 0x00;          // Fixe
  canFrame.data[7] = 0x00;          // Fixe

  ESP32Can.writeFrame(canFrame);
}

void sendVoltageCurrentTemp()
{
  canFrame = {0};
  canFrame.identifier = CAN_ID_VOLTAGE_CURRENT;
  canFrame.extd = 0;
  canFrame.data_length_code = 8;

  uint16_t voltage = 4200; // 42.00V en 0.01V
  uint16_t current = 0;    // Valeur par défaut 0A, sans offset 10.0A est un envoi de 100
  uint16_t temp = 270;     // 27.0°C en 0.1°C

  extern AggregateBatteryMetrics latestMetrics;

  if (latestMetrics.isDataValid)
  {
    // Tension: Conversion V vers 0.01V (ex: 48.5V → 4850)
    voltage = (uint16_t)(latestMetrics.averageVoltage * 100);

    // Courant: Conversion A vers 0.1A SANS OFFSET.
    // La valeur est un entier signé sur 16 bits (négatif = décharge).
    // int16_t signed_current = (int16_t)(latestMetrics.totalCurrent * 10);
    // current = (uint16_t)signed_current; // Cast en non-signé pour l'envoi
    current = (int16_t)(latestMetrics.totalCurrent * 10);
    // Température: Conversion °C vers 0.1°C (ex: 35.2°C → 352)
    temp = (uint16_t)(latestMetrics.averageTemp * 10);

    // Sécurités pour éviter les valeurs aberrantes
    if (voltage > 6000)
      voltage = 6000; // Max 60V
    if (temp > 1000)
      temp = 1000; // Max 100°C
  }

  // Format little-endian selon la doc
  canFrame.data[0] = lowByte(voltage);  // 0x68
  canFrame.data[1] = highByte(voltage); // 0x10
  canFrame.data[2] = lowByte(current);  // 0x00
  canFrame.data[3] = highByte(current); // 0x00
  canFrame.data[4] = lowByte(temp);     // 0x0E
  canFrame.data[5] = highByte(temp);    // 0x01
  canFrame.data[6] = 0x00;              // Fixe
  canFrame.data[7] = 0x00;              // Fixe

  ESP32Can.writeFrame(canFrame);
}

void sendAlarms()
{
  canFrame = {0};
  canFrame.identifier = CAN_ID_ALARMS;
  canFrame.extd = 0;
  canFrame.data_length_code = 8;

  // On ne calcule plus les alarmes, on force à 0
  uint8_t protections = 0x00;
  uint8_t alarms = 0x00;

  // Nombre de modules réel
  uint8_t moduleCount = (configuredBatteryCount > 0) ? configuredBatteryCount : 1;

  canFrame.data[0] = protections; // Sera toujours 0x00
  canFrame.data[1] = 0x00;        // Réservé
  canFrame.data[2] = alarms;      // Sera toujours 0x00
  canFrame.data[3] = 0x00;        // Réservé
  canFrame.data[4] = moduleCount; // Nombre réel de batteries
  canFrame.data[5] = 0x00;        // Signature
  canFrame.data[6] = 0x00;        // Signature
  canFrame.data[7] = 0x00;        // Réservé

  ESP32Can.writeFrame(canFrame);
}

void sendRequests()
{
  // Données Brutes : C0 00 00 00 00 00 00 00
  canFrame = {0};
  canFrame.identifier = CAN_ID_REQUESTS;
  canFrame.extd = 0;
  canFrame.data_length_code = 8;

  canFrame.data[0] = 0xC0; // Bits 7+6: Charge+Discharge enable
  canFrame.data[1] = 0x00; // Réservé
  canFrame.data[2] = 0x00; // Réservé
  canFrame.data[3] = 0x00; // Réservé
  canFrame.data[4] = 0x00; // Réservé
  canFrame.data[5] = 0x00; // Réservé
  canFrame.data[6] = 0x00; // Réservé
  canFrame.data[7] = 0x00; // Réservé

  ESP32Can.writeFrame(canFrame);
}

void sendSafeLimitsZero()
{
  canFrame = {0};
  canFrame.identifier = CAN_ID_LIMITS;
  canFrame.extd = 0;
  canFrame.data_length_code = 8;

  // Tension de charge max: 51.6V = 516 = 0x0204
  uint16_t vchg = 516;

  // Consignes à 0A (0.1A * 10 = 0)
  uint16_t ichg = 0;
  uint16_t idis = 0;

  // Format little-endian
  canFrame.data[0] = lowByte(vchg);  // 0x04
  canFrame.data[1] = highByte(vchg); // 0x02
  canFrame.data[2] = lowByte(ichg);  // 0x00
  canFrame.data[3] = highByte(ichg); // 0x00
  canFrame.data[4] = lowByte(idis);  // 0x00
  canFrame.data[5] = highByte(idis); // 0x00
  canFrame.data[6] = 0xC9;           // Fixe
  canFrame.data[7] = 0x01;           // Fixe

  ESP32Can.writeFrame(canFrame);
}
