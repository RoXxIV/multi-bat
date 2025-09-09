#include "OtaManager.h"
#include "DisplayManager.h"

// ——————— VARIABLES GLOBALES ———————
bool otaServerActive = false;
unsigned long otaStartTime = 0;
bool otaInProgress = false;
int otaProgress = 0;

// Configuration Point d'Accès WiFi PROFESSIONNEL
const char *AP_SSID = "multibatt";            // Standard sur toutes vos cartes client
const char *AP_PASSWORD = "VotreCodePro2024"; // Mot de passe (à définir)
const IPAddress AP_IP(192, 168, 4, 1);        // IP fixe du point d'accès
const IPAddress AP_GATEWAY(192, 168, 4, 1);
const IPAddress AP_SUBNET(255, 255, 255, 0);

const char *OTA_HOSTNAME = "multi-battery"; // Nom technique sur réseau
const char *OTA_PASSWORD = "update_pro";    // Authentification OTA renforcée

// ——————— FONCTIONS MODIFIÉES ———————

void initOTA()
{
    Serial.println("Système OTA Point d'Accès initialisé");
}

bool connectToWiFi()
{
    // Cette fonction crée maintenant un point d'accès au lieu de se connecter
    Serial.printf("Création du point d'accès WiFi '%s'...\n", AP_SSID);
    showMessage("OTA", "Creation AP WiFi...");

    // Configuration du point d'accès
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(AP_IP, AP_GATEWAY, AP_SUBNET);

    bool success = WiFi.softAP(AP_SSID, AP_PASSWORD);

    if (success)
    {
        Serial.printf("Point d'accès créé!\n");
        Serial.printf("SSID: %s\n", AP_SSID);
        Serial.printf("IP: %s\n", WiFi.softAPIP().toString().c_str());
        Serial.printf("Mot de passe: %s\n", AP_PASSWORD);
        return true;
    }
    else
    {
        Serial.println("Échec création point d'accès");
        return false;
    }
}

bool startOTAServer()
{
    if (otaServerActive)
    {
        return true;
    }

    if (!connectToWiFi())
    {
        return false;
    }

    // Configuration ArduinoOTA (identique)
    ArduinoOTA.setHostname(OTA_HOSTNAME);
    ArduinoOTA.setPassword(OTA_PASSWORD);

    // Callbacks identiques à la version précédente...
    ArduinoOTA.onStart([]()
                       {
        Serial.println("Début mise à jour OTA");
        otaInProgress = true;
        otaProgress = 0;
        
        clearDisplay();
        drawTitle("MISE A JOUR OTA");
        drawText(5, 30, "Debut maj...");
        showDisplay(); });

    ArduinoOTA.onEnd([]()
                     {
        Serial.println("Mise à jour terminée");
        otaInProgress = false;
        
        clearDisplay();
        drawTitle("MISE A JOUR OTA");
        drawText(5, 30, "Maj terminee!");
        drawText(5, 40, "Redemarrage...");
        showDisplay();
        delay(2000); });

    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
                          {
        int percentage = (progress / (total / 100));
        otaProgress = percentage;
        
        if (percentage % 10 == 0) {
            clearDisplay();
            drawTitle("MISE A JOUR OTA");
            
            char progressStr[20];
            sprintf(progressStr, "Progression: %d%%", percentage);
            drawText(5, 30, progressStr);
            
            int barWidth = percentage;
            drawFrame(10, 40, 100, 8);
            if (barWidth > 0) {
                display_u8g2->drawBox(11, 41, barWidth, 6);
            }
            
            drawText(5, 55, "Ne pas eteindre!");
            showDisplay();
        } });

    ArduinoOTA.onError([](ota_error_t error)
                       {
        otaInProgress = false;
        clearDisplay();
        drawTitle("ERREUR OTA");
        drawText(5, 30, "Erreur mise a jour");
        showDisplay();
        delay(3000); });

    ArduinoOTA.begin();
    otaServerActive = true;
    otaStartTime = millis();

    Serial.println("Serveur OTA démarré en mode Point d'Accès");
    Serial.printf("  - SSID: %s\n", AP_SSID);
    Serial.printf("  - IP: %s\n", WiFi.softAPIP().toString().c_str());
    Serial.printf("  - Hostname: %s\n", OTA_HOSTNAME);

    return true;
}

void stopOTAServer()
{
    if (otaServerActive)
    {
        ArduinoOTA.end();
        WiFi.softAPdisconnect(true); // Arrêter le point d'accès
        otaServerActive = false;
        otaInProgress = false;
        Serial.println("Point d'accès et serveur OTA arrêtés");
    }
}

void handleOTAProcess()
{
    if (otaServerActive)
    {
        ArduinoOTA.handle();

        // Arrêt automatique après 10 minutes
        if (!otaInProgress && (millis() - otaStartTime) > 600000)
        {
            stopOTAServer();
            Serial.println("OTA timeout - serveur arrêté");
        }
    }
}

String getWiFiStatus()
{
    if (otaServerActive)
    {
        int clientCount = WiFi.softAPgetStationNum();
        return String("AP actif (") + String(clientCount) + String(" clients)");
    }
    return "AP inactif";
}

String getOTAInfo()
{
    if (otaServerActive)
    {
        int clients = WiFi.softAPgetStationNum();
        return String("SSID: ") + AP_SSID +
               String("\nIP: ") + WiFi.softAPIP().toString() +
               String("\nClients: ") + String(clients) +
               String("\nNom: ") + OTA_HOSTNAME;
    }
    return "Point d'accès inactif";
}