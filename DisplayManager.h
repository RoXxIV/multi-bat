#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include <U8g2lib.h>
#include "Config.h"

// Petites tailles de police "symboliques"
enum FontSize
{
    FONT_SMALL,
    FONT_MEDIUM,
    FONT_LARGE
};

// Ligne à dessiner via drawScreen
struct DisplayLine
{
    int y;
    const char *text;
    FontSize size;
    bool inverted; // surbrillance sélection
    int xOffset;   // marge gauche
};

class DisplayManager
{
public:
    explicit DisplayManager(U8G2 *u8) : u8g2(u8) {}

    void begin()
    {
        u8g2->begin();
        u8g2->clearBuffer();
        u8g2->setFont(u8g2_font_6x10_tf);
        u8g2->sendBuffer();
    }

    void clear() { u8g2->clearBuffer(); }
    void show() { u8g2->sendBuffer(); }

    void drawTitle(const char *title)
    {
        u8g2->setFont(u8g2_font_6x10_tf);
        u8g2->drawStr(5, 12, title);
        u8g2->drawLine(0, 14, 127, 14);
    }

    void drawText(int x, int y, const char *txt, FontSize size)
    {
        switch (size)
        {
        case FONT_SMALL:
            u8g2->setFont(u8g2_font_5x8_tf);
            break;
        case FONT_MEDIUM:
            u8g2->setFont(u8g2_font_6x10_tf);
            break;
        case FONT_LARGE:
            u8g2->setFont(u8g2_font_logisoso16_tf);
            break;
        }
        u8g2->drawStr(x, y, txt);
    }

    void drawFrame(int x, int y, int w, int h) { u8g2->drawFrame(x, y, w, h); }

    void drawInstructions(const char *txt)
    {
        u8g2->setFont(u8g2_font_5x8_tf);
        u8g2->drawStr(0, 63, txt);
    }

    void drawMenuCursor(int y)
    {
        // petite flèche à gauche de la ligne sélectionnée
        u8g2->drawStr(0, y, ">");
    }

    void drawScreen(const DisplayLine *lines, int count)
    {
        clear();
        for (int i = 0; i < count; i++)
        {
            const DisplayLine &L = lines[i];
            // inversion simple: on dessine un fond noir derrière le texte
            if (L.inverted)
            {
                u8g2->setDrawColor(1);
                u8g2->drawBox(0, L.y - 8, 128, 10);
                u8g2->setDrawColor(0);
                drawText(L.xOffset, L.y, L.text, L.size);
                u8g2->setDrawColor(1);
            }
            else
            {
                drawText(L.xOffset, L.y, L.text, L.size);
            }
        }
        show();
    }

    void showMessage(const char *title, const char *message, uint16_t /*ms*/)
    {
        clear();
        drawTitle(title);
        drawText(5, 32, message, FONT_MEDIUM);
        show();
    }

private:
    U8G2 *u8g2;
};

#endif
