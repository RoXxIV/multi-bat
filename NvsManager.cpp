#include "NvsManager.h"

// Le premier argument "multi-bat" est le nom
// de la partition, et le second (false) signifie qu'elle est en lecture/écriture.
Preferences preferences;

// La clé sous laquelle nous allons stocker notre information.
const char *NVS_KEY_BAT_COUNT = "bat_count";

void initNvs()
{
  preferences.begin("multi-bat", false);
}

void saveBatteryCount(int count)
{
  // Ouvre la NVS et écrit la valeur 'count' sous la clé "bat_count".
  preferences.putUChar(NVS_KEY_BAT_COUNT, (uint8_t)count);
}

int loadBatteryCount()
{
  // Le second argument (0) est la valeur par défaut à retourner si la clé n'existe pas.
  return (int)preferences.getUChar(NVS_KEY_BAT_COUNT, 0);
}