#include "NvsManager.h"

// Le premier argument "multi-bat" est le nom
// de la partition, et le second (false) signifie qu'elle est en lecture/écriture.
Preferences preferences;

// La clé sous laquelle nous allons stocker notre information.
const char *NVS_KEY_BAT_COUNT = "bat_count";

void initNvs()
{
    // Initialise la NVS. Doit être appelé une seule fois dans setup().
    preferences.begin("multi-bat", false);
    Serial.println("NVS initialisée.");
}

void saveBatteryCount(int count)
{
    // Ouvre la NVS et écrit la valeur 'count' sous la clé "bat_count".
    // On utilise putUChar car le nombre de batteries (0-9) tient sur un octet non signé.
    preferences.putUChar(NVS_KEY_BAT_COUNT, (uint8_t)count);
    Serial.printf("NVS: Nombre de batteries (%d) sauvegardé.\n", count);
}

int loadBatteryCount()
{
    // Lit la valeur associée à la clé "bat_count".
    // Le second argument (0) est la valeur par défaut à retourner si la clé n'existe pas.
    return (int)preferences.getUChar(NVS_KEY_BAT_COUNT, 0);
}