#include "MenuManager.h"
#include "ButtonManager.h"
#include "NvsManager.h"
#include "BatteryLogic.h"

// ——————— VARIABLES GLOBALES ———————
int currentScreen = SCREEN_MAIN_DATA;
int selectedMenuItem = 0;
int totalMenuItems = 0;
int menuViewTop = 0;
bool adminMode = false;
int selectedBatteryIndex = 0;
int selectedBatteryId = 0;
int detailViewTop = 0;
int brightnessLevel = 3; // Niveau par défaut (1 à 5)
// Tableau de correspondance: index 0 est ignoré, index 1=Niveau 1, etc.
const uint8_t brightnessValues[] = {0, 1, 30, 80, 150, 255};
// Code admin
int codeDigits[3] = {0, 0, 0};
int currentDigit = 0;
unsigned long resultTimer = 0;
bool codeSuccess = false;
// Menu items
MenuItem menuItems[MAX_MENU_ITEMS];
int errorViewTop = 0;              // Index de la première erreur visible
const int VISIBLE_ERROR_ITEMS = 3; // Nombre d'erreurs affichées à l'écran
const char *APP_VERSION = "1.0.0";

// ——————— FONCTIONS D'INITIALISATION ———————
void initMenu()
{
  buildMenu();
  resetCodeInput();
  menuViewTop = 0;
}

void buildMenu()
{
  totalMenuItems = 0;

  // Items de base (utilisateur standard)
  menuItems[totalMenuItems++] = {"Regler luminosite", ACTION_BRIGHTNESS, false};
  menuItems[totalMenuItems++] = {"Version", ACTION_VERSION, false};
  menuItems[totalMenuItems++] = {"Mode admin", ACTION_ADMIN_CODE, false};
  // Items admin uniquement
  if (adminMode)
  {
    menuItems[totalMenuItems++] = {"Effectuer appairage", ACTION_PAIRING, true};
    menuItems[totalMenuItems++] = {"Batteries individuelles", ACTION_INDIVIDUAL, true};
    menuItems[totalMenuItems++] = {"Mettre a jour (OTA)", ACTION_OTA_UPDATE, true};
  }
  // Garde-fou
  if (totalMenuItems == 0)
  {
    menuItems[0] = {"Aucun item", ACTION_ERRORS, false};
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
  }
  else if (currentScreen == SCREEN_CODE_INPUT)
  {
    codeDigits[currentDigit] = (codeDigits[currentDigit] + 1) % 10;
  }
  else if (currentScreen == SCREEN_BRIGHTNESS)
  {
    if (brightnessLevel < 5)
    {
      brightnessLevel++;
      setBrightness(brightnessValues[brightnessLevel]);
    }
    // MAIN_DATA : pas de navigation up/down
  }
  else if (currentScreen == SCREEN_BATTERY_LIST)
  {
    // nombre de batteries
    if (configuredBatteryCount > 0)
    {
      selectedBatteryIndex = (selectedBatteryIndex - 1 + configuredBatteryCount) % configuredBatteryCount;
    }
  }
  else if (currentScreen == SCREEN_BATTERY_DETAIL)
  {
    if (detailViewTop > 0)
    {
      detailViewTop--;
    }
  }
  else if (currentScreen == SCREEN_ERROR_LIST)
  {
    if (errorViewTop > 0)
    {
      errorViewTop--;
    }
    else
    {
      currentScreen = SCREEN_MAIN_DATA;
    }
  }
  else if (currentScreen == SCREEN_MAIN_DATA)
  {
    currentScreen = SCREEN_ERROR_LIST;
    errorViewTop = 0;
  }
}

