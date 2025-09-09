#ifndef BATTERY_LOGIC_H
#define BATTERY_LOGIC_H

#include "Config.h"
#include "ModbusManager.h"

// ——————— VARIABLES GLOBALES ———————
extern BatteryState batteryStates[MAX_BATTERIES];
extern SystemDiagnostics systemDiag;
extern float currentChargeSetpoint;
extern float currentDischargeSetpoint;
extern int activeBatteryCount;
extern int respondingBatteryCount;
extern bool degradedMode;

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

/**
 * @brief Affiche l'état complet du système pour debug
 */
void printSystemStatus();

/**
 * @brief Met à jour les métriques globales du système
 * À appeler après chaque lecture des données batteries
 */
void updateSystemMetrics();

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

/**
 * @brief Met à jour l'état d'une batterie individuelle
 * @param batteryIndex Index de la batterie (0-based)
 * @param isResponding true si la batterie répond
 */
void updateBatteryState(int batteryIndex, bool isResponding);

// ——————— DIAGNOSTIC ET DÉTECTION DES PANNES ———————
/**
 * @brief Vérifie l'état des MOSFETs de toutes les batteries
 * @return true si au moins un MOSFET a un problème
 */
bool checkMosfetStatus();

/**
 * @brief Vérifie si une batterie peut récupérer d'un problème MOSFET
 * @param batteryIndex Index de la batterie à vérifier
 * @return true si la récupération est possible
 */
bool checkMosfetRecoveryConditions(int batteryIndex);

/**
 * @brief Vérifie les conditions d'activation du mode dégradé
 * @return true si le mode dégradé doit être activé
 */
bool checkDegradedModeConditions();

/**
 * @brief Vérifie si le système peut sortir du mode dégradé
 * @return true si sortie possible
 */
bool checkCanExitDegradedMode();

// ——————— GESTION MODE DÉGRADÉ ———————
/**
 * @brief Active le mode dégradé avec les limitations associées
 * @param reason Raison de l'activation (pour logs)
 */
void enableDegradedMode(const char *reason);

/**
 * @brief Désactive le mode dégradé si les conditions le permettent
 */
void disableDegradedMode();

// ——————— GESTION DES BATTERIES ACTIVES ———————
/**
 * @brief Gère l'activation/désactivation des batteries selon les deltas de tension
 */
void manageBatteryActivation();

/**
 * @brief Vérifie si une batterie peut être activée (conditions complémentaires)
 * @param batteryIndex Index de la batterie à vérifier
 * @return true si activation possible
 */
bool checkBatteryCanBeActivated(int batteryIndex);

/**
 * @brief Met à jour les deltas de tension par rapport à la batterie la plus haute
 */
void updateVoltageDeltas();

/**
 * @brief Applique les limitations de courant aux batteries si nécessaire
 */
void manageBatteryLimitations();

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

// ——————— MOYENNES ET AGRÉGATION ———————
/**
 * @brief Calcule les moyennes SOC, SOH, tension et températures
 * Met à jour la structure latestMetrics
 */
void calculateAverages();

/**
 * @brief Calcule les alarmes système pour les trames CAN
 * @return Byte d'alarmes selon l'état du système
 */
uint8_t calculateSystemAlarms();

/**
 * @brief Calcule les protections système pour les trames CAN
 * @return Byte de protections selon l'état du système
 */
uint8_t calculateSystemProtections();

// ——————— FONCTIONS UTILITAIRES ———————
/**
 * @brief Trouve la batterie avec la tension la plus élevée
 * @return Index de la batterie (-1 si aucune trouvée)
 */
int findHighestVoltageBattery();

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

/**
 * @brief Vérifie la source de réveil des batteries (bouton physique)
 */
void checkWakeUpSource();

/**
 * @brief Vérifie les codes défauts internes rapportés par le BMS
 */
void checkBmsFaults();
#endif