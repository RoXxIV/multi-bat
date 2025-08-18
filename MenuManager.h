#ifndef MENU_MANAGER_H
#define MENU_MANAGER_H

#include "Config.h"
#include "DisplayManager.h"
#include "ModbusManager.h"

// ——————— VARIABLES GLOBALES MENU ———————
extern int currentScreen;
extern int selectedMenuItem;
extern int totalMenuItems;
extern int menuViewTop;
extern bool adminMode;

// Variables pour le code admin
extern int codeDigits[3];
extern int currentDigit;
extern unsigned long resultTimer;
extern bool codeSuccess;

// Menu items
extern MenuItem menuItems[MAX_MENU_ITEMS];

// ——————— FONCTIONS PUBLIQUES ———————

// Initialisation
void initMenu();
void buildMenu();

// Navigation (appelées par les boutons)
void navigateMenuUp();
void navigateMenuDown();
void selectMenuItem();
void goBackMenu();

// Affichage (selon l'écran courant)
void updateMenuDisplay();

// Écrans spécifiques
void showMainDataScreen();
void showMenuScreen();
void showCodeInputScreen();
void showCodeResultScreen();
void showCanFramesScreen();

// Gestion du mode admin
void activateAdminMode();
void deactivateAdminMode();
bool isAdminMode();

// Gestion du code admin
void resetCodeInput();
void checkAdminCode();

// Actions des items de menu (à implémenter selon besoins)
void actionDisplayIds();
void actionShowErrors();
void actionIndividualBatteries();
void actionPairing();
void actionSystemSettings();
void actionShowCanFrames();

// Utilitaires internes
void adjustMenuView();
void executeMenuAction(int itemIndex);

// Getters
int getCurrentScreen();
int getTotalMenuItems();

#endif