void navigateMenuDown()
{
  if (currentScreen == SCREEN_MENU)
  {
    selectedMenuItem = (selectedMenuItem + 1) % totalMenuItems;
    adjustMenuView();
  }
  else if (currentScreen == SCREEN_CODE_INPUT)
  {
    codeDigits[currentDigit] = (codeDigits[currentDigit] + 9) % 10; // -1 mod 10
  }
  else if (currentScreen == SCREEN_BRIGHTNESS)
  {
    if (brightnessLevel > 1)
    {
      brightnessLevel--;
      setBrightness(brightnessValues[brightnessLevel]);
    }
  }
  else if (currentScreen == SCREEN_BATTERY_LIST)
  {

    if (configuredBatteryCount > 0)
    {
      selectedBatteryIndex = (selectedBatteryIndex + 1) % configuredBatteryCount;
    }
  }
  else if (currentScreen == SCREEN_BATTERY_DETAIL)
  {
    // Le nombre de lignes de données est 7, et on en affiche 4.
    // La position maximale pour le scroll est donc l'index 3 (7 - 4).
    const int TOTAL_LINES = 7;
    const int VISIBLE_LINES = 4;
    if (detailViewTop < (TOTAL_LINES - VISIBLE_LINES))
    {
      detailViewTop++;
    }
  }
  else if (currentScreen == SCREEN_ERROR_LIST)
  {
    extern int errorCount;
    // Si on peut descendre, on descend
    if (errorCount > VISIBLE_ERROR_ITEMS && errorViewTop < (errorCount - VISIBLE_ERROR_ITEMS))
    {
      errorViewTop++;
    }
    else
    {
      // Si on est en bas, on "sort" vers l'écran principal
      currentScreen = SCREEN_MAIN_DATA;
    }
  }
  // --- MODIFICATION POUR L'ECRAN PRINCIPAL ---
  else if (currentScreen == SCREEN_MAIN_DATA)
  {
    // Depuis l'écran principal, BAS emmène aussi aux erreurs
    currentScreen = SCREEN_ERROR_LIST;
    errorViewTop = 0;
  }
}

void selectMenuItem()
{
  if (currentScreen == SCREEN_MAIN_DATA)
  {
    // OK sur l'écran principal → aller au menu
    currentScreen = SCREEN_MENU;
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
  else if (currentScreen == SCREEN_BRIGHTNESS)
  {
    // L'action est déjà appliquée en direct, OK sert juste à retourner au menu
    currentScreen = SCREEN_MENU;
  }
  else if (currentScreen == SCREEN_BATTERY_LIST)
  {

    if (configuredBatteryCount > 0)
    {
      // On stocke l'ID de la batterie sélectionnée (index + 2)
      selectedBatteryId = selectedBatteryIndex + 2;
      currentScreen = SCREEN_BATTERY_DETAIL;
      detailViewTop = 0;
    }
  }
  else if (currentScreen == SCREEN_ERROR_LIST)
  {
    // Le bouton OK depuis les erreurs emmène au Menu
    currentScreen = SCREEN_MENU;
  }
  else if (currentScreen == SCREEN_OTA)
  {
    // Démarrer le serveur si OK est pressé et que le serveur n'est pas déjà actif
    if (!otaServerActive)
    {
      if (startOTAServer())
      {
      }
      else
      {
        showMessage("ERREUR", "Echec WiFi");
        delay(2000);
      }
    }
  }
}

void goBackMenu()
{
  switch (currentScreen)
  {
  case SCREEN_MAIN_DATA:
    // Retour depuis l'écran principal : ne fait rien (ou mode veille)
    break;
  case SCREEN_MENU:
    // Retour depuis le menu → écran principal
    currentScreen = SCREEN_MAIN_DATA;
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
  case SCREEN_BRIGHTNESS:
    currentScreen = SCREEN_MENU;
    break;
  case SCREEN_BATTERY_LIST:
    currentScreen = SCREEN_MENU; // Retour de la liste vers le menu principal
    break;
  case SCREEN_BATTERY_DETAIL:
    currentScreen = SCREEN_BATTERY_LIST; // Retour des détails vers la liste
    break;
  case SCREEN_ERROR_LIST:
    currentScreen = SCREEN_MAIN_DATA;
    break;
  case SCREEN_OTA:
    stopOTAServer();
    startModbus();
    currentScreen = SCREEN_MENU;
    break;
  case SCREEN_VERSION:
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
    showMainData();
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
  case SCREEN_BRIGHTNESS:
    showBrightnessScreen();
    break;
  case SCREEN_BATTERY_LIST:
    showBatteryListScreen();
    break;
  case SCREEN_BATTERY_DETAIL:
    showBatteryDetailScreen();
    break;
  case SCREEN_ERROR_LIST:
    showErrorScreen();
    break;
  case SCREEN_OTA:
    showOTAScreen();
    break;
  case SCREEN_VERSION:
    showVersionScreen();
    break;
  }
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
  // drawText(15, 25, "Entrez code (3 chiffres)");
  drawSmallText(15, 25, "Entrez code (3 chiffres)");

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
    activateAdminMode();
    showMessage("CODE VALIDE", "Mode Admin Active");
  }
  else
  {
    showMessage("CODE INCORRECT", "Acces refuse");
  }

  if (millis() - resultTimer > MESSAGE_TIMEOUT) // 1500ms
  {
    if (codeSuccess)
      currentScreen = SCREEN_MENU;
  }
}

