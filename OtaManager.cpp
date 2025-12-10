#include "OtaManager.h"
#include "DisplayManager.h"

// ——————— VARIABLES GLOBALES ———————
bool otaServerActive = false;
unsigned long otaStartTime = 0;
bool otaInProgress = false;
int otaProgress = 0;

// Configuration Point d'Accès WiFi
const char *AP_SSID = "multibatt";
const char *AP_PASSWORD = "VotreCodePro2024"; // pass wifi
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
  showMessage("OTA", "Creation AP WiFi...");

  // Configuration du point d'accès
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(AP_IP, AP_GATEWAY, AP_SUBNET);

  bool success = WiFi.softAP(AP_SSID, AP_PASSWORD);

  if (success)
  {
    return true;
  }
  else
  {
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

  // Configuration ArduinoOTA
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.onStart([]()
                     {
        otaInProgress = true;
    otaProgress = 0;

    clearDisplay();
    drawTitle("MISE A JOUR OTA");
    drawText(5, 30, "Debut maj...");
    showDisplay(); });

  ArduinoOTA.onEnd([]()
                   {
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
    }
  }
}

String getOTAInfo()
{
  if (otaServerActive)
  {
    int clients = WiFi.softAPgetStationNum();
    return String("SSID: ") + AP_SSID + String("\nIP: ") + WiFi.softAPIP().toString() + String("\nClients: ") + String(clients) + String("\nNom: ") + OTA_HOSTNAME;
  }
  return "Point d'accès inactif";
}