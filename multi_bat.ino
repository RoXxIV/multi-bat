#include <Wire.h>
#include <U8g2lib.h>
#include "Config.h"
#include "DisplayManager.h"
#include "MenuManager.h"
#include "ButtonManager.h"

// U8G2 (ex: SH1106 I2C)
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, OLED_SCL_PIN, OLED_SDA_PIN, OLED_RESET);

// Managers
DisplayManager displayMgr(&u8g2);
MenuManager menuMgr(&displayMgr);
ButtonManager buttons;

void setup()
{
  Serial.begin(115200);
  initializeConfig();
  printSystemInfo();

  displayMgr.begin();
  buttons.begin(BTN_UP_PIN, BTN_DOWN_PIN, BTN_OK_PIN, BTN_BACK_PIN);
  buttons.setDebounceDelay(DEBOUNCE_DELAY);

  menuMgr.begin();
}

void loop()
{
  buttons.update();

  if (buttons.isUpPressed())
    menuMgr.navigateUp();
  if (buttons.isDownPressed())
    menuMgr.navigateDown();
  if (buttons.isOkPressed())
    menuMgr.selectCurrentItem();
  if (buttons.isBackPressed())
    menuMgr.goBack();

  menuMgr.updateDisplay();
}
