#include "MenuManager.h"

void MenuManager::displayMainData()
{
    display->clear();

    // Données fictives pour test
    float soc = 75.5;              // %
    float voltage = 51.2;          // V
    float current = -12.3;         // A (négatif = charge)
    float chargeSetpoint = 450;    // A
    float dischargeSetpoint = 600; // A
    float avgTemp = 23.4;          // °C

    // Ligne 1 : SOC et Tension
    display->drawText(5, 12, ("SOC:" + String(soc, 1) + "%").c_str());
    display->drawText(70, 12, ("V:" + String(voltage, 1) + "V").c_str());

    // Ligne 2 : Intensité et Température
    display->drawText(5, 22, ("I:" + String(current, 1) + "A").c_str());
    display->drawText(70, 22, ("Temp:" + String(avgTemp, 1) + "C").c_str());

    // Ligne 3 : Consignes
    display->drawText(5, 32, ("Ch:" + String((int)chargeSetpoint) + "A").c_str());
    display->drawText(70, 32, ("Dch:" + String((int)dischargeSetpoint) + "A").c_str());

    // Ligne 5 : Boutons
    display->drawText(5, 55, "R:N/A");
    display->drawText(80, 55, "OK:menu");

    display->show();
}
#include "MenuManager.h"

MenuManager::MenuManager(DisplayManager *displayMgr) : display(displayMgr),
                                                       currentState(SCREEN_MAIN_DATA), // Commencer par l'écran de données
                                                       selectedMenuItem(0),
                                                       totalMenuItems(0),
                                                       adminAuthenticated(false),
                                                       currentDigit(0),
                                                       resultTimer(0),
                                                       codeSuccess(false)
{
}

void MenuManager::begin()
{
    buildMenuItems();
    resetCodeInput();
    viewTop = 0;
    Serial.printf("MenuManager prêt - %d items\n", totalMenuItems);
}

void MenuManager::buildMenuItems()
{
    totalMenuItems = 0;

    // Items de base (utilisateur standard)
    menuItems[totalMenuItems++] = {"Afficher ID batteries", ACTION_DISPLAY_IDS, false};
    menuItems[totalMenuItems++] = {"Affichage erreurs", ACTION_ERRORS, false};
    menuItems[totalMenuItems++] = {"Batteries individuelles", ACTION_INDIVIDUAL, false};
    menuItems[totalMenuItems++] = {"Mode admin", ACTION_ADMIN_CODE, false};

    // Items admin uniquement
    if (adminAuthenticated)
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
    adjustView();
}

void MenuManager::navigateUp()
{
    if (currentState == SCREEN_MAIN_MENU)
    {
        selectedMenuItem = (selectedMenuItem - 1 + totalMenuItems) % totalMenuItems;
        adjustView();
        Serial.printf("UP -> %d: %s\n", selectedMenuItem, menuItems[selectedMenuItem].text);
    }
    else if (currentState == SCREEN_CODE_INPUT)
    {
        codeDigits[currentDigit] = (codeDigits[currentDigit] + 1) % 10;
    }
    // MAIN_DATA : pas de navigation up/down
}

void MenuManager::navigateDown()
{
    if (currentState == SCREEN_MAIN_MENU)
    {
        selectedMenuItem = (selectedMenuItem + 1) % totalMenuItems;
        adjustView();
        Serial.printf("DOWN -> %d: %s\n", selectedMenuItem, menuItems[selectedMenuItem].text);
    }
    else if (currentState == SCREEN_CODE_INPUT)
    {
        codeDigits[currentDigit] = (codeDigits[currentDigit] + 9) % 10; // -1 mod 10
    }
    // MAIN_DATA : pas de navigation up/down
}

void MenuManager::selectCurrentItem()
{
    if (currentState == SCREEN_MAIN_DATA)
    {
        // OK sur l'écran principal → aller au menu
        currentState = SCREEN_MAIN_MENU;
        Serial.println("Passage vers le menu");
    }
    else if (currentState == SCREEN_MAIN_MENU)
    {
        executeMenuAction(selectedMenuItem);
    }
    else if (currentState == SCREEN_CODE_INPUT)
    {
        if (currentDigit < 2)
        {
            currentDigit++;
        }
        else
        {
            checkCode();
        }
    }
    else if (currentState == SCREEN_CODE_RESULT)
    {
        currentState = SCREEN_MAIN_MENU;
    }
}

void MenuManager::goBack()
{
    switch (currentState)
    {
    case SCREEN_MAIN_DATA:
        // Retour depuis l'écran principal : ne fait rien (ou mode veille)
        Serial.println("Retour: unknow");
        break;
    case SCREEN_MAIN_MENU:
        // Retour depuis le menu → écran principal
        currentState = SCREEN_MAIN_DATA;
        Serial.println("Retour vers écran principal");
        break;
    case SCREEN_CODE_INPUT:
        if (currentDigit > 0)
        {
            currentDigit--;
        }
        else
        {
            currentState = SCREEN_MAIN_MENU;
        }
        break;
    case SCREEN_CODE_RESULT:
        currentState = SCREEN_MAIN_MENU;
        break;
    }
}

