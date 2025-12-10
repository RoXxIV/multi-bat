#ifndef CANBUS_MANAGER_H
#define CANBUS_MANAGER_H

#include <Arduino.h>
#include <ESP32-TWAI-CAN.hpp>
#include "Config.h"
#include "ModbusManager.h"
#include "BatteryLogic.h"

// ——————— CONFIGURATION MATÉRIELLE ———————
// Pins du contrôleur CAN
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

// ——————— PARAMÈTRES DE COMMUNICATION ———————
#define CAN_SEND_INTERVAL_MS 1000   // Intervalle d'envoi des trames CAN en millisecondes
#define MAX_CHARGE_CURRENT_A 600    // Courant de charge maximum (A)
#define MAX_DISCHARGE_CURRENT_A 600 // Courant de décharge maximum (A)

// ——————— VARIABLES GLOBALES ———————
extern CanFrame canFrame;         // Structure de trame CAN pour envoi
extern unsigned long lastCanSend; // Timestamp du dernier envoi CAN

// ——————— FONCTIONS D'INITIALISATION ———————

/**
 * @brief Initialise le bus CAN avec la configuration matérielle
 * Configure les pins, vitesse et démarre la communication
 * @return true si initialisation réussie, false sinon
 */
bool initCanBus();

// ——————— ENVOI DES TRAMES CAN ———————
/**
 * @brief Fonction principale d'envoi CAN
 * Envoie toutes les trames vers l'onduleur &
 * Met à jour l'affichage si actif
 */
void sendCanData();

/**
 * @brief Envoie la trame 0x351 - Limites de charge/décharge
 * Contient tension max (51.6V) et consignes de courant dynamiques
 */
void sendChargeLimits();

/**
 * @brief Envoie la trame 0x355 - État de charge et santé
 * Contient SOC moyen calculé et SOH fixe à 100%
 */
void sendSocSoh();

/**
 * @brief Envoie la trame 0x356 - Mesures principales
 * Contient tension moyenne, courant total et température moyenne
 */
void sendVoltageCurrentTemp();

/**
 * @brief Envoie la trame 0x359 - Protections et alarmes
 * Contient état des protections et alarmes calculées dynamiquement
 */
void sendAlarms();

/**
 * @brief Envoie la trame 0x35C - Requêtes système
 * Indique à l'onduleur que charge et décharge sont autorisées
 */
void sendRequests();

/**
 * @brief Envoie une trame 0x351 de limites de charge/décharge à 0A.
 * Utilisé au démarrage comme mesure de sécurité.
 */
void sendSafeLimitsZero();

#endif