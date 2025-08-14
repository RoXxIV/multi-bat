#include "MenuManager.h"

// ——————— VARIABLES GLOBALES ———————
int currentScreen = SCREEN_MAIN_DATA;
int selectedMenuItem = 0;
int totalMenuItems = 0;
int menuViewTop = 0;
bool adminMode = false;

// Code admin
int codeDigits[3] = {0, 0, 0};
int currentDigit = 0;
unsigned long resultTimer = 0;
bool codeSuccess = false;

// Menu items
MenuItem menuItems[MAX_MENU_ITEMS];

// ——————— FONCTIONS D'INITIALISATION ———————

void initMenu()
{
    buildMenu();
    resetCodeInput();
    menuViewTop = 0;
    Serial.printf("Menu initialisé - %d items\n", totalMenuItems);
}

void buildMenu()
{
    totalMenuItems = 0;

    // Items de base (utilisateur standard)
    menuItems[totalMenuItems++] = {"Afficher ID batteries", ACTION_DISPLAY_IDS, false};
    menuItems[totalMenuItems++] = {"Affichage erreurs", ACTION_ERRORS, false};
    menuItems[totalMenuItems++] = {"Batteries individuelles", ACTION_INDIVIDUAL, false};
    menuItems[totalMenuItems++] = {"Mode admin", ACTION_ADMIN_CODE, false};

    // Items admin uniquement
    if (adminMode)
    {
        menuItems[totalMenuItems++] = {"Effectuer appairage", ACTION_PAIRING, true};
        menuItems[totalMenuItems++] = {"Parametres systeme", ACTION_SYSTEM_SETTINGS, true};
    }

    // Garde-fou
    if (totalMenuItems == 0)
    {
        menuItems[0] = {"Aucun item", ACTION_DISPLAY_IDS, false};
        totalMenuItems = 1;
    }

    // Ajustements après reconstruction
    if (selectedMenuItem >= totalMenuItems)
    {
        selectedMenuItem = totalMenuItems - 1;
    }
    adjustMenuView();
}

// ——————— FONCTIONS DE NAVIGATION ———————

void navigateMenuUp()
{
    if (currentScreen == SCREEN_MENU)
    {
        selectedMenuItem = (selectedMenuItem - 1 + totalMenuItems) % totalMenuItems;
        adjustMenuView();
        Serial.printf("UP -> %d: %s\n", selectedMenuItem, menuItems[selectedMenuItem].text);
    }
    else if (currentScreen == SCREEN_CODE_INPUT)
    {
        codeDigits[currentDigit] = (codeDigits[currentDigit] + 1) % 10;
    }
    // MAIN_DATA : pas de navigation up/down
}

void navigateMenuDown()
{
    if (currentScreen == SCREEN_MENU)
    {
        selectedMenuItem = (selectedMenuItem + 1) % totalMenuItems;
        adjustMenuView();
        Serial.printf("DOWN -> %d: %s\n", selectedMenuItem, menuItems[selectedMenuItem].text);
    }
    else if (currentScreen == SCREEN_CODE_INPUT)
    {
        codeDigits[currentDigit] = (codeDigits[currentDigit] + 9) % 10; // -1 mod 10
    }
    // MAIN_DATA : pas de navigation up/down
}

void selectMenuItem()
{
    if (currentScreen == SCREEN_MAIN_DATA)
    {
        // OK sur l'écran principal → aller au menu
        currentScreen = SCREEN_MENU;
        Serial.println("Passage vers le menu");
    }
    else if (currentScreen == SCREEN_MENU)
    {
        executeMenuAction(selectedMenuItem);
    }
    else if (currentScreen == SCREEN_CODE_INPUT)
    {
        if (currentDigit < 2)
        {
            currentDigit++;
        }
        else
        {
            checkAdminCode();
        }
    }
    else if (currentScreen == SCREEN_CODE_RESULT)
    {
        currentScreen = SCREEN_MENU;
    }
}

void goBackMenu()
{
    switch (currentScreen)
    {
    case SCREEN_MAIN_DATA:
        // Retour depuis l'écran principal : ne fait rien (ou mode veille)
        Serial.println("Retour: déjà sur écran principal");
        break;
    case SCREEN_MENU:
        // Retour depuis le menu → écran principal
        currentScreen = SCREEN_MAIN_DATA;
        Serial.println("Retour vers écran principal");
        break;
    case SCREEN_CODE_INPUT:
        if (currentDigit > 0)
        {
            currentDigit--;
        }
        else
        {
            currentScreen = SCREEN_MENU;
        }
        break;
    case SCREEN_CODE_RESULT:
        currentScreen = SCREEN_MENU;
        break;
    }
}

// ——————— FONCTIONS D'AFFICHAGE ———————

void updateMenuDisplay()
{
    switch (currentScreen)
    {
    case SCREEN_MAIN_DATA:
        showMainDataScreen();
        break;
    case SCREEN_MENU:
        showMenuScreen();
        break;
    case SCREEN_CODE_INPUT:
        showCodeInputScreen();
        break;
    case SCREEN_CODE_RESULT:
        showCodeResultScreen();
        break;
    }
}

void showMainDataScreen()
{
    showMainData(); // Utilise la fonction du DisplayManager
}

