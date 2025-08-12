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
  Serial.println("=== MULTI-BATTERIE v2.2 ===");
  printSystemInfo();

  displayMgr.begin();
  buttons.begin(BTN_UP_PIN, BTN_DOWN_PIN, BTN_OK_PIN, BTN_BACK_PIN);
  buttons.setDebounceDelay(DEBOUNCE_DELAY);

  modbus.begin();

  menuMgr.begin();

  Serial.println("Système prêt !");
}

void loop()
{
  buttons.update();

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
  // Si on est en mode admin et qu'on sélectionne l'appairage
  if (menuMgr.getCurrentState() == SCREEN_MAIN_MENU &&
      menuMgr.isAdminAuthenticated())
  {

    Serial.println("=== DÉBUT TEST H=7 TOUTES BATTERIES ===");
    modbus.sendDisplayIdToAllBatteries();
  }

  // Action normale du menu
  menuMgr.selectCurrentItem();
}

// Fonction de test rapide (si besoin depuis Serial Monitor)
void testModbusNow()
{
  Serial.println("=== TEST MANUEL MODBUS ===");
  modbus.sendDisplayIdToAllBatteries();
}