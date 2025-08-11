#ifndef MENU_MANAGER_H
#define MENU_MANAGER_H

#include <Arduino.h>
#include <string.h>
#include "DisplayManager.h"
#include "Config.h"

// Écrans gérés
enum ScreenState
{
    SCREEN_MAIN_MENU = 0,
    SCREEN_CODE_INPUT,
    SCREEN_CODE_RESULT
};

// Élément de menu (base ou admin)
struct MenuItem
{
    const char *text;
    bool enabled;
    bool isAdminItem;
};

class MenuManager
{
public:
    explicit MenuManager(DisplayManager *displayMgr);

    void begin();

    // Navigation déclenchée par les boutons
    void navigateUp();
    void navigateDown();
    void selectCurrentItem();
    void goBack();

    // Rafraîchit l’affichage selon l’état courant
    void updateDisplay();

    // Getters
    ScreenState getCurrentState() const { return currentState; }
    bool isAdminAuthenticated() const { return adminAuthenticated; }
    int getTotalMenuItems() const { return totalMenuItems; }

    // Gestion du mode admin
    void activateAdmin();
    void deactivateAdmin();

    // Actions liées aux items (stubs)
    static void actionDisplayIds();
    static void actionPairing();
    static void actionErrors();
    static void actionIndividual();
    static void actionAdminCode();
    static void actionAdminStuff();
    static void actionSystemSettings();

private:
    DisplayManager *display;

    // État UI
    ScreenState currentState;
    int selectedMenuItem;
    int totalMenuItems;
    bool adminAuthenticated;

    // Saisie code admin
    int codeDigits[3];
    int currentDigit;

    // Résultat code
    unsigned long resultTimer;
    bool codeSuccess;

    // Menu courant
    MenuItem menuItems[MAX_MENU_ITEMS];

    // Construction / rendu
    void buildMenuItems(); // (re)construit le menu selon admin on/off
    void displayMainMenu();
    void displayCodeInput();
    void displayCodeResult();

    // Code admin
    void resetCodeInput();
    void checkCode();

    // Exécution d’un item (routing simple par texte)
    void executeMenuAction(int itemIndex);

    // Fenêtre de défilement
    static const int VISIBLE_COUNT = 4;
    int viewTop = 0; // index du premier item affiché (0-based)

    // Maintient la sélection dans la fenêtre visible (dir = +1 down, -1 up)
    void adjustViewAfterMove(int dir);
};

#endif