void showMenuScreen()
{
    clearDisplay();

    // Titre
    const char *title = adminMode ? "=== MENU (ADMIN) ===" : "=== MENU PRINCIPAL ===";
    drawTitle(title);

    // Items visibles
    int visible = (totalMenuItems < VISIBLE_MENU_ITEMS) ? totalMenuItems : VISIBLE_MENU_ITEMS;
    for (int j = 0; j < visible; ++j)
    {
        int idx = menuViewTop + j;
        int y = 25 + j * 10;
        bool sel = (idx == selectedMenuItem);

        // Marquer visuellement les items admin
        const char *itemText = menuItems[idx].text;
        if (menuItems[idx].isAdminOnly)
        {
            // Ajouter un indicateur pour les items admin
            drawText(10, y, itemText, false, sel);
            drawText(2, y, "*", false, false); // Astérisque pour marquer admin
        }
        else
        {
            drawText(10, y, itemText, false, sel);
        }
    }

    // Curseur
    int rel = selectedMenuItem - menuViewTop;
    if (rel >= 0 && rel < visible)
    {
        drawMenuCursor(25 + rel * 10);
    }

    showDisplay();
}

void showCodeInputScreen()
{
    clearDisplay();
    drawTitle("CODE ADMIN");
    drawText(15, 25, "Entrez code (3 chiffres)");

    for (int i = 0; i < 3; ++i)
    {
        int x = 30 + i * 25;
        int y = 45;
        if (i == currentDigit)
        {
            drawFrame(x - 3, y - 15, 20, 18);
        }
        char buf[2];
        sprintf(buf, "%d", codeDigits[i]);
        drawText(x, y, buf, true); // large font
    }

    showDisplay();
}

void showCodeResultScreen()
{
    if (codeSuccess)
    {
        showMessage("CODE VALIDE", "Mode Admin Active");
    }
    else
    {
        showMessage("CODE INCORRECT", "Acces refuse");
    }

    if (millis() - resultTimer > MESSAGE_TIMEOUT)
    {
        if (codeSuccess)
            activateAdminMode();
        currentScreen = SCREEN_MENU;
    }
}

// ——————— FONCTIONS UTILITAIRES ———————

void adjustMenuView()
{
    if (totalMenuItems <= VISIBLE_MENU_ITEMS)
    {
        menuViewTop = 0;
        return;
    }

    // Centre la sélection dans la fenêtre visible
    menuViewTop = selectedMenuItem - (VISIBLE_MENU_ITEMS / 2);
    if (menuViewTop < 0)
        menuViewTop = 0;
    if (menuViewTop > totalMenuItems - VISIBLE_MENU_ITEMS)
    {
        menuViewTop = totalMenuItems - VISIBLE_MENU_ITEMS;
    }
}

void executeMenuAction(int idx)
{
    if (idx < 0 || idx >= totalMenuItems)
        return;

    Serial.printf("Action: %s\n", menuItems[idx].text);

    switch (menuItems[idx].action)
    {
    case ACTION_ADMIN_CODE:
        currentScreen = SCREEN_CODE_INPUT;
        resetCodeInput();
        break;
    case ACTION_DISPLAY_IDS:
        actionDisplayIds();
        break;
    case ACTION_PAIRING:
        actionPairing();
        break;
    case ACTION_ERRORS:
        actionShowErrors();
        break;
    case ACTION_INDIVIDUAL:
        actionIndividualBatteries();
        break;
    case ACTION_SYSTEM_SETTINGS:
        actionSystemSettings();
        break;
    }
}

// ——————— GESTION MODE ADMIN ———————

void activateAdminMode()
{
    if (!adminMode)
    {
        adminMode = true;
        buildMenu();
        Serial.println("Mode admin activé - Appairage et paramètres disponibles");
    }
}

void deactivateAdminMode()
{
    if (adminMode)
    {
        adminMode = false;
        buildMenu();
        Serial.println("Mode admin désactivé - Mode utilisateur standard");
    }
}

bool isAdminMode()
{
    return adminMode;
}

// ——————— GESTION CODE ADMIN ———————

void resetCodeInput()
{
    codeDigits[0] = codeDigits[1] = codeDigits[2] = 0;
    currentDigit = 0;
}

void checkAdminCode()
{
    codeSuccess = (codeDigits[0] == ADMIN_CODE_1 &&
                   codeDigits[1] == ADMIN_CODE_2 &&
                   codeDigits[2] == ADMIN_CODE_3);

    Serial.printf("Code saisi: %d%d%d -> %s\n",
                  codeDigits[0], codeDigits[1], codeDigits[2],
                  codeSuccess ? "OK" : "KO");

    currentScreen = SCREEN_CODE_RESULT;
    resultTimer = millis();
}

// ——————— ACTIONS MENU (STUBS) ———————

void actionDisplayIds()
{
    Serial.println("Action: Afficher ID batteries");
}

void actionShowErrors()
{
    Serial.println("Action: Affichage erreurs");
}

void actionIndividualBatteries()
{
    Serial.println("Action: Batteries individuelles");
}

void actionPairing()
{
    Serial.println("=== DÉBUT APPAIRAGE DES BATTERIES ===");
    // Cette fonction sera appelée depuis le main avec accès aux fonctions Modbus
}

void actionSystemSettings()
{
    Serial.println("Action: Parametres systeme (ADMIN)");
}

// ——————— GETTERS ———————

int getCurrentScreen()
{
    return currentScreen;
}

int getTotalMenuItems()
{
    return totalMenuItems;
}