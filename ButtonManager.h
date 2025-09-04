#ifndef BUTTON_MANAGER_H
#define BUTTON_MANAGER_H

#include "Config.h"

// ——————— VARIABLES GLOBALES ———————
extern ButtonState buttons[BTN_COUNT]; // Tableau contenant l'état de tous les boutons du système
extern unsigned long debounceDelay;    // Délai en ms pour l'anti-rebond des boutons (par défaut 50ms)

// ——————— FONCTIONS D'INITIALISATION ———————

// Initialise les 4 boutons avec leurs pins respectives
void initButtons(int upPin, int downPin, int okPin, int backPin);
// Modifie le délai d'anti-rebond (utile pour ajuster selon le matériel)
void setDebounceDelay(unsigned long delay);

// ——————— FONCTIONS DE MISE À JOUR ———————
void updateButtons(); // Met à jour l'état de tous les boutons (à appeler dans loop() principal)

// ——————— FONCTIONS DE DÉTECTION D'APPUI ———————
bool isButtonPressed(int buttonIndex); // Retourne vrai si le bouton est appuyé
bool isUpPressed();                    // Retourne vrai si le bouton UP est appuyé
bool isDownPressed();                  // Retourne vrai si le bouton DOWN est appuyé
bool isOkPressed();                    // Retourne vrai si le bouton OK est appuyé
bool isBackPressed();                  // Retourne vrai si le bouton BACK est appuyé

#endif