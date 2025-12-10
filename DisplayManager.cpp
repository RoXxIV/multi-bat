#include "DisplayManager.h"
#include "ModbusManager.h"
#include "CanBusManager.h"
#include "BatteryLogic.h"

// ——————— VARIABLE GLOBALE ———————
U8G2 *display_u8g2 = nullptr;
bool isScreenOn = true;
extern AggregateBatteryMetrics latestMetrics;
extern int turnedOffBatteryId;

// ——————— FONCTIONS D'INITIALISATION ———————
void initDisplay(U8G2 *u8g2_ptr)
{
  display_u8g2 = u8g2_ptr;
  display_u8g2->begin();
  display_u8g2->setFont(u8g2_font_6x10_tf);
  turnOnDisplay();
  clearDisplay();
  showDisplay();
}

// ——————— FONCTIONS DE BASE ———————
void clearDisplay()
{
  if (display_u8g2)
  {
    display_u8g2->clearBuffer();
  }
}

void showDisplay()
{
  if (display_u8g2)
  {
    display_u8g2->sendBuffer();
  }
}

// ——————— FONCTIONS DE DESSIN ———————
void drawText(int x, int y, const char *text, bool large, bool inverted)
{
  if (!display_u8g2)
    return;

  display_u8g2->setFont(large ? u8g2_font_logisoso16_tf : u8g2_font_6x10_tf);

  if (inverted)
  {
    int textWidth = strlen(text) * (large ? 12 : 6);
    int textHeight = large ? 16 : 10;
    display_u8g2->setDrawColor(1);
    display_u8g2->drawBox(x - 2, y - textHeight + 2, textWidth + 4, textHeight);
    display_u8g2->setDrawColor(0);
    display_u8g2->drawStr(x, y, text);
    display_u8g2->setDrawColor(1);
  }
  else
  {
    display_u8g2->drawStr(x, y, text);
  }
}

void drawTitle(const char *title)
{
  if (!display_u8g2)
    return;

  drawText(5, 12, title);
  display_u8g2->drawLine(0, 14, 127, 14);
}

void drawFrame(int x, int y, int w, int h)
{
  if (display_u8g2)
  {
    display_u8g2->drawFrame(x, y, w, h);
  }
}

void drawMenuCursor(int y)
{
  drawText(0, y, ">");
}

// ——————— FONCTIONS UTILITAIRES ———————
void showMessage(const char *title, const char *message)
{
  clearDisplay();
  drawTitle(title);
  drawText(5, 32, message);
  showDisplay();
}

void drawSmallText(int x, int y, const char *text)
{
  if (display_u8g2)
  {
    // Police 5x7 pixels (plus petite et compacte)
    display_u8g2->setFont(u8g2_font_5x7_tf);
    display_u8g2->drawStr(x, y, text);
  }
}

void showMainData()
{
  clearDisplay();

  if (turnedOffBatteryId > 0)
  {
    char line[32];
    sprintf(line, "La batterie ID %d", turnedOffBatteryId);

    drawText(5, 25, "SECURITE: BOUTON OFF");
    drawText(5, 35, line);
    drawText(5, 45, "est eteinte.");

    // On n'affiche RIEN d'autre.
  }

  // --- AFFICHAGE NORMAL (SI PAS D'ALERTE) ---
  else if (latestMetrics.isDataValid)
  {
    float soc = latestMetrics.averageSoc;
    float voltage = latestMetrics.averageVoltage;
    float current = latestMetrics.totalCurrent;
    float tempSystem = latestMetrics.averageTemp;
    float chargeSetpoint = currentChargeSetpoint;
    float dischargeSetpoint = currentDischargeSetpoint;
    char line[32];
    display_u8g2->setFont(u8g2_font_logisoso16_tf); // police grande taille

    //___SOC centré---
    sprintf(line, "%.0f%%", soc);
    int w = display_u8g2->getStrWidth(line);
    int x = (128 - w) / 2;
    display_u8g2->drawStr(x, 24, line);
    display_u8g2->setFont(u8g2_font_6x10_tf); // retour police standard
    // Tension à gauche
    sprintf(line, "%.1fV", voltage);
    display_u8g2->drawStr(2, 42, line);
    // Courant aligné à droite
    sprintf(line, "%.1fA", current);
    w = display_u8g2->getStrWidth(line);
    display_u8g2->drawStr(126 - w, 42, line);
    // Température
    sprintf(line, "%.1fC", tempSystem);
    w = display_u8g2->getStrWidth(line);
    x = (128 - w) / 2;
    display_u8g2->drawStr(x, 54, line);
  }

  // --- AFFICHAGE "EN ATTENTE" (SI PAS D'ALERTE NI DONNÉES) ---
  else
  {
    drawText(5, 25, "En attente des");
    drawText(5, 35, "donnees batteries...");
  }

  // L'envoi vers l'écran se fait une seule fois à la fin
  showDisplay();
}

void turnOnDisplay()
{
  if (display_u8g2)
  {
    display_u8g2->setPowerSave(0);
    isScreenOn = true;
  }
}

void turnOffDisplay()
{
  if (display_u8g2)
  {
    display_u8g2->setPowerSave(1);
    isScreenOn = false;
  }
}

void setBrightness(uint8_t value)
{
  if (display_u8g2)
  {
    // La "luminosité" de l'OLED est contrôlée par le contraste
    display_u8g2->setContrast(value);
  }
}
