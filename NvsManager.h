#ifndef NVS_MANAGER_H
#define NVS_MANAGER_H

#include <Preferences.h>

extern Preferences preferences;  // Déclare une instance de l'objet Preferences

void initNvs();                    // Initialise la NVS au démarrage
void saveBatteryCount(int count);  // Sauvegarde le nombre de batteries configurées
int loadBatteryCount();            // Charge le nombre de batteries depuis la NVS

#endif