void showBatteryListScreen()
{

  clearDisplay();
  drawTitle("DETAILS BATTERIES");

  if (configuredBatteryCount == 0)
  {
    drawText(5, 35, "Aucune batterie");
    drawText(5, 45, "configuree.");
  }
  else
  {
    // Affiche jusqu'à 4 batteries à la fois
    for (int i = 0; i < configuredBatteryCount && i < 4; i++)
    {
      char line[20];
      int battery_id = i + 2; // Les ID commencent à 2
      sprintf(line, "Batterie ID %d", battery_id);

      // Met en surbrillance l'item sélectionné
      bool isSelected = (i == selectedBatteryIndex);
      drawText(10, 25 + i * 10, line, false, isSelected);
    }
    // Dessine le curseur
    drawMenuCursor(25 + selectedBatteryIndex * 10);
  }
  showDisplay();
}

void showBatteryDetailScreen()
{
  IndividualBatteryData *data = &individualBatteryMetrics[selectedBatteryId - 1];

  clearDisplay();
  char title[20];
  sprintf(title, "DETAILS ID %d", selectedBatteryId);
  drawTitle(title);

  if (!data->isValid)
  {
    drawText(5, 35, "Donnees invalides...");
  }
  else
  {
    // On prépare nos 7 lignes de données
    const int TOTAL_LINES = 7;
    static char lines[TOTAL_LINES][32];

    sprintf(lines[0], "V: %.2fV  I: %.2fA", data->voltage, data->current);
    sprintf(lines[1], "SOC: %.1f%%  SOH: %.1f%%", data->soc, data->soh);
    sprintf(lines[2], "Temp: %.1f/%.1fC", data->minCellTemp, data->maxCellTemp);
    sprintf(lines[3], "Diff cells: %.3fV", data->cellVoltageDifference);

    // Logique améliorée pour afficher le S/N sur 2 lignes
    char sn_part1[15] = {0}; // 14 caractères + null
    char sn_part2[15] = {0};
    strncpy(sn_part1, data->serialNumber, 14);
    if (strlen(data->serialNumber) > 14)
    {
      strncpy(sn_part2, data->serialNumber + 14, 14);
    }
    sprintf(lines[4], "S/N: %s", sn_part1);
    sprintf(lines[5], "     %s", sn_part2);

    sprintf(lines[6], "MOSFET Ch:%s Dch:%s", data->chargeMosfetStatus ? "ON" : "OFF", data->dischargeMosfetStatus ? "ON" : "OFF");

    // Boucle d'affichage
    const int VISIBLE_LINES = 4;
    for (int i = 0; i < VISIBLE_LINES; i++)
    {
      int line_index = detailViewTop + i;
      if (line_index < TOTAL_LINES)
      {
        drawText(5, 25 + i * 10, lines[line_index]);
      }
    }

    // Indicateurs de défilement
    if (detailViewTop > 0)
    {
      drawText(120, 25, "^");
    }
    if (detailViewTop < (TOTAL_LINES - VISIBLE_LINES))
    {
      drawText(120, 55, "v");
    }
  }

  showDisplay();
}

