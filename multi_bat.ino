#include <Wire.h>
#include <U8g2lib.h>
#include "Config.h"
#include "DisplayManager.h"
#include "MenuManager.h"
#include "ButtonManager.h"
#include "ModbusManager.h"

// ——————— OBJETS HARDWARE ———————
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, OLED_SCL_PIN, OLED_SDA_PIN, OLED_RESET);

// ——————— SETUP ———————
void setup()
{
  Serial.begin(115200);
  Serial.println("=== MULTI-BATTERIE ===");
  // Initialisation des modules
  initDisplay(&u8g2);
  initButtons(BTN_UP_PIN, BTN_DOWN_PIN, BTN_OK_PIN, BTN_BACK_PIN);
  setDebounceDelay(DEBOUNCE_DELAY);
  initModbus(&MODBUS_SERIAL);
  initMenu();
  Serial.println("Système prêt !");
}

// ——————— LOOP PRINCIPAL ———————
void loop()
{
  // Mettre à jour les boutons
  updateButtons();
  // Traiter les appuis de boutons
  if (isUpPressed())
  {
    navigateMenuUp();
  }

  if (isDownPressed())
  {
    navigateMenuDown();
  }

  if (isOkPressed())
  {
    handleOkButton();
  }

  if (isBackPressed())
  {
    goBackMenu();
  }
  // Mettre à jour l'affichage
  updateMenuDisplay();
  // Petite pause pour éviter la surcharge CPU
  delay(10);
}

// ——————— GESTION BOUTON OK ———————
void handleOkButton()
{
  // Action normale du menu (gère l'écran principal → menu)
  selectMenuItem();
}
