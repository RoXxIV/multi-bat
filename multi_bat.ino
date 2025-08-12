#include <Wire.h>
#include <U8g2lib.h>
#include "Config.h"
#include "DisplayManager.h"
#include "MenuManager.h"
#include "ButtonManager.h"
#include "ModbusManager.h"

// U8G2 (ex: SH1106 I2C)
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, OLED_SCL_PIN, OLED_SDA_PIN, OLED_RESET);

// Managers
DisplayManager displayMgr(&u8g2);
MenuManager menuMgr(&displayMgr);
ButtonManager buttons;
ModbusManager modbus(&MODBUS_SERIAL);

void setup()
{
  Serial.begin(115200);
  Serial.println("=== MULTI-BATTERIE v2.3 (Modbus Test) ===");
  printSystemInfo();

  // Initialisation des composants
  displayMgr.begin();
  buttons.begin(BTN_UP_PIN, BTN_DOWN_PIN, BTN_OK_PIN, BTN_BACK_PIN);
  buttons.setDebounceDelay(DEBOUNCE_DELAY);

  // NOUVEAU : Initialisation Modbus
  modbus.begin();

  menuMgr.begin();

  Serial.println("Système prêt avec Modbus !");
  Serial.println("RX:" + String(MODBUS_RX_PIN) + " TX:" + String(MODBUS_TX_PIN));
}

void loop()
{
  buttons.update();

  // Navigation menu classique
  if (buttons.isUpPressed())
    menuMgr.navigateUp();
  if (buttons.isDownPressed())
    menuMgr.navigateDown();
  if (buttons.isOkPressed())
    handleOkButton();
  if (buttons.isBackPressed())
    menuMgr.goBack();

  menuMgr.updateDisplay();
}

void handleOkButton()
{
  // Si on est dans le menu admin et qu'on sélectionne "Effectuer appairage"
  if (menuMgr.getCurrentState() == SCREEN_MAIN_MENU &&
      menuMgr.isAdminAuthenticated())
  {

    // Ici on devrait identifier quelle action est sélectionnée
    // Pour l'instant, on teste juste la commande H=7
    Serial.println("Test: Envoi commande H=7");
    modbus.sendDisplayIdCommand();
  }

  // Action normale du menu
  menuMgr.selectCurrentItem();
}

// Fonction de test rapide (appeler depuis Serial Monitor si besoin)
void testModbusNow()
{
  Serial.println("=== TEST MANUEL MODBUS ===");
  modbus.sendDisplayIdCommand();
}