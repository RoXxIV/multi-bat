#ifndef BUTTON_MANAGER_H
#define BUTTON_MANAGER_H

#include "Config.h"

// ——————— VARIABLES GLOBALES BOUTONS ———————
extern ButtonState buttons[BTN_COUNT];
extern unsigned long debounceDelay;

// ——————— FONCTIONS PUBLIQUES ———————

// Initialisation
void initButtons(int upPin, int downPin, int okPin, int backPin);
void setDebounceDelay(unsigned long delay);

// Mise à jour (à appeler dans loop)
void updateButtons();
void updateSingleButton(int buttonIndex);

// Test d'appui simple (front montant)
bool isButtonPressed(int buttonIndex);
bool isUpPressed();
bool isDownPressed();
bool isOkPressed();
bool isBackPressed();

// Utilitaires
bool anyButtonPressed();
void resetAllButtons();

// Debug
void printButtonStates();
const char *getButtonName(int buttonIndex);

#endif