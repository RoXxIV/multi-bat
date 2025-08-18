#ifndef CANBUS_MANAGER_H
#define CANBUS_MANAGER_H

#include <Arduino.h>
#include <ESP32-TWAI-CAN.hpp>
#include "Config.h"

// ——————— CONFIGURATION CAN (a ajouter dans Config.h si valide) ———————
#ifndef CAN_TX_PIN
#define CAN_TX_PIN 5
#endif

#ifndef CAN_RX_PIN
#define CAN_RX_PIN 4
#endif

#ifndef CAN_SPEED_KBPS
#define CAN_SPEED_KBPS 500
#endif

// ——————— PROTOCOLES SUPPORTÉS ———————
enum CanProtocol
{
    PROTOCOL_PYLONTECH = 0,
    PROTOCOL_SOLIS = 1
};

// IDs des trames CAN selon protocole
// Pylontech/BYD
#define PYLON_ID_LIMITS 0x351
#define PYLON_ID_SOC_SOH 0x355
#define PYLON_ID_VOLTAGE_CURRENT 0x356

// Solis
#define SOLIS_ID_LIMITS 0x320
#define SOLIS_ID_SOC_SOH 0x323
#define SOLIS_ID_VOLTAGE_CURRENT 0x325

// IDs des trames CAN selon protocole Pylontech/BYD
#define CAN_ID_LIMITS 0x351          // Limites Charge/Décharge
#define CAN_ID_SOC_SOH 0x355         // SOC/SOH
#define CAN_ID_VOLTAGE_CURRENT 0x356 // Tensions/Courants
#define CAN_ID_ALARMS 0x359          // Status Protections/Alarmes
#define CAN_ID_REQUESTS 0x35C        // Indicateurs de Requête
#define CAN_ID_MANUFACTURER 0x35E    // Nom du Fabricant

// Timing d'envoi
#define CAN_SEND_INTERVAL_MS 1000

// Limites pour consignes variables
#define MAX_CHARGE_CURRENT_A 600 // 0 à 600A pour 4 batteries
#define MAX_DISCHARGE_CURRENT_A 600

// ——————— VARIABLES GLOBALES ———————
extern CanFrame canFrame;
extern unsigned long lastCanSend;

// Variables pour consignes dynamiques
extern float chargeCurrentSetpoint;    // 0 à 600A
extern float dischargeCurrentSetpoint; // 0 à 600A

// Variables pour affichage des trames
extern char lastCanFrames[5][50]; // Stockage des 5 dernières trames
extern bool canDisplayActive;

// ——————— FONCTIONS PUBLIQUES ———————

// Initialisation
bool initCanBus();

// Contrôle des consignes
void setChargeCurrentSetpoint(float currentA);    // 0-600A
void setDischargeCurrentSetpoint(float currentA); // 0-600A
float getChargeCurrentSetpoint();
float getDischargeCurrentSetpoint();

// Envoi des données
void sendCanData();
bool shouldSendCan();

// Envoi de trames spécifiques
void sendChargeLimits();
void sendSocSoh();
void sendVoltageCurrentTemp();
void sendAlarms();
void sendRequests();

// Fonctions d'affichage des trames
void updateCanFrameDisplay();
void showCanFrames();
void setCanDisplayActive(bool active);

#endif