#include "MenuManager.h"

MenuManager::MenuManager(DisplayManager *displayMgr)
    : display(displayMgr),
      currentState(SCREEN_MAIN_MENU),
      selectedMenuItem(0),
      totalMenuItems(0),
      adminAuthenticated(false),
      currentDigit(0),
      resultTimer(0),
      codeSuccess(false) {}

void MenuManager::begin()
{
    buildMenuItems(); // construit le menu de base
    resetCodeInput();
    viewTop = 0;
    if (totalMenuItems > VISIBLE_COUNT)
    {
        // optionnel: démarre sur 0..3 (classique)
        // si tu veux démarrer sur 1..4, mets viewTop = 1;
    }
    Serial.printf("MenuManager prêt - %d items\n", totalMenuItems);
    Serial.printf("MenuManager prêt - %d items\n", totalMenuItems);
}

void MenuManager::buildMenuItems()
{
    totalMenuItems = 0;

    // Items de base
    for (int i = 0; i < MENU_BASE_COUNT && totalMenuItems < MAX_MENU_ITEMS; i++)
    {
        menuItems[totalMenuItems] = {BASE_MENU_ITEMS[i], true, false};
        totalMenuItems++;
    }

    // Items admin si authentifié
    if (adminAuthenticated)
    {
        for (int i = 0; i < MENU_ADMIN_COUNT && totalMenuItems < MAX_MENU_ITEMS; i++)
        {
            menuItems[totalMenuItems] = {ADMIN_MENU_ITEMS[i], true, true};
            totalMenuItems++;
        }
    }

    if (totalMenuItems == 0)
    {
        // garde-fou : jamais 0
        menuItems[0] = {"Aucun item", false, false};
        totalMenuItems = 1;
    }

    if (selectedMenuItem >= totalMenuItems)
    {
        selectedMenuItem = totalMenuItems - 1;
    }
    int maxTop = (totalMenuItems > VISIBLE_COUNT) ? (totalMenuItems - VISIBLE_COUNT) : 0;
    if (viewTop > maxTop)
        viewTop = maxTop;
    if (viewTop < 0)
        viewTop = 0;
}

void MenuManager::navigateUp()
{
    if (currentState == SCREEN_MAIN_MENU)
    {
        int before = selectedMenuItem;
        selectedMenuItem = (selectedMenuItem - 1 + totalMenuItems) % totalMenuItems;
        adjustViewAfterMove(-1);
        Serial.printf("UP -> %d: %s\n", selectedMenuItem, menuItems[selectedMenuItem].text);
    }
    else if (currentState == SCREEN_CODE_INPUT)
    {
        codeDigits[currentDigit] = (codeDigits[currentDigit] + 1) % 10;
    }
}

void MenuManager::navigateDown()
{
    if (currentState == SCREEN_MAIN_MENU)
    {
        int before = selectedMenuItem;
        selectedMenuItem = (selectedMenuItem + 1) % totalMenuItems;
        adjustViewAfterMove(+1);
        Serial.printf("DOWN -> %d: %s\n", selectedMenuItem, menuItems[selectedMenuItem].text);
    }
    else if (currentState == SCREEN_CODE_INPUT)
    {
        codeDigits[currentDigit] = (codeDigits[currentDigit] + 9) % 10; // -1 mod 10
    }
}

void MenuManager::selectCurrentItem()
{
    if (currentState == SCREEN_MAIN_MENU)
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
    case SCREEN_MAIN_MENU:
        // rien
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
    const char *t = menuItems[idx].text;
    Serial.printf("Action: %s\n", t);

    // ——— Ouverture de la saisie de code
    if (strcmp(t, "Mode admin") == 0)
    {
        actionAdminCode();
        currentState = SCREEN_CODE_INPUT;
        resetCodeInput();
        return;
    }

    // ——— Items admin (apparaissent seulement si adminAuthenticated == true)
    if (strcmp(t, "Parametres systeme") == 0)
    {
        actionSystemSettings();
        return;
    }

    // ——— Items de base
    if (strcmp(t, "Afficher ID batteries") == 0)
    {
        actionDisplayIds();
        return;
    }
    if (strcmp(t, "Effectuer appairage") == 0)
    {
        actionPairing();
        return;
    }
    if (strcmp(t, "Affichage erreurs") == 0)
    {
        actionErrors();
        return;
    }
    if (strcmp(t, "Batteries individuelles") == 0)
    {
        actionIndividual();
        return;
    }
}