void MenuManager::executeMenuAction(int idx)
{
    if (idx < 0 || idx >= totalMenuItems)
        return;

    Serial.printf("Action: %s\n", menuItems[idx].text);

    switch (menuItems[idx].action)
    {
    case ACTION_ADMIN_CODE:
        currentState = SCREEN_CODE_INPUT;
        resetCodeInput();
        break;
    case ACTION_DISPLAY_IDS:
        actionDisplayIds();
        break;
    case ACTION_PAIRING:
        actionPairing();
        break;
    case ACTION_ERRORS:
        actionErrors();
        break;
    case ACTION_INDIVIDUAL:
        actionIndividual();
        break;
    case ACTION_SYSTEM_SETTINGS:
        actionSystemSettings();
        break;
    }
}

void MenuManager::updateDisplay()
{
    switch (currentState)
    {
    case SCREEN_MAIN_DATA:
        displayMainData();
        break;
    case SCREEN_MAIN_MENU:
        displayMainMenu();
        break;
    case SCREEN_CODE_INPUT:
        displayCodeInput();
        break;
    case SCREEN_CODE_RESULT:
        displayCodeResult();
        break;
    }
}

void MenuManager::displayMainMenu()
{
    display->clear();

    // Titre
    const char *title = adminAuthenticated ? "=== MENU (ADMIN) ===" : "=== MENU PRINCIPAL ===";
    display->drawTitle(title);

    // Items visibles
    int visible = min(VISIBLE_COUNT, totalMenuItems);
    for (int j = 0; j < visible; ++j)
    {
        int idx = viewTop + j;
        int y = 25 + j * 10;
        bool sel = (idx == selectedMenuItem);

        // Marquer visuellement les items admin
        const char *itemText = menuItems[idx].text;
        if (menuItems[idx].isAdminItem)
        {
            // Ajouter un indicateur pour les items admin
            display->drawText(10, y, itemText, false, sel);
            display->drawText(2, y, "*", false, false); // Astérisque pour marquer admin
        }
        else
        {
            display->drawText(10, y, itemText, false, sel);
        }
    }

    // Curseur
    int rel = selectedMenuItem - viewTop;
    if (rel >= 0 && rel < visible)
    {
        display->drawMenuCursor(25 + rel * 10);
    }

    display->show();
}

void MenuManager::adjustView()
{
    if (totalMenuItems <= VISIBLE_COUNT)
    {
        viewTop = 0;
        return;
    }

    // Centre la sélection dans la fenêtre visible
    viewTop = selectedMenuItem - (VISIBLE_COUNT / 2);
    viewTop = max(0, min(viewTop, totalMenuItems - VISIBLE_COUNT));
}

void MenuManager::displayCodeInput()
{
    display->clear();
    display->drawTitle("CODE ADMIN");
    display->drawText(15, 25, "Entrez code (3 chiffres)");

    for (int i = 0; i < 3; ++i)
    {
        int x = 30 + i * 25;
        int y = 45;
        if (i == currentDigit)
        {
            display->drawFrame(x - 3, y - 15, 20, 18);
        }
        char buf[2];
        sprintf(buf, "%d", codeDigits[i]);
        display->drawText(x, y, buf, true); // large font
    }

    display->show();
}

void MenuManager::displayCodeResult()
{
    if (codeSuccess)
    {
        display->showMessage("CODE VALIDE", "Mode Admin Active");
    }
    else
    {
        display->showMessage("CODE INCORRECT", "Acces refuse");
    }

    if (millis() - resultTimer > MESSAGE_TIMEOUT)
    {
        if (codeSuccess)
            activateAdmin();
        currentState = SCREEN_MAIN_MENU;
    }
}

void MenuManager::resetCodeInput()
{
    codeDigits[0] = codeDigits[1] = codeDigits[2] = 0;
    currentDigit = 0;
}

void MenuManager::checkCode()
{
    codeSuccess = (codeDigits[0] == ADMIN_CODE_1 &&
                   codeDigits[1] == ADMIN_CODE_2 &&
                   codeDigits[2] == ADMIN_CODE_3);

    Serial.printf("Code saisi: %d%d%d -> %s\n",
                  codeDigits[0], codeDigits[1], codeDigits[2],
                  codeSuccess ? "OK" : "KO");

    currentState = SCREEN_CODE_RESULT;
    resultTimer = millis();
}

void MenuManager::activateAdmin()
{
    if (!adminAuthenticated)
    {
        adminAuthenticated = true;
        buildMenuItems();
        Serial.println("Admin activé - Appairage et paramètres disponibles");
    }
}

void MenuManager::deactivateAdmin()
{
    if (adminAuthenticated)
    {
        adminAuthenticated = false;
        buildMenuItems();
        Serial.println("Admin désactivé - Mode utilisateur standard");
    }
}

// ——— Stubs d'actions
void MenuManager::actionDisplayIds() { Serial.println("Action: Afficher ID batteries"); }

void MenuManager::actionPairing()
{
    Serial.println("=== DÉBUT APPAIRAGE DES BATTERIES ===");
    // Cette fonction sera appelée depuis le main avec accès au ModbusManager
}

void MenuManager::actionErrors() { Serial.println("Action: Affichage erreurs"); }
void MenuManager::actionIndividual() { Serial.println("Action: Batteries individuelles"); }
void MenuManager::actionSystemSettings() { Serial.println("Action: Parametres systeme (ADMIN)"); }