void showErrorScreen()
{
  extern SystemError systemErrors[MAX_SYSTEM_ERRORS];

  extern int errorCount;

  clearDisplay();

  drawTitle("ERREURS");

  if (errorCount == 0)

  {
    drawText(5, 35, "Aucune erreur active");

    drawText(5, 45, "Systeme OK");
  }
  else
  {
    int displayCount = 0;
    int activeErrorsFound = 0;

    // On parcourt toutes les erreurs possibles
    for (int i = 0; i < MAX_SYSTEM_ERRORS; i++)

    {
      if (systemErrors[i].active)

      {
        // On ne dessine que si l'erreur est dans notre "fenêtre" de vue
        if (activeErrorsFound >= errorViewTop && displayCount < VISIBLE_ERROR_ITEMS)
        {
          char errorLine[40];
          if (systemErrors[i].batteryId >= 0)

          {
            sprintf(errorLine, "ID%d: %s", systemErrors[i].batteryId, systemErrors[i].description);
          }
          else
          {
            sprintf(errorLine, "SYS: %s", systemErrors[i].description);
          }
          drawSmallText(5, 30 + displayCount * 8, errorLine);
          displayCount++;
        }
        activeErrorsFound++;
      }
    }

    // --- Indicateurs de défilement ---
    if (errorViewTop > 0)
    {
      drawText(120, 30, "^");
      // Flèche vers le haut
    }
    if (errorViewTop < (errorCount - VISIBLE_ERROR_ITEMS))
    {
      drawText(120, 50, "v");
      // Flèche vers le bas
    }
  }

  showDisplay();
}

void showOTAScreen()
{
  clearDisplay();
  drawTitle("MISE A JOUR OTA");

  if (otaInProgress)
  {
    // Mise à jour en cours - affichage géré par les callbacks
    // Cette partie sera gérée automatiquement par ArduinoOTA
    return;
  }
  else if (!otaServerActive)
  {
    drawText(5, 22, "Serveur OTA eteint");
    drawText(5, 32, "Appuyez OK pour");
    drawText(5, 42, "demarrer");

    // Instructions
    drawText(5, 60, "OK:start  BACK:retour");
  }
  else
  {
    // Serveur actif - afficher les infos
    drawText(5, 22, "Serveur OTA actif");

    // Afficher les informations de connexion
    String otaInfo = getOTAInfo();

    // Parser et afficher les infos sur plusieurs lignes
    int lineY = 32;
    int startPos = 0;
    int newlinePos = 0;

    while ((newlinePos = otaInfo.indexOf('\n', startPos)) != -1)
    {
      String line = otaInfo.substring(startPos, newlinePos);
      if (line.length() > 20)
      {
        // Ligne trop longue, la couper
        drawText(5, lineY, line.substring(0, 20).c_str());
        lineY += 7;
        if (line.length() > 20)
        {
          drawText(5, lineY, line.substring(20).c_str());
          lineY += 7;
        }
      }
      else
      {
        drawText(5, lineY, line.c_str());
        lineY += 7;
      }
      startPos = newlinePos + 1;

      if (lineY > 50)
        break; // Éviter de déborder
    }

    // Dernière ligne (après le dernier \n)
    if (startPos < otaInfo.length() && lineY <= 50)
    {
      String lastLine = otaInfo.substring(startPos);
      drawText(5, lineY, lastLine.c_str());
    }

    drawText(5, 62, "BACK:arreter");
  }

  showDisplay();
}

