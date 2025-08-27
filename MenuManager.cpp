#include "MenuManager.h"
#include "CanBusManager.h"
#include "ButtonManager.h"
#include "NvsManager.h"

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
    menuItems[totalMenuItems++] = {"Afficher trames CAN", ACTION_CAN_FRAMES, false};
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
    case SCREEN_CAN_FRAMES:
        setCanDisplayActive(false);
        currentScreen = SCREEN_MENU;
        Serial.println("Retour du menu CAN vers menu principal");
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
    case SCREEN_CAN_FRAMES: // ⭐ MANQUE ICI !
        showCanFramesScreen();
        break;
    }
}

void showMainDataScreen()
{
    showMainData(); // Utilise la fonction du DisplayManager (pour l'instant, fake data)
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

    if (millis() - resultTimer > MESSAGE_TIMEOUT) // 1500ms
    {
        if (codeSuccess)
            activateAdminMode();
        currentScreen = SCREEN_MENU;
    }
}

void showCanFramesScreen()
{
    // Utiliser la fonction du CanBusManager
    extern void showCanFrames();
    showCanFrames();
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
    case ACTION_CAN_FRAMES:
        actionShowCanFrames();
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

void actionSystemSettings()
{
    Serial.println("Action: Parametres systeme (ADMIN)");
}

void actionShowCanFrames()
{
    Serial.println("Action: Affichage trames CAN");
    extern void setCanDisplayActive(bool active);
    setCanDisplayActive(true);
    currentScreen = SCREEN_CAN_FRAMES; // ⭐ CRUCIAL: changer l'écran
    Serial.printf("DEBUG: currentScreen = %d\n", currentScreen);
}

// ——————— APPAIRAGE DES BATTERIES ———————
void actionPairing() //
{
    Serial.println("=== DÉBUT APPAIRAGE ===");

    // Étape 1: Reset tous les IDs à 1
    Serial.println("Étape 1: Reset ID batteries vers 1...");
    showMessage("APPAIRAGE", "Reset ID batteries...");
    changeAllBatteriesToId1();
    delay(2000);

    // Étape 2: Demander déconnexion
    Serial.println("Étape 2: Déconnexion des batteries...");
    waitForUserConfirmation("APPAIRAGE", "Deconnectez toutes les", "batteries puis appuyez", "sur OK pour continuer");

    // Étape 3: Appairage séquentiel avec menu
    Serial.println("Étape 3: Appairage séquentiel...");
    sequentialPairingWithMenu();

    Serial.println("=== APPAIRAGE TERMINÉ ===");
}

void waitForUserConfirmation(const char *title, const char *line1, const char *line2, const char *line3)
{
    Serial.printf("Attente confirmation utilisateur: %s\n", title);

    bool waiting = true;
    unsigned long lastUpdate = 0;

    while (waiting)
    {
        // Mettre à jour les boutons
        updateButtons();

        // Vérifier si OK est pressé
        if (isOkPressed())
        {
            waiting = false;
            Serial.println("Utilisateur a confirmé avec OK");
            break;
        }

        // Mettre à jour l'affichage (moins fréquent pour éviter scintillement)
        if (millis() - lastUpdate >= 200)
        {
            clearDisplay();
            drawTitle(title);

            // Afficher les lignes de texte
            if (line1)
                drawText(5, 25, line1);
            if (line2)
                drawText(5, 35, line2);
            if (line3)
                drawText(5, 45, line3);

            // Instructions
            drawText(5, 60, "OK: continuer", false, false);

            showDisplay();
            lastUpdate = millis();
        }

        // Petite pause pour éviter surcharge CPU
        delay(10);
    }
}

void sequentialPairingWithMenu()
{
    int currentBatteryNumber = 1;
    int batteriesConfigured = 0;
    bool continuePairing = true;

    while (continuePairing)
    {
        // Demander de brancher la batterie
        char title[20];
        sprintf(title, "BATTERIE #%d", currentBatteryNumber);

        char instruction[30];
        sprintf(instruction, "Brancher batterie #%d", currentBatteryNumber);

        waitForUserConfirmation(title, instruction, "puis appuyez sur OK", "pour confirmer");

        // Processus de configuration
        Serial.printf("Configuration batterie #%d...\n", currentBatteryNumber);
        showMessage("EN COURS", "Configuration...");

        int newId = currentBatteryNumber + 1; // ID 2, 3, 4, etc.
        bool success = configureBattery(newId);

        if (success)
        {
            batteriesConfigured++;
            showMessage("SUCCES", "Batterie configuree");
            delay(1500);

            // Menu de choix
            PairingChoice choice = showPairingMenu(batteriesConfigured);

            if (choice == CHOICE_ADD_BATTERY)
            {
                currentBatteryNumber++;
                if (currentBatteryNumber > 8) // Max 8 batteries
                {
                    showMessage("LIMITE", "Max 8 batteries");
                    delay(2000);
                    continuePairing = false;
                }
            }
            else // CHOICE_FINISH
            {
                // L'utilisateur a choisi de terminer, on lance la finalisation. h4
                finalizePairing(batteriesConfigured);
                continuePairing = false;
            }
        }
        else
        {
            showMessage("ERREUR", "Echec configuration");
            delay(2000);

            // En cas d'erreur, proposer de réessayer ou terminer
            PairingChoice choice = showErrorMenu();
            if (choice == CHOICE_FINISH)
            {
                continuePairing = false;
            }
            // Si CHOICE_ADD_BATTERY, on réessaie avec la même batterie
        }
    }

    // Affichage final
    char finalMsg[30];
    sprintf(finalMsg, "%d batteries config.", batteriesConfigured);
    waitForUserConfirmation("TERMINE", finalMsg, "Appuyez sur OK", "pour retourner au menu");
}

// Fonction pour configurer une batterie (ID + H7)
bool configureBattery(int newId)
{
    // Changer ID de 1 vers newId
    bool idChanged = changeBatteryIdFrom1To(newId);

    if (idChanged)
    {
        // Envoyer H=7 pour affichage
        sendDisplayIdToBattery(newId, ASCII_7);
        return true;
    }

    return false;
}

void finalizePairing(int batteriesConfigured)
{
    if (batteriesConfigured <= 0)
        return; // Ne fait rien si aucune batterie n'a été configurée

    Serial.println("=== FINALISATION APPAIRAGE ===");
    showMessage("FINALISATION", "Configuration H4...");

    // La valeur à envoyer est toujours '4' (H4), ce qui correspond à l'ASCII 0x34.
    const uint8_t asciiValueToSend = '4';

    // Boucle sur toutes les batteries qui viennent d'être appairées.
    // Les ID commencent à 2 et vont jusqu'au nombre de batteries configurées + 1.
    for (int id = 2; id <= batteriesConfigured + 1; id++)
    {
        Serial.printf("Envoi H4 (ASCII 0x%02X) à la batterie ID=%d\n", asciiValueToSend, id);

        // Envoie la commande H4 à la batterie avec son nouvel ID.
        // La fonction sendDisplayIdToBattery construit déjà la bonne trame (ex: 82... pour ID=2, 83... pour ID=3)
        sendDisplayIdToBattery(id, asciiValueToSend);

        // Petite pause pour ne pas surcharger le bus Modbus
        delay(500);
        saveBatteryCount(batteriesConfigured);
        showMessage("SUCCES", "Configuration OK !");
        delay(2000);
    }

    Serial.println("=== FINALISATION TERMINEE ===");
}
// Menu après configuration réussie
PairingChoice showPairingMenu(int batteriesCount)
{
    int selectedOption = 0;
    bool menuActive = true;
    unsigned long lastUpdate = 0;

    while (menuActive)
    {
        updateButtons();

        // Navigation
        if (isUpPressed())
        {
            selectedOption = (selectedOption - 1 + 2) % 2;
        }
        if (isDownPressed())
        {
            selectedOption = (selectedOption + 1) % 2;
        }
        if (isOkPressed())
        {
            return (selectedOption == 0) ? CHOICE_ADD_BATTERY : CHOICE_FINISH;
        }

        // Affichage
        if (millis() - lastUpdate >= 200)
        {
            clearDisplay();
            drawTitle("CHOIX");

            char status[25];
            sprintf(status, "%d batteries config.", batteriesCount);
            drawText(5, 20, status, false, false);

            // Options de menu
            drawText(10, 35, "Ajouter batterie", false, selectedOption == 0);
            drawText(10, 45, "Terminer", false, selectedOption == 1);

            // Curseur
            drawText(2, selectedOption == 0 ? 35 : 45, ">", false, false);

            showDisplay();
            lastUpdate = millis();
        }

        delay(10);
    }

    return CHOICE_FINISH;
}

// Menu en cas d'erreur
PairingChoice showErrorMenu()
{
    int selectedOption = 0;
    bool menuActive = true;
    unsigned long lastUpdate = 0;

    while (menuActive)
    {
        updateButtons();

        if (isUpPressed())
        {
            selectedOption = (selectedOption - 1 + 2) % 2;
        }
        if (isDownPressed())
        {
            selectedOption = (selectedOption + 1) % 2;
        }
        if (isOkPressed())
        {
            return (selectedOption == 0) ? CHOICE_ADD_BATTERY : CHOICE_FINISH;
        }

        if (millis() - lastUpdate >= 200)
        {
            clearDisplay();
            drawTitle("ERREUR CONFIG");

            drawText(5, 25, "Configuration echec", false, false);

            drawText(10, 35, "Reessayer", false, selectedOption == 0);
            drawText(10, 45, "Terminer", false, selectedOption == 1);

            drawText(2, selectedOption == 0 ? 35 : 45, ">", false, false);

            showDisplay();
            lastUpdate = millis();
        }

        delay(10);
    }

    return CHOICE_FINISH;
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
