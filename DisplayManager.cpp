#include "DisplayManager.h"
#include "ModbusManager.h"
#include "CanBusManager.h"
// ——————— VARIABLE GLOBALE ———————
U8G2 *display_u8g2 = nullptr;
bool isScreenOn = true; // vvv
extern AggregateBatteryMetrics latestMetrics;
extern int configuredBatteryCount;

// ——————— FONCTIONS D'INITIALISATION ———————
void initDisplay(U8G2 *u8g2_ptr)
{
    display_u8g2 = u8g2_ptr;
    display_u8g2->begin();
    display_u8g2->setFont(u8g2_font_6x10_tf);
    turnOnDisplay(); // vvv
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

    if (latestMetrics.isDataValid)
    {
        float soc = latestMetrics.averageSoc;
        float voltage = latestMetrics.averageVoltage;
        float current = latestMetrics.totalCurrent;

        // --- 1. Trouver les températures max T1 et T2 (inchangé) ---
        float maxT1 = -100.0f;
        float maxT2 = -100.0f;
        for (int i = 0; i < configuredBatteryCount; i++)
        {
            if (individualBatteryMetrics[i + 1].isValid)
            {
                if (individualBatteryMetrics[i + 1].temp1 > maxT1)
                    maxT1 = individualBatteryMetrics[i + 1].temp1;
                if (individualBatteryMetrics[i + 1].temp2 > maxT2)
                    maxT2 = individualBatteryMetrics[i + 1].temp2;
            }
        }

        // --- 2. NOUVEAU : Calculer la moyenne des températures batteries ---
        float avgBatteryTemp = (maxT1 + maxT2) / 2.0f;

        float chargeSetpoint = getChargeCurrentSetpoint();
        float dischargeSetpoint = getDischargeCurrentSetpoint();

        char line[32];

        // --- 3. NOUVELLE DISPOSITION D'AFFICHAGE sur 3 lignes ---

        // Ligne 1 : SOC et Tension
        sprintf(line, "SOC:%.1f%%", soc);
        drawText(5, 12, line);
        sprintf(line, "V:%.1fV", voltage);
        drawText(70, 12, line);

        // Ligne 2 : Courant et Température moyenne des batteries
        sprintf(line, "I:%.1fA", current);
        drawText(5, 22, line);
        sprintf(line, "Temp:%.1fC", avgBatteryTemp); // <-- Utilisation de la nouvelle variable
        drawText(70, 22, line);

        // Ligne 3 : Consignes Charge / Décharge
        sprintf(line, "Ch:%dA", (int)chargeSetpoint);
        drawText(5, 32, line);
        sprintf(line, "Dch:%dA", (int)dischargeSetpoint);
        drawText(70, 32, line);
    }
    else
    {
        drawText(5, 25, "En attente des");
        drawText(5, 35, "donnees batteries...");
    }

    drawText(5, 55, "R:N/A");
    drawText(80, 55, "OK:menu");

    showDisplay();
}

void turnOnDisplay()
{
    if (display_u8g2)
    {
        display_u8g2->setPowerSave(0);
        isScreenOn = true;
        Serial.println("Écran allumé");
    }
}

void turnOffDisplay()
{
    if (display_u8g2)
    {
        display_u8g2->setPowerSave(1);
        isScreenOn = false;
        Serial.println("Écran éteint");
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