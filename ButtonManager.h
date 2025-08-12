#ifndef BUTTON_MANAGER_H
#define BUTTON_MANAGER_H

#include <Arduino.h>

// Énumération des types de boutons
enum ButtonType
{
    BTN_TYPE_UP = 0,
    BTN_TYPE_DOWN,
    BTN_TYPE_OK,
    BTN_TYPE_BACK,
    BTN_COUNT
};

// Structure pour l'état d'un bouton
struct ButtonState
{
    int pin;                    // Pin du bouton
    bool currentState;          // État actuel (HIGH/LOW)
    bool previousState;         // État précédent
    unsigned long lastDebounce; // Timestamp du dernier changement

    // Méthode unique : appui simple
    bool pressed(); // Front montant (appui unique)
};

class ButtonManager
{
private:
    ButtonState buttons[BTN_COUNT];
    unsigned long debounceDelay;

    // Méthodes privées
    void updateButton(ButtonType buttonType);

public:
    // Constructeur
    ButtonManager();

    // Configuration simplifiée
    void begin(int upPin, int downPin, int okPin, int backPin);
    void setDebounceDelay(unsigned long delay) { debounceDelay = delay; }

    // Méthode principale
    void update(); // À appeler dans loop()

    // Getters simplifiés - UNIQUEMENT appuis uniques
    bool isUpPressed() { return buttons[BTN_TYPE_UP].pressed(); }
    bool isDownPressed() { return buttons[BTN_TYPE_DOWN].pressed(); }
    bool isOkPressed() { return buttons[BTN_TYPE_OK].pressed(); }
    bool isBackPressed() { return buttons[BTN_TYPE_BACK].pressed(); }

    // Utilitaires
    bool anyPressed(); // N'importe quel bouton pressé
    void resetAll();   // Reset de tous les états

    // Debug
    void printStates(); // Affiche l'état de tous les boutons
    const char *getButtonName(ButtonType type);
};

#endif