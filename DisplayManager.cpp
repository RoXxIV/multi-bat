#include "DisplayManager.h"

// ——————— VARIABLE GLOBALE ———————
U8G2 *display_u8g2 = nullptr;

// ——————— FONCTIONS D'INITIALISATION ———————

void initDisplay(U8G2 *u8g2_ptr)
{
    display_u8g2 = u8g2_ptr;
    display_u8g2->begin();
    display_u8g2->setFont(u8g2_font_6x10_tf);
    clearDisplay();
    showDisplay();
    Serial.println("Écran initialisé");
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

void showMainData()
{
    clearDisplay();

    // Données fictives pour test
    float soc = 75.5;              // %
    float voltage = 51.2;          // V
    float current = -12.3;         // A (négatif = charge)
    float chargeSetpoint = 450;    // A
    float dischargeSetpoint = 600; // A
    float avgTemp = 23.4;          // °C

    // Ligne 1 : SOC et Tension
    char line[32];
    sprintf(line, "SOC:%.1f%%", soc);
    drawText(5, 12, line);

    sprintf(line, "V:%.1fV", voltage);
    drawText(70, 12, line);

    // Ligne 2 : Intensité et Température
    sprintf(line, "I:%.1fA", current);
    drawText(5, 22, line);

    sprintf(line, "Temp:%.1fC", avgTemp);
    drawText(70, 22, line);

    // Ligne 3 : Consignes
    sprintf(line, "Ch:%dA", (int)chargeSetpoint);
    drawText(5, 32, line);

    sprintf(line, "Dch:%dA", (int)dischargeSetpoint);
    drawText(70, 32, line);

    // Ligne 5 : Boutons
    drawText(5, 55, "R:N/A");
    drawText(80, 55, "OK:menu");

    showDisplay();
}