void showVersionScreen()
{
  clearDisplay();
  drawTitle("VERSION");
  drawText(5, 35, "Version du firmware:");

  // Utilise la constante globale définie en haut
  drawText(5, 45, APP_VERSION, false, false);

  drawText(5, 62, "BACK: retour");
  showDisplay();
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

  switch (menuItems[idx].action)
  {
  case ACTION_ADMIN_CODE:
    currentScreen = SCREEN_CODE_INPUT;
    resetCodeInput();
    break;
  case ACTION_PAIRING:
    actionPairing();
    break;
  case ACTION_ERRORS:
    errorViewTop = 0;
    currentScreen = SCREEN_ERROR_LIST;
    break;
  case ACTION_INDIVIDUAL:
    selectedBatteryIndex = 0;
    currentScreen = SCREEN_BATTERY_LIST;
    break;
  case ACTION_BRIGHTNESS:
    currentScreen = SCREEN_BRIGHTNESS;
    break;
  case ACTION_VERSION:
    currentScreen = SCREEN_VERSION;
    break;
  case ACTION_OTA_UPDATE:
    stopModbus();
    actionOTAUpdate();
    break;
  }
}

// ——————— GESTION MODE ADMIN ———————
void updateAllBatteryDisplays(uint8_t asciiValue)
{
  // Boucle à partir de l'ID 2 jusqu'à l'ID max configuré
  for (int id = 2; id <= configuredBatteryCount + 1; id++)
  {
    sendDisplayIdToBattery(id, asciiValue);
    // Petite pause pour ne pas surcharger le bus Modbus
    delay(50);
  }
}

void activateAdminMode()
{
  if (!adminMode)
  {
    adminMode = true;
    updateAllBatteryDisplays(ASCII_7);
    buildMenu();
  }
}

void deactivateAdminMode()
{
  if (adminMode)
  {
    adminMode = false;
    updateAllBatteryDisplays(ASCII_4);
    buildMenu();
  }
}

// ——————— GESTION CODE ADMIN ———————
void resetCodeInput()
{
  codeDigits[0] = codeDigits[1] = codeDigits[2] = 0;
  currentDigit = 0;
}

void checkAdminCode()
{
  codeSuccess = (codeDigits[0] == ADMIN_CODE_1 && codeDigits[1] == ADMIN_CODE_2 && codeDigits[2] == ADMIN_CODE_3);

  currentScreen = SCREEN_CODE_RESULT;
  resultTimer = millis();
}

// ——————— APPAIRAGE DES BATTERIES ———————
void actionPairing() //
{
  // Étape 0: Demande de connexion des batteries
  waitForUserConfirmation(
      "APPAIRAGE",
      "Branchez toutes les",
      "batteries a appairer,",
      "puis appuyez sur OK.");

  // Étape 1: Reset tous les IDs à 1
  showMessage("APPAIRAGE", "Reset ID batteries...");
  changeAllBatteriesToId1();
  delay(2000);

  // Étape 2: Demander déconnexion
  waitForUserConfirmation("APPAIRAGE", "Deconnectez toutes les", "batteries puis appuyez", "sur OK pour continuer");

  // Étape 3: Appairage séquentiel avec menu
  sequentialPairingWithMenu();
}

