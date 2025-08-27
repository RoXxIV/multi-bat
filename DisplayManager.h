#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include <U8g2lib.h>
#include "Config.h"

// ——————— VARIABLES GLOBALES ———————
extern U8G2 *display_u8g2; // Pointeur vers l'objet U8G2 de l'afficheur
extern bool isScreenOn;    // AJOUT: Indicateur si l'écran est allumé vvv

// ——————— FONCTIONS D'INITIALISATION ———————
void initDisplay(U8G2 *u8g2_ptr); // Initialise l'écran OLED avec la configuration par défaut

// ——————— FONCTIONS DE GESTION BUFFER ———————
void clearDisplay(); // Vide le buffer d'affichage (prépare un nouvel écran)
void showDisplay();  // Envoie le buffer vers l'écran physique (affiche le contenu)

// ——————— FONCTIONS DE DESSIN PRIMITIVES ———————
// Dessine du texte à une position donnée avec options de style
void drawText(int x, int y, const char *text, bool large = false, bool inverted = false);
void drawTitle(const char *title);          // Dessine un titre centré avec ligne de séparation
void drawFrame(int x, int y, int w, int h); // Dessine un rectangle/cadre aux dimensions spécifiées
void drawMenuCursor(int y);                 // Dessine le curseur de sélection de menu (">" à la position Y)
void setBrightness(uint8_t value);          // contrôle la luminosité de l'écran OLED
// ——————— FONCTIONS D'AFFICHAGE HAUT NIVEAU ———————
void showMessage(const char *title, const char *message); // Affiche un message simple avec titre et texte centré
void showMainData();                                      // Affiche l'écran principal avec données batteries (pour l'instant fake data)
// ——————— GESTION DE L'ALIMENTATION DE L'Ecran ———————
void turnOnDisplay();
void turnOffDisplay();
#endif