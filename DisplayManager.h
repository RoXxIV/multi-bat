#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include <U8g2lib.h>
#include "Config.h"

// ——————— VARIABLE GLOBALE ÉCRAN ———————
extern U8G2 *display_u8g2;

// ——————— FONCTIONS PUBLIQUES ———————

// Initialisation
void initDisplay(U8G2 *u8g2_ptr);

// Fonctions de base
void clearDisplay();
void showDisplay();

// Fonctions de dessin
void drawText(int x, int y, const char *text, bool large = false, bool inverted = false);
void drawTitle(const char *title);
void drawFrame(int x, int y, int w, int h);
void drawMenuCursor(int y);

// Fonctions utilitaires
void showMessage(const char *title, const char *message);
void showMainData();

#endif