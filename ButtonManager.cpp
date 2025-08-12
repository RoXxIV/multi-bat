#include "ButtonManager.h"

// ——————— MÉTHODES ButtonState ———————
bool ButtonState::pressed()
{
    return currentState && !previousState; // logique d'appui
}

// ——————— MÉTHODES ButtonManager ———————
ButtonManager::ButtonManager()
{
    debounceDelay = 50; // 50ms par défaut

    // Initialisation des états
    for (int i = 0; i < BTN_COUNT; i++)
    {
        buttons[i].pin = -1;
        buttons[i].currentState = false;
        buttons[i].previousState = false;
        buttons[i].lastDebounce = 0;
    }
}

void ButtonManager::begin(int upPin, int downPin, int okPin, int backPin)
{
    // Affectation des pins
    buttons[BTN_TYPE_UP].pin = upPin;
    buttons[BTN_TYPE_DOWN].pin = downPin;
    buttons[BTN_TYPE_OK].pin = okPin;
    buttons[BTN_TYPE_BACK].pin = backPin;

    // Configuration des pins
    for (int i = 0; i < BTN_COUNT; i++)
    {
        if (buttons[i].pin != -1)
        {
            pinMode(buttons[i].pin, INPUT);

            // Init façon alpha : on prend l'état brut au boot
            bool raw = digitalRead(buttons[i].pin);
            buttons[i].currentState = raw;      // filtré initial = brut
            buttons[i].previousState = raw;     // évite un pressed() fantôme
            buttons[i].lastDebounce = millis(); // point de départ chrono
        }
    }

    Serial.println("ButtonManager initialisé:");
    Serial.printf("  UP:%d DOWN:%d OK:%d BACK:%d\n", upPin, downPin, okPin, backPin);
}

void ButtonManager::update()
{
    for (int i = 0; i < BTN_COUNT; i++)
    {
        updateButton((ButtonType)i);
    }
}

void ButtonManager::updateButton(ButtonType buttonType)
{
    ButtonState &btn = buttons[buttonType];
    if (btn.pin == -1)
        return;

    unsigned long now = millis();
    bool raw = digitalRead(btn.pin); // lecture directe (HIGH = appui)

    // EXACTEMENT comme l'alpha : on fige l'état précédent AVANT le filtrage
    btn.previousState = btn.currentState;

    if (now - btn.lastDebounce > debounceDelay)
    {
        btn.currentState = raw;
        if (raw)
            btn.lastDebounce = now;
    }
}

bool ButtonManager::anyPressed()
{
    for (int i = 0; i < BTN_COUNT; i++)
    {
        if (buttons[i].pressed())
            return true;
    }
    return false;
}

void ButtonManager::resetAll()
{
    for (int i = 0; i < BTN_COUNT; i++)
    {
        buttons[i].currentState = false;
        buttons[i].previousState = false;
        buttons[i].lastDebounce = millis(); // repart proprement
    }
    Serial.println("Tous les boutons ont été réinitialisés");
}

// DEBUG
void ButtonManager::printStates()
{
    Serial.print("États boutons: ");
    for (int i = 0; i < BTN_COUNT; i++)
    {
        Serial.printf("%s:%s ",
                      getButtonName((ButtonType)i),
                      buttons[i].currentState ? "ON" : "off");
    }
    Serial.println();
}

const char *ButtonManager::getButtonName(ButtonType type)
{
    switch (type)
    {
    case BTN_TYPE_UP:
        return "UP";
    case BTN_TYPE_DOWN:
        return "DOWN";
    case BTN_TYPE_OK:
        return "OK";
    case BTN_TYPE_BACK:
        return "BACK";
    default:
        return "UNKNOWN";
    }
}