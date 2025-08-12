#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include <U8g2lib.h>
#include "Config.h"

class DisplayManager
{
public:
    explicit DisplayManager(U8G2 *u8) : u8g2(u8) {}

    void begin()
    {
        u8g2->begin();
        u8g2->setFont(u8g2_font_6x10_tf);
        clear();
        show();
    }

    void clear() { u8g2->clearBuffer(); }
    void show() { u8g2->sendBuffer(); }

    // Méthode de texte unifiée avec paramètres optionnels
    void drawText(int x, int y, const char *txt, bool large = false, bool inverted = false)
    {
        u8g2->setFont(large ? u8g2_font_logisoso16_tf : u8g2_font_6x10_tf);

        if (inverted)
        {
            int textWidth = strlen(txt) * (large ? 12 : 6);
            int textHeight = large ? 16 : 10;
            u8g2->setDrawColor(1);
            u8g2->drawBox(x - 2, y - textHeight + 2, textWidth + 4, textHeight);
            u8g2->setDrawColor(0);
            u8g2->drawStr(x, y, txt);
            u8g2->setDrawColor(1);
        }
        else
        {
            u8g2->drawStr(x, y, txt);
        }
    }

    void drawTitle(const char *title)
    {
        drawText(5, 12, title);
        u8g2->drawLine(0, 14, 127, 14);
    }

    void drawFrame(int x, int y, int w, int h)
    {
        u8g2->drawFrame(x, y, w, h);
    }

    void drawMenuCursor(int y)
    {
        drawText(0, y, ">");
    }

    void showMessage(const char *title, const char *message)
    {
        clear();
        drawTitle(title);
        drawText(5, 32, message);
        show();
    }

private:
    U8G2 *u8g2;
};

#endif