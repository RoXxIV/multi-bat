#ifndef CANBUS_MANAGER_H
#define CANBUS_MANAGER_H

#include <Arduino.h>
#include <ESP32-TWAI-CAN.hpp>
#include "Config.h"
#include "ModbusManager.h"

// ——————— CONFIGURATION MATÉRIELLE ———————
// Pins du contrôleur CAN (à déplacer dans Config.h si validé)
#ifndef CAN_TX_PIN
#define CAN_TX_PIN 5 // Pin de transmission CAN
#endif

#ifndef CAN_RX_PIN
#define CAN_RX_PIN 4 // Pin de reception CAN
#endif

#ifndef CAN_SPEED_KBPS
#define CAN_SPEED_KBPS 500 // Vitesse de transmission CAN en kbps
#endif

// ——————— PROTOCOLE PYLONTECH/BYD ———————
// IDs des trames CAN selon protocole Pylontech/BYD (utilisé actuellement)
#define CAN_ID_LIMITS 0x351          // Limites Charge/Décharge
#define CAN_ID_SOC_SOH 0x355         // SOC/SOH
#define CAN_ID_VOLTAGE_CURRENT 0x356 // Tensions/Courants
#define CAN_ID_ALARMS 0x359          // Status Protections/Alarmes
#define CAN_ID_REQUESTS 0x35C        // Requêtes et indicateurs système

// ——————— PARAMÈTRES TEMPORELS ———————
#define CAN_SEND_INTERVAL_MS 1000 // Intervalle d'envoi des trames CAN en millisecondes
// Limites des consignes de courant (adaptées pour 4 batteries max)
#define MAX_CHARGE_CURRENT_A 600    // Courant de charge maximum (A)
#define MAX_DISCHARGE_CURRENT_A 600 // Courant de décharge maximum (A)

// ——————— ÉNUMÉRATIONS ———————
enum CanProtocol
{
    PROTOCOL_PYLONTECH = 0, // Protocole Pylontech/BYD (actuel)
    PROTOCOL_SOLIS = 1      // Protocole Solis (test) vvv
};

// ——————— VARIABLES GLOBALES ———————
extern CanFrame canFrame;         // Structure de trame CAN pour envoi
extern unsigned long lastCanSend; // Timestamp du dernier envoi CAN
// Consignes dynamiques de courant (modifiables en temps réel)
extern float chargeCurrentSetpoint;    // Consigne charge (0-600A)
extern float dischargeCurrentSetpoint; // Consigne décharge (0-600A)
// Variables pour affichage temps réel des trames
extern char lastCanFrames[5][50]; // Stockage texte des trames
extern bool canDisplayActive;     // État affichage temps réel

// ——————— FONCTIONS D'INITIALISATION ———————
bool initCanBus(); // Initialise le bus CAN avec la configuration matérielle

// ——————— FONCTIONS DE CONTRÔLE DES CONSIGNES ———————
// Définit la consigne de courant de charge (avec limitation 0-600A)
void setChargeCurrentSetpoint(float currentA);
// Définit la consigne de courant de décharge (avec limitation 0-600A)
void setDischargeCurrentSetpoint(float currentA);
float getChargeCurrentSetpoint();    // Retourne la consigne actuelle de charge
float getDischargeCurrentSetpoint(); // Retourne la consigne actuelle de décharge

// ——————— FONCTIONS D'ENVOI PRINCIPAL ———————
void sendCanData();   // Envoie toutes les trames CAN vers l'onduleur (fonction principale)
bool shouldSendCan(); // Vérifie s'il est temps d'envoyer les trames CAN

// ——————— FONCTIONS D'ENVOI SPÉCIFIQUE PAR TRAME ———————
void sendChargeLimits();       // Envoie la trame 0x351 (limites de charge/décharge avec consignes)
void sendSocSoh();             // Envoie la trame 0x355 (SOC et SOH globaux du système)
void sendVoltageCurrentTemp(); // Envoie la trame 0x356 (tension, courant et température globaux)
void sendAlarms();             // Envoie la trame 0x359 (états des protections et alarmes)
void sendRequests();           // Envoie la trame 0x35C (requêtes système et indicateurs)

// ——————— FONCTIONS D'AFFICHAGE TEMPS RÉEL ———————
void updateCanFrameDisplay();          // Met à jour le texte d'affichage des trames avec valeurs actuelles
void showCanFrames();                  // Affiche les trames CAN sur l'écran OLED (pour debug/monitoring)
void setCanDisplayActive(bool active); // Active/désactive l'affichage temps réel des trames

// ——————— ACCÈS AUX VARIABLES EXTERNES NÉCESSAIRES ———————
// Ces déclarations permettent aux fonctions CAN d'accéder aux données système
extern BatteryState batteryStates[];                     // États des batteries individuelles
extern SystemDiagnostics systemDiag;                     // Diagnostic système global
extern IndividualBatteryData individualBatteryMetrics[]; // Données détaillées batteries
extern AggregateBatteryMetrics latestMetrics;            // Métriques consolidées
extern bool degradedMode;                                // État mode dégradé
extern int configuredBatteryCount;                       // Nombre de batteries configurées
extern int respondingBatteryCount;                       // Nombre de batteries répondantes

#endif