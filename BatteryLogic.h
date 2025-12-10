#ifndef BATTERY_LOGIC_H
#define BATTERY_LOGIC_H

#include "Config.h"
#include "ModbusManager.h"

// ——————— VARIABLES GLOBALES ———————
extern BatteryState batteryStates[MAX_BATTERIES];
extern SystemDiagnostics systemDiag;
extern float currentChargeSetpoint;
extern float currentDischargeSetpoint;
extern int respondingBatteryCount;
extern bool degradedMode;
extern bool systemPowerButtonOn;
extern bool globalChargeMosfetOk;
extern bool globalDischargeMosfetOk;
extern int turnedOffBatteryId;

// ——————— INITIALISATION ET SYSTÈME ———————
/**
 * @brief Initialise toutes les structures de gestion des batteries
 * À appeler dans setup() après les initialisations existantes
 */
void initBatteryManagement();

/**
 * @brief Fonction principale de la logique de gestion.
 * Orchestre l'analyse, la gestion des limitations, l'activation des batteries
 * et le calcul des consignes.
 */
void runBatteryManagementCycle();

// ——————— GESTION DES CONNEXIONS ET SURVEILLANCE ———————
/**
 * @brief Surveille en continu les connexions des batteries
 * À appeler dans loop() principal
 */
void monitorBatteryConnections();

/**
 * @brief Vérifie quelles batteries répondent aux commandes Modbus
 * @return Nombre de batteries répondantes
 */
int checkBatteryConnections();

// ——————— DIAGNOSTIC ET DÉTECTION DES PANNES ———————
/**
 * @brief Met à jour les métriques globales du système
 * À appeler après chaque lecture des données batteries
 */
void updateSystemMetrics();
/**
 * @brief Fonction unifiée qui remplace checkMosfetStatus, checkWakeUpSource,
 * manageBatteryLimitations et calculateAverages pour optimiser les performances.
 */
void processBatteriesGlobalState();

/**
 * @brief Vérifie si une batterie peut récupérer d'un problème MOSFET
 * @param batteryIndex Index de la batterie à vérifier
 * @return true si la récupération est possible
 */
bool checkMosfetRecoveryConditions(int batteryIndex);

// ——————— CALCUL DES CONSIGNES ———————
/**
 * @brief Calcule la consigne de charge selon les conditions actuelles
 * @return Consigne de charge en Ampères
 */
float calculateChargeSetpoint();

/**
 * @brief Calcule la consigne de décharge selon les conditions actuelles
 * @return Consigne de décharge en Ampères
 */
float calculateDischargeSetpoint();

// ——————— GESTION DES LEDS D'ÉTAT ———————
/**
 * @brief Initialise les LEDs d'état du système
 */
void initStatusLeds();

/**
 * @brief Met à jour l'état des LEDs selon les conditions système
 */
void updateStatusLeds();

// ——————— GESTION DES ERREURS SYSTÈME ———————
/**
 * @brief Initialise le système de gestion des erreurs
 */
void initErrorSystem();

/**
 * @brief Ajoute ou met à jour une erreur système
 * @param type Type d'erreur
 * @param batteryId ID de la batterie (-1 si erreur globale)
 * @param description Description de l'erreur
 */
void addSystemError(ErrorType type, int batteryId, const char *description);

/**
 * @brief Supprime une erreur spécifique
 * @param type Type d'erreur
 * @param batteryId ID de la batterie
 */
void removeSystemError(ErrorType type, int batteryId);

#endif