void waitForUserConfirmation(const char *title, const char *line1, const char *line2, const char *line3)
{
  bool waiting = true;
  unsigned long lastUpdate = 0;

  while (waiting)
  {
    // Mettre à jour les boutons
    updateButtons();

    // Vérifier si OK est pressé
    if (isButtonPressed(BTN_OK))
    {
      waiting = false;
      break;
    }

    // Mettre à jour l'affichage (moins fréquent pour éviter scintillement)
    if (millis() - lastUpdate >= 200)
    {
      clearDisplay();
      drawTitle(title);

      // Afficher les lignes de texte
      if (line1)
        // drawText(5, 25, line1);
        drawSmallText(5, 25, line1);
      if (line2)
        // drawText(5, 35, line2);
        drawSmallText(5, 35, line2);
      if (line3)
        // drawText(5, 45, line3);
        drawSmallText(5, 45, line3);

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

  showMessage("FINALISATION", "Configuration H4...");

  // La valeur à envoyer est toujours '4' (H4), ce qui correspond à l'ASCII 0x34.
  const uint8_t asciiValueToSend = '4';

  // Boucle sur toutes les batteries qui viennent d'être appairées.
  // Les ID commencent à 2 et vont jusqu'au nombre de batteries configurées + 1.
  for (int id = 2; id <= batteriesConfigured + 1; id++)
  {
    // Envoie la commande H4 à la batterie avec son nouvel ID.
    // La fonction sendDisplayIdToBattery construit déjà la bonne trame (ex: 82... pour ID=2, 83... pour ID=3)
    sendDisplayIdToBattery(id, asciiValueToSend);

    // Petite pause pour ne pas surcharger le bus Modbus
    delay(500);
    showMessage("SUCCES", "Configuration OK !");
    delay(2000);
  }
  saveBatteryCount(batteriesConfigured);

  // 1. Mettre à jour la variable globale en RAM immédiatement
  extern int configuredBatteryCount; // Rappel explicite (déjà dans config.h mais sûr)
  configuredBatteryCount = batteriesConfigured;

  // 2. Réinitialiser la logique batterie (états, erreurs, timeouts)
  // Cela évite que le système considère les batteries comme "perdues"
  // à cause du temps passé dans le menu.
  initBatteryManagement();
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
    if (isButtonPressed(BTN_UP))
    {
      selectedOption = (selectedOption - 1 + 2) % 2;
    }
    if (isButtonPressed(BTN_DOWN))
    {
      selectedOption = (selectedOption + 1) % 2;
    }
    if (isButtonPressed(BTN_OK))
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

    if (isButtonPressed(BTN_UP))
    {
      selectedOption = (selectedOption - 1 + 2) % 2;
    }
    if (isButtonPressed(BTN_DOWN))
    {
      selectedOption = (selectedOption + 1) % 2;
    }
    if (isButtonPressed(BTN_OK))
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

void showBrightnessScreen()
{
  clearDisplay();
  drawTitle("LUMINOSITE");

  // Afficher le niveau actuel (1 à 5)
  char buf[20];
  sprintf(buf, "Niveau: %d / 5", brightnessLevel);
  drawText(5, 30, buf);

  // Dessiner une barre de progression basée sur le niveau
  int barWidth = map(brightnessLevel, 1, 5, 0, 120);
  drawFrame(3, 40, 122, 10);
  display_u8g2->drawBox(4, 41, barWidth, 8);
  // Instructions
  // drawText(0, 60, "R:annuler   OK:valider");

  showDisplay();
}

void actionOTAUpdate()
{
  currentScreen = SCREEN_OTA;
}

void handleAutoErrorScreen()
{
  extern int errorCount;
  static int lastKnownErrorCount = 0; // Mémoire statique (reste entre les appels)

  // Si le nombre d'erreurs a AUGMENTÉ depuis la dernière vérification
  if (errorCount > lastKnownErrorCount)
  {
    currentScreen = SCREEN_ERROR_LIST; // On force l'écran des erreurs
    extern int errorViewTop;           // On remet la vue en haut de la liste
    errorViewTop = 0;
    turnOnDisplay();     // On allume l'écran pour être sûr que l'utilisateur le voit
    updateMenuDisplay(); // On force une mise à jour immédiate de l'affichage
  }
  lastKnownErrorCount = errorCount; // Mise à jour de la mémoire pour la prochaine boucle
}