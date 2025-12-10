#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <WiFi.h>
#include <ArduinoOTA.h>
#include "Config.h"

// ——————— VARIABLES GLOBALES OTA ———————
extern bool otaServerActive;
extern unsigned long otaStartTime;
extern bool otaInProgress;
extern int otaProgress;

// ——————— FONCTIONS OTA ———————
void initOTA();          // Initialise le système OTA
bool startOTAServer();   // Démarre le serveur OTA
void stopOTAServer();    // Arrête le serveur OTA
bool connectToWiFi();    // Se connecte au WiFi
void handleOTAProcess(); // Gère le processus OTA (à appeler dans loop)
String getOTAInfo();     // Retourne les infos OTA (IP, nom, etc.)

#endif