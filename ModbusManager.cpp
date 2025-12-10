#include "ModbusManager.h"
#include "ModbusLib.h"
#include "DisplayManager.h"

// ——————— VARIABLES GLOBALES ———————
extern HardwareSerial modbusSerial;
uint8_t sendBuffer[256];
uint8_t receiveBuffer[256];

// ——————— FONCTIONS D'INITIALISATION ———————
void initModbus()
{
  // On passe l'adresse de l'objet global à notre librairie
  modbus_init(&modbusSerial, MODBUS_DE_RE_PIN);
  // On initialise directement l'objet global
  modbusSerial.begin(BAUD_RATE, SERIAL_8N1, MODBUS_RX_PIN, MODBUS_TX_PIN);

  memset(sendBuffer, 0, sizeof(sendBuffer));
  memset(receiveBuffer, 0, sizeof(receiveBuffer));
}

void startModbus()
{
  if (!modbusSerial)
  {
    modbusSerial.begin(BAUD_RATE, SERIAL_8N1, MODBUS_RX_PIN, MODBUS_TX_PIN);
  }
}

void stopModbus()
{
  if (modbusSerial)
  {
    modbusSerial.end();
  }
}

bool processNextModbusRead(uint8_t batteryId, int &readStep)
{
  // Note : L'ID 1 est réservé. L'index 0 de individualBatteryMetrics est inutilisé.
  // batteryId = 2 -> index 1, batteryId = 3 -> index 2
  IndividualBatteryData *data = &individualBatteryMetrics[batteryId - 1];

  uint8_t data_payload[4]; // Buffer pour 2 registres max (pour la capacité)
  int bytesRead = -1;
  uint16_t regAddr = 0;
  uint16_t regCount = 1;

  // Sélectionne l'adresse à lire en fonction de l'étape
  switch (readStep)
  {
  case 0:
    regAddr = 0x0038;
    break; // Tension totale
  case 1:
    regAddr = 0x0039;
    break; // Courant
  case 2:
    regAddr = 0x003A;
    break; // SOC
  case 3:
    regAddr = 0x0040;
    break; // Tension min cellule
  case 4:
    regAddr = 0x003E;
    break; // Tension max cellule
  case 5:
    regAddr = 0x0042;
    break; // Différence tension cellule
  case 6:
    regAddr = 0x0043;
    break; // Température max cellule
  case 7:
    regAddr = 0x0045;
    break; // Température min cellule
  case 8:
    regAddr = 0x0052;
    break; // État MOSFET Charge
  case 9:
    regAddr = 0x0053;
    break; // État MOSFET Décharge
  case 10:
    regAddr = 0x006B;
    break; // Source de réveil
  case 11:
    regAddr = 0x0140;
    break; // Limite de courant de charge L1
  case 12:
    regAddr = 0x0145;
    break; // Limite de courant de décharge L1
  case 13:
    regAddr = 0x0109;
    regCount = 2;
    break; // Capacité nominale (2 registres)
  case 14:
    regAddr = 0x006D;
    regCount = 1;
    break; // Fault Code 0-1
  case 15:
    regAddr = 0x006E;
    regCount = 1;
    break; // Fault Code 2-3
  case 16:
    regAddr = 0x0132; // MODBUS_OVER_VOLT_TWO_RESTORE
    regCount = 1;
    break; // Seuil Surtension L2 (mV)
  case 17:
    regAddr = 0x0136; // MODBUS_LESS_VOLT_TWO_RESTORE
    regCount = 1;
    break; // Seuil Sous-tension L2 (mV)
  default:
    return false; // Étape inconnue
  }

  // --- LECTURE MODBUS ---
  // (Cette fonction contient déjà la logique de 3 tentatives et la pause de 5ms)
  bytesRead = modbus_read_registers(batteryId, regAddr, regCount, data_payload);

  // --- DÉCODAGE ---
  if (bytesRead > 0)
  {
    // La lecture a réussi, on décode et on stocke
    uint16_t rawValue = (data_payload[0] << 8) | data_payload[1];

    switch (readStep)
    {
    case 0: // Tension totale
      data->voltage = rawValue / 10.0f;
      break;
    case 1: // Courant
      // data->current = (((int16_t)rawValue) - 30000) * 0.1f;
      {
        int32_t signedRaw = (int32_t)rawValue - 30000;
        data->current = signedRaw * 0.1f;
      }
      break;
    case 2: // SOC
      data->soc = rawValue / 10.0f;
      break;
    case 3: // Tension min cellule
      data->minCellVoltage = rawValue / 1000.0f;
      break;
    case 4: // Tension max cellule
      data->maxCellVoltage = rawValue / 1000.0f;
      break;
    case 5: // Différence tension cellule
      data->cellVoltageDifference = rawValue / 1000.0f;
      break;
    case 6: // Température max cellule
      data->maxCellTemp = ((float)rawValue) - 40.0f;
      break;
    case 7: // Température min cellule
      data->minCellTemp = ((float)rawValue) - 40.0f;
      break;
    case 8: // État MOSFET Charge
      data->chargeMosfetStatus = (rawValue & 0x01) != 0;
      break;
    case 9: // État MOSFET Décharge
      data->dischargeMosfetStatus = (rawValue & 0x01) != 0;
      break;
    case 10: // Source de réveil
      data->wakeUpSource = rawValue;
      break;
    case 11: // Limite de courant de charge L1
    {
      int32_t signedRaw = (int32_t)rawValue - 30000;
      data->chargeCurrentLimitL1 = signedRaw * 0.1f;
    }
    break;
    case 12: // Limite de courant de décharge L1
    {
      int32_t signedRaw = (int32_t)rawValue - 30000;
      data->dischargeCurrentLimitL1 = signedRaw * 0.1f;
    }
    break;
    case 13: // Capacité nominale (32 bits)
    {
      uint16_t rated_high = rawValue;
      uint16_t rated_low = (data_payload[2] << 8) | data_payload[3];
      uint32_t rated_capacity_raw = ((uint32_t)rated_high << 16) | rated_low;
      data->ratedCapacity = rated_capacity_raw / 1000.0f;
      data->soh = 100.0f; // SOH fixe
      break;
    }
    case 14: // Fault Code 0-1
      data->faultCode0_1 = rawValue;
      break;
    case 15: // Fault Code 2-3
      data->faultCode2_3 = rawValue;
      break;
    case 16: // Seuil Surtension L2 (mV)
      data->overVoltL2Threshold_mV = rawValue;
      break;
    case 17: // Seuil Sous-tension L2 (mV)
      data->underVoltL2Threshold_mV = rawValue;
      break;
    }

    readStep++;  // On passe à l'étape suivante
    return true; // Succès
  }
  else
  {
    // La lecture a échoué (après 3 tentatives)
    return false; // Échec
  }
}

