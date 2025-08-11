#include "ButtonManager.h"

// ——————— MÉTHODES ButtonState ———————
bool ButtonState::pressed()
{
    return currentState && !previousState; // front montant logique d'appui
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

    // Configuration des pins (même logique que l’alpha : lecture directe, pas de pullup ici)
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

    Serial.println("ButtonManager initialisé (alpha-like):");
    Serial.printf("  UP:   pin %d\n", upPin);
    Serial.printf("  DOWN: pin %d\n", downPin);
    Serial.printf("  OK:   pin %d\n", okPin);
    Serial.printf("  BACK: pin %d\n", backPin);
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
    bool raw = digitalRead(btn.pin); // alpha = lecture directe (HIGH = appui)

    // EXACTEMENT comme l’alpha : on fige l’état précédent AVANT le filtrage
    btn.previousState = btn.currentState;

    // Débounce "naïf" de l’alpha :
    // - si la fenêtre est dépassée, on prend la lecture brute
    // - si la lecture est HAUTE (appui), on relance la fenêtre
    if (now - btn.lastDebounce > debounceDelay)
    {
        btn.currentState = raw;
        if (raw)
            btn.lastDebounce = now;
    }

    // Log optionnel (décommente si tu veux voir les fronts)
    // if (btn.pressed()) {
    //     Serial.printf("Bouton %s appuyé\n", getButtonName(buttonType));
    // }
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
