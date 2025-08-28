#ifndef MENU_MANAGER_H
#define MENU_MANAGER_H

#include "Config.h"
#include "DisplayManager.h"
#include "ModbusManager.h"

// ——————— ENUMERATIONS ———————
// Choix disponibles après configuration d'une batterie
enum PairingChoice
{
    CHOICE_ADD_BATTERY,
    CHOICE_FINISH
};

// ——————— VARIABLES GLOBALES ———————
// Paramètres de l'affichage
extern int brightnessLevel;              // Niveau de luminosité (1 à 5)
extern const uint8_t brightnessValues[]; // Tableau de valeurs de luminosité
extern uint8_t currentBrightness;        // Stocke la valeur de luminosité actuelle
// État du menu et navigation
extern int currentScreen;        // État actuel de l'interface (écran principal, menu, saisie code, etc.)
extern int selectedMenuItem;     // Index de l'item de menu actuellement sélectionné
extern int totalMenuItems;       // Nombre total d'items dans le menu (change selon mode admin)
extern int menuViewTop;          // Index du premier item visible dans le menu
extern bool adminMode;           // État du mode administrateur (débloque items avancés)
extern int selectedBatteryIndex; // Index dans la liste des batteries (0, 1, 2...)
extern int selectedBatteryId;    // ID de la batterie choisie (2, 3, 4...)
// Menu items
extern MenuItem menuItems[MAX_MENU_ITEMS]; // Tableau des items de menu avec texte et actions
// Variables pour le code admin
extern int codeDigits[3];         // Tableau pour stocker les chiffres du code
extern int currentDigit;          // Index du chiffre actuellement saisie
extern unsigned long resultTimer; // Temporisateur pour afficher le résultat de la saisie
extern bool codeSuccess;          // Indicateur si le code est correct

// ——————— FONCTIONS D'INITIALISATION ———————
void initMenu();  // Initialise le système de menu au démarrage
void buildMenu(); // Construit la liste des items selon le mode (user/admin)

// ——————— FONCTIONS DE NAVIGATION ———————
void navigateMenuUp();   // Déplace la sélection vers le haut (bouton UP)
void navigateMenuDown(); // Déplace la sélection vers le bas (bouton DOWN)
void selectMenuItem();   // Exécuter l'action de l'item de menu actuellement sélectionné
void goBackMenu();       // Retour vers le menu principal

// ——————— FONCTIONS D'AFFICHAGE ———————
// Affichage principal
void updateMenuDisplay(); // Met à jour l'affichage selon l'écran courant (appelée dans loop)
// Écrans spécifiques
void showMainDataScreen();      // Affiche l'écran principal avec données batteries temps réel
void showMenuScreen();          // Affiche le menu principal avec navigation
void showCodeInputScreen();     // Affiche l'écran de saisie du code admin
void showCodeResultScreen();    // Affiche le résultat de validation du code admin
void showCanFramesScreen();     // Affiche les trames CAN en temps réel
void showBrightnessScreen();    // Affiche l'écran de choix de luminosité
void showBatteryListScreen();   // Affiche la liste des batteries configurées
void showBatteryDetailScreen(); // Affiche les détails de la batterie sélectionnée

// ——————— GESTION MODE ADMIN ———————
void activateAdminMode();   // Active le mode admin (ajoute items avancés au menu)
void deactivateAdminMode(); // Désactive le mode admin (retour mode utilisateur)
bool isAdminMode();         // Retourne l'état du mode admin
// Gestion du code admin
void resetCodeInput(); // Remet à zéro la saisie du code admin
void checkAdminCode(); // Vérifie si le code saisi est correct et gère l'activation du mode admin

// ——————— ACTIONS DES MENUS ———————
void actionDisplayIds();          // Envoie H=7 à toutes les batteries pour affichage ID
void actionShowErrors();          // Affiche les erreurs système (à implémenter)
void actionIndividualBatteries(); // Affiche les données individuelles par batterie (à implémenter)
void actionSystemSettings();      // Configuration système avancée (mode admin uniquement)
void actionShowCanFrames();       // Active l'affichage temps réel des trames CAN

// ——————— APPAIRAGE DES BATTERIES ———————
void actionPairing();                              // Lance le processus complet d'appairage des batteries
void sequentialPairingWithMenu();                  // Gère l'appairage séquentiel avec menus de choix
bool configureBattery(int newId);                  // Configure une batterie (change ID de 1 vers newId et envoie H=7)
void finalizePairing(int batteriesConfigured);     // Finalise l'appairage des batteries (change H=7 vers H=4)
PairingChoice showPairingMenu(int batteriesCount); // Affiche le menu de choix après configuration réussie
PairingChoice showErrorMenu();                     // Affiche le menu en cas d'erreur de configuration
// Affiche un message et attend confirmation OK de l'utilisateur
void waitForUserConfirmation(const char *title, const char *line1, const char *line2 = nullptr, const char *line3 = nullptr);

// ——————— FONCTIONS UTILITAIRES ———————
void adjustMenuView();                 // Ajuste la fenêtre de visualisation du menu (scroll automatique)
void executeMenuAction(int itemIndex); // Exécute l'action associée à un item de menu

#endif