// ——————— LECTURE INFOS STATIQUES ———————
void updateBatteryStaticInfo(uint8_t batteryId)
{
  IndividualBatteryData *batteryData = &individualBatteryMetrics[batteryId - 1];

  // On utilise un buffer local pour la réponse
  uint8_t data_payload[REG_SERIAL_NUMBER_COUNT * 2];

  int bytesRead = modbus_read_registers(
      batteryId,
      REG_SERIAL_NUMBER_START,
      REG_SERIAL_NUMBER_COUNT,
      data_payload);

  if (bytesRead > 0)
  {
    // La librairie a déjà tout validé, on peut utiliser les données directement !
    memcpy(batteryData->serialNumber, data_payload, bytesRead);
    batteryData->serialNumber[bytesRead] = '\0';
  }
  else
  {
  }
}

// ——————— FONCTIONS D'ÉCRITURE  ———————
bool sendDisplayIdToBattery(uint8_t batteryId, uint8_t asciiValue)
{
  // Préparation des données (4 registres = 8 octets)
  uint8_t payload[8] = {0};
  payload[0] = asciiValue; // On met la valeur ASCII dans le premier octet

  // Appel à la librairie pour l'écriture de 4 registres à l'adresse 0x01F1
  bool success = modbus_write_multiple_registers(batteryId, 0x01F1, 4, payload);

  if (success)
  {
  }
  else
  {
  }
  return success;
}

bool changeBatteryIdTo1(uint8_t batteryId)
{
  // On appelle la librairie pour écrire la valeur 1 dans le registre 0x0100
  bool success = modbus_write_register(batteryId, 0x0100, 1);

  if (success)
  {
  }
  else
  {
  }
  return success;
}

bool changeBatteryIdFrom1To(uint8_t newId)
{
  // L'adresse du registre de l'ID est 0x0100
  bool success = modbus_write_register(1, 0x0100, newId);

  if (success)
  {
  }
  else
  {
  }
  return success;
}

bool changeAllBatteriesToId1()
{
  bool globalSuccess = true;

  for (uint8_t batteryId = 1; batteryId <= MAX_BATTERIES; batteryId++)
  {
    bool result = changeBatteryIdTo1(batteryId);
    if (!result)
    {
      globalSuccess = false;
    }

    delay(2000); // Délai entre chaque batterie
  }

  if (globalSuccess)
  {
  }
  else
  {
  }

  return globalSuccess;
}
