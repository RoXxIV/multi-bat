#include <Wire.h>
#include <U8g2lib.h>
#include "Config.h"
#include "DisplayManager.h"
#include "MenuManager.h"
#include "ButtonManager.h"
#include "ModbusManager.h"
#include "CanBusManager.h"

// ——————— OBJETS HARDWARE ———————
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, OLED_SCL_PIN, OLED_SDA_PIN, OLED_RESET);

// ——————— VARIABLES DE TIMING ———————
unsigned long lastDisplayUpdate = 0;
unsigned long lastConsigneUpdate = 0;
#define DISPLAY_UPDATE_INTERVAL 500   // Mettre à jour l'affichage toutes les 500ms
#define CONSIGNE_UPDATE_INTERVAL 5000 // Changer les consignes toutes les 5s (pour test)

// ——————— SETUP ———————
void setup()
{
  Serial.begin(115200);
  Serial.println("=== MULTI-BATTERIE avec CAN ===");

  // Initialisation des modules
  initDisplay(&u8g2);
  initButtons(BTN_UP_PIN, BTN_DOWN_PIN, BTN_OK_PIN, BTN_BACK_PIN);
  setDebounceDelay(DEBOUNCE_DELAY);
  // Initialisation du Modbus
  initModbus(&MODBUS_SERIAL);

  // Initialisation du CAN Bus
  if (!initCanBus())
  {
    Serial.println("ERREUR: Impossible d'initialiser le CAN!");
    showMessage("ERREUR", "Echec init CAN");
    delay(2000);
  }

  initMenu();

  // Consignes initiales temporaire
  setChargeCurrentSetpoint(10.0);    // 10A comme sur le doc
  setDischargeCurrentSetpoint(10.0); // 10A comme sur le doc

  Serial.println("Système prêt !");
  Serial.println("Consignes variables: 0-600A pour charge/décharge");
  showMessage("SYSTEME", "Pret ! CAN actif");
  delay(1000);
}

// ——————— LOOP PRINCIPAL ———————
void loop()
{
  unsigned long now = millis();

  // ENVOI PÉRIODIQUE DES DONNÉES CAN
  sendCanData();

  // Faire varier les consignes automatiquement
  if (now - lastConsigneUpdate >= CONSIGNE_UPDATE_INTERVAL)
  {
    testVariableConsignes();
    lastConsigneUpdate = now;
  }

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

  // Mettre à jour l'affichage (moins fréquent pour éviter le scintillement)
  if (now - lastDisplayUpdate >= DISPLAY_UPDATE_INTERVAL)
  {
    updateMenuDisplay();
    lastDisplayUpdate = now;
  }

  // Petite pause pour éviter la surcharge CPU
  delay(10);
}

// ——————— TEST DES CONSIGNES VARIABLES ———————
/*
avec mode dégradé : 10/10
Seconde 0  : "Limite charge=10A, décharge=10A"    ← Très restrictif
Seconde 5  : "Limite charge=50A, décharge=100A"
Seconde 10 : "Limite charge=200A, décharge=300A"
Seconde 15 : "Limite charge=500A, décharge=600A"  ← Limite MAX
Seconde 20 : "Limite charge=0A, décharge=0A"      ← STOP complet !
Seconde 25 : "Limite charge=347A, décharge=82A"   ← Aléatoire
Seconde 30 : "Limite charge=156A, décharge=423A"  ← Aléatoire
Seconde 35 : "Limite charge=10A, décharge=10A"    ← Retour au début
*/
void testVariableConsignes()
{
  static int testStep = 0;

  switch (testStep)
  {
  case 0:
    setChargeCurrentSetpoint(10.0);
    setDischargeCurrentSetpoint(10.0);
    Serial.println("TEST: Consignes 10A/10A");
    break;
  case 1:
    setChargeCurrentSetpoint(50.0);
    setDischargeCurrentSetpoint(100.0);
    Serial.println("TEST: Consignes 50A/100A");
    break;
  case 2:
    setChargeCurrentSetpoint(200.0);
    setDischargeCurrentSetpoint(300.0);
    Serial.println("TEST: Consignes 200A/300A");
    break;
  case 3:
    setChargeCurrentSetpoint(500.0);
    setDischargeCurrentSetpoint(600.0);
    Serial.println("TEST: Consignes 500A/600A (MAX)");
    break;
  case 4:
    setChargeCurrentSetpoint(0.0);
    setDischargeCurrentSetpoint(0.0);
    Serial.println("TEST: Consignes 0A/0A (STOP)");
    break;
  default:
    // Consignes aléatoires
    float randomCharge = random(0, 601);    // 0-600A
    float randomDischarge = random(0, 601); // 0-600A
    setChargeCurrentSetpoint(randomCharge);
    setDischargeCurrentSetpoint(randomDischarge);
    Serial.printf("TEST: Consignes aléatoires %.0fA/%.0fA\n", randomCharge, randomDischarge);
    break;
  }

  testStep++;
  if (testStep > 8)
    testStep = 0; // Boucle de test
}

// ——————— GESTION BOUTON OK ———————
void handleOkButton()
{
  // Action normale du menu (gère l'écran principal → menu)
  selectMenuItem();
}

// ——————— FONCTIONS UTILITAIRES ———————
void printSystemStatus()
{
  Serial.println("\n=== STATUS SYSTÈME ===");
  Serial.printf("Consigne charge: %.1fA\n", getChargeCurrentSetpoint());
  Serial.printf("Consigne décharge: %.1fA\n", getDischargeCurrentSetpoint());
  Serial.printf("Uptime: %lu s\n", millis() / 1000);
  Serial.println("========================\n");
}