void MenuManager::updateDisplay()
{
    switch (currentState)
    {
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

// ——— Rendu

void MenuManager::displayMainMenu()
{
    DisplayLine lines[MAX_MENU_ITEMS + 2];
    int n = 0;

    lines[n++] = {12, adminAuthenticated ? "=== MENU (ADMIN) ===" : "=== MENU PRINCIPAL ===",
                  FONT_MEDIUM, false, 5};

    // on affiche au plus 4 éléments, en partant de viewTop
    int visible = min(VISIBLE_COUNT, totalMenuItems);
    for (int j = 0; j < visible; ++j)
    {
        int idx = viewTop + j; // pas de modulo: on garde une fenêtre linéaire
        int y = 25 + j * 10;
        bool sel = (idx == selectedMenuItem);
        lines[n++] = {y, menuItems[idx].text, FONT_MEDIUM, sel, 10};
    }

    display->drawScreen(lines, n);

    // Curseur aligné sur la position relative
    int rel = selectedMenuItem - viewTop; // 0..visible-1
    if (rel >= 0 && rel < visible)
    {
        display->drawMenuCursor(25 + rel * 10);
    }
}

void MenuManager::adjustViewAfterMove(int dir)
{
    if (totalMenuItems <= VISIBLE_COUNT)
    {
        viewTop = 0;
        return;
    }

    int maxTop = totalMenuItems - VISIBLE_COUNT;

    if (dir > 0)
    { // DOWN
        // Cas wrap: dernier -> 0
        if (selectedMenuItem == 0)
        {
            viewTop = 0; // <-- clé: on remonte la fenêtre pour montrer l'index 0
            return;
        }
        // Si la sélection dépasse le bas de la fenêtre, on pousse
        int bottom = viewTop + VISIBLE_COUNT - 1;
        if (selectedMenuItem > bottom)
        {
            viewTop = selectedMenuItem - (VISIBLE_COUNT - 1);
            if (viewTop > maxTop)
                viewTop = maxTop;
        }
    }
    else
    { // dir < 0, UP
        // Cas wrap: 0 -> dernier
        if (selectedMenuItem == totalMenuItems - 1)
        {
            viewTop = maxTop; // affiche les 4 derniers
            return;
        }
        // Si la sélection remonte au-dessus de la fenêtre, on remonte
        if (selectedMenuItem < viewTop)
        {
            viewTop = selectedMenuItem;
            if (viewTop < 0)
                viewTop = 0;
        }
    }
}

void MenuManager::displayCodeInput()
{
    display->clear();
    display->drawTitle(MSG_CODE_TITLE);
    display->drawText(15, 25, MSG_CODE_PROMPT, FONT_MEDIUM);

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
        display->drawText(x, y, buf, FONT_LARGE);
    }

    // On retire l’instruction en bas d’écran
    // display->drawInstructions("UP/DN:Chiffre  OK:Suivant  BACK:Retour");
    display->show();
}

void MenuManager::displayCodeResult()
{
    if (codeSuccess)
    {
        display->showMessage(MSG_CODE_SUCCESS, MSG_ADMIN_ACTIVATED, 0);
    }
    else
    {
        display->showMessage(MSG_CODE_ERROR, "Acces refuse", 0);
    }

    if (millis() - resultTimer > MESSAGE_TIMEOUT)
    {
        if (codeSuccess)
            activateAdmin();             // ← ajoute les items admin
        currentState = SCREEN_MAIN_MENU; // retour menu
    }
}

// ——— Code admin

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

// ——— Admin on/off

void MenuManager::activateAdmin()
{
    if (!adminAuthenticated)
    {
        adminAuthenticated = true;
        buildMenuItems(); // reconstruit le menu avec ADMIN_MENU_ITEMS
        Serial.println("Admin activé (menu étendu)");
    }
}

void MenuManager::deactivateAdmin()
{
    if (adminAuthenticated)
    {
        adminAuthenticated = false;
        buildMenuItems();
        Serial.println("Admin désactivé (menu de base)");
    }
}

// ——— Stubs d’actions

void MenuManager::actionAdminCode() { Serial.println("Action: Saisie code admin"); }
void MenuManager::actionSystemSettings() { Serial.println("Action: Parametres systeme"); }
void MenuManager::actionDisplayIds() { Serial.println("Action: Afficher ID batteries"); }
void MenuManager::actionPairing() { Serial.println("Action: Effectuer appairage"); }
void MenuManager::actionErrors() { Serial.println("Action: Affichage erreurs"); }
void MenuManager::actionIndividual() { Serial.println("Action: Batteries individuelles"); }
