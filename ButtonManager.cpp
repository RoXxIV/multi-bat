#include "ButtonManager.h"

// ——————— VARIABLES GLOBALES & SETUP ———————
ButtonState buttons[BTN_COUNT];
unsigned long debounceDelay = 50;

// ——————— FONCTIONS D'INITIALISATION ———————

void initButtons(int upPin, int downPin, int okPin, int backPin)
{
    // Affectation des pins
    buttons[BTN_UP].pin = upPin;
    buttons[BTN_DOWN].pin = downPin;
    buttons[BTN_OK].pin = okPin;
    buttons[BTN_BACK].pin = backPin;

    // Configuration des pins
    for (int i = 0; i < BTN_COUNT; i++)
    {
        if (buttons[i].pin != -1)
        {
            pinMode(buttons[i].pin, INPUT);

            // État initial
            bool rawState = digitalRead(buttons[i].pin);
            buttons[i].currentState = rawState;
            buttons[i].previousState = rawState;
            buttons[i].lastDebounce = millis();
        }
    }

    Serial.println("Boutons initialisés:");
    Serial.printf("  UP:%d DOWN:%d OK:%d BACK:%d\n", upPin, downPin, okPin, backPin);
}

void setDebounceDelay(unsigned long delay)
{
    debounceDelay = delay;
}

// ——————— FONCTIONS DE MISE À JOUR ———————

void updateButtons()
{
    for (int i = 0; i < BTN_COUNT; i++)
    {
        updateSingleButton(i);
    }
}

void updateSingleButton(int buttonIndex)
{
    if (buttonIndex < 0 || buttonIndex >= BTN_COUNT)
        return;
    if (buttons[buttonIndex].pin == -1)
        return;

    unsigned long now = millis();
    bool rawState = digitalRead(buttons[buttonIndex].pin);

    // Sauvegarder l'état précédent AVANT le filtrage
    buttons[buttonIndex].previousState = buttons[buttonIndex].currentState;

    // Filtrage anti-rebond
    if (now - buttons[buttonIndex].lastDebounce > debounceDelay)
    {
        buttons[buttonIndex].currentState = rawState;
        if (rawState)
        {
            buttons[buttonIndex].lastDebounce = now;
        }
    }
}

// ——————— FONCTIONS DE TEST D'APPUI ———————

bool isButtonPressed(int buttonIndex)
{
    if (buttonIndex < 0 || buttonIndex >= BTN_COUNT)
        return false;
    // Front montant : current = true ET previous = false
    return buttons[buttonIndex].currentState && !buttons[buttonIndex].previousState;
}

bool isUpPressed()
{
    return isButtonPressed(BTN_UP);
}

bool isDownPressed()
{
    return isButtonPressed(BTN_DOWN);
}

bool isOkPressed()
{
    return isButtonPressed(BTN_OK);
}

bool isBackPressed()
{
    return isButtonPressed(BTN_BACK);
}
