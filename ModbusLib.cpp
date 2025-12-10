// ModbusLib.cpp
#include "ModbusLib.h"

// --- AJOUTS DE LA STRATÉGIE "ProgB" ---
// Nombre de tentatives pour chaque commande Modbus
static const uint8_t MODBUS_MAX_RETRIES = 3;
// Délai long après une écriture 0x10 (laisse le temps à l'EEPROM du BMS)
static const uint16_t MODBUS_WRITE_DELAY_MS = 350;
// Délai court après une transaction 0x03 ou 0x06 réussie
static const uint16_t AFTER_SUCCESS_DELAY_MS = 5;
// Délai court entre les tentatives en cas d'échec
static const uint16_t RETRY_DELAY_MS = 2;
// --- FIN DES AJOUTS ---

static LoopCallback _modbus_idle_callback = nullptr;

uint16_t faultCode0_1;
uint16_t faultCode2_3;

void modbus_set_idle_callback(LoopCallback callback)
{
  _modbus_idle_callback = callback;
}

// Variables "privées" à la bibliothèque, non visibles de l'extérieur.
static HardwareSerial *_modbusSerial = nullptr;
static int _de_re_pin = -1;

void modbus_init(HardwareSerial *serial, int de_re_pin)
{
  _de_re_pin = de_re_pin;
  pinMode(_de_re_pin, OUTPUT);
  modbus_enable_receive(); // Mode réception par défaut

  _modbusSerial = serial;
  // La configuration du port (baud, etc.) reste dans ModbusManager pour l'instant
  // car elle peut être spécifique au projet.
}

void modbus_enable_transmit()
{
  digitalWrite(_de_re_pin, HIGH);
}

void modbus_enable_receive()
{
  digitalWrite(_de_re_pin, LOW);
}

int modbus_build_read_frame(uint8_t *buffer, uint8_t slave_id, uint16_t start_addr, uint16_t reg_count)
{
  buffer[0] = 0x80 + slave_id;
  buffer[1] = CMD_READ_HOLDING;
  buffer[2] = (start_addr >> 8) & 0xFF;
  buffer[3] = start_addr & 0xFF;
  buffer[4] = (reg_count >> 8) & 0xFF;
  buffer[5] = reg_count & 0xFF;

  uint16_t crc = calculateCRC16(buffer, 6);
  buffer[6] = crc & 0xFF;
  buffer[7] = (crc >> 8) & 0xFF;

  return 8;
}

uint16_t calculateCRC16(uint8_t *data, uint8_t length)
{
  uint16_t crc = 0xFFFF;

  for (int i = 0; i < length; i++)
  {
    crc ^= data[i];
    for (int j = 0; j < 8; j++)
    {
      if (crc & 0x0001)
      {
        crc >>= 1;
        crc ^= 0xA001;
      }
      else
      {
        crc >>= 1;
      }
    }
  }
  return crc;
}

int modbus_read_registers(uint8_t slave_id, uint16_t start_addr, uint16_t reg_count, uint8_t *dest_buffer)
{
  uint8_t frame[8];
  uint8_t response[256];

  // 1. Construire la trame de commande (une seule fois)
  modbus_build_read_frame(frame, slave_id, start_addr, reg_count);

  // --- AJOUT DE LA BOUCLE DE RÉESSAI ---
  for (int t = 0; t < MODBUS_MAX_RETRIES; t++)
  {
    // Vider le buffer de réception avant l'envoi
    while (_modbusSerial->available())
      _modbusSerial->read();

    // 2. Envoyer la commande
    modbus_enable_transmit();
    _modbusSerial->write(frame, 8);
    _modbusSerial->flush();
    delay(2);
    modbus_enable_receive();

    // 3. Attendre et recevoir la réponse
    unsigned long timeout = millis() + 400; // Timeout
    int responseLength = 0;
    while (millis() < timeout && responseLength < sizeof(response))
    {
      if (_modbusSerial->available())
      {
        response[responseLength++] = _modbusSerial->read();
        timeout = millis() + 50; // On prolonge le timeout à chaque octet reçu
      }
      if (_modbus_idle_callback != nullptr)
      {
        _modbus_idle_callback(); // Exécute les tâches de fond (boutons)
      }
    }

    if (responseLength == 0)
    {
      delay(RETRY_DELAY_MS); // Échec (Timeout), on réessaie
      continue;
    }

    // 4. Valider la réponse
    int expected_len = 5 + reg_count * 2;
    if (responseLength != expected_len)
    {
      delay(RETRY_DELAY_MS); // Échec (Taille incorrecte), on réessaie
      continue;
    }

    uint16_t receivedCrc = (response[responseLength - 1] << 8) | response[responseLength - 2];
    uint16_t calculatedCrc = calculateCRC16(response, responseLength - 2);

    if (receivedCrc != calculatedCrc)
    {
      delay(RETRY_DELAY_MS); // Échec (CRC incorrect), on réessaie
      continue;
    }

    // --- SUCCÈS ---
    // Si tout est OK, on copie les données utiles
    int payload_len = reg_count * 2;
    memcpy(dest_buffer, &response[3], payload_len);

    // AJOUT DE LA PAUSE DE 5ms
    delay(AFTER_SUCCESS_DELAY_MS);

    return payload_len; // Succès, on renvoie le nombre d'octets de données
  }

  // Si la boucle se termine sans succès
  return -1; // Échec après toutes les tentatives
}

bool modbus_write_register(uint8_t slave_id, uint16_t reg_addr, uint16_t value)
{
  uint8_t frame[8];
  // 1. Construire la trame d'écriture 0x06
  frame[0] = 0x80 + slave_id;
  frame[1] = CMD_WRITE_SINGLE;
  frame[2] = (reg_addr >> 8) & 0xFF;
  frame[3] = reg_addr & 0xFF;
  frame[4] = (value >> 8) & 0xFF;
  frame[5] = value & 0xFF;
  uint16_t crc = calculateCRC16(frame, 6);
  frame[6] = crc & 0xFF;
  frame[7] = (crc >> 8) & 0xFF;

  // --- AJOUT DE LA BOUCLE DE RÉESSAI ---
  for (int t = 0; t < MODBUS_MAX_RETRIES; t++)
  {
    // Vider le buffer de réception avant l'envoi
    while (_modbusSerial->available())
      _modbusSerial->read();

    // 2. Envoyer la commande
    modbus_enable_transmit();
    _modbusSerial->write(frame, 8);
    _modbusSerial->flush();
    delay(2);
    modbus_enable_receive();

    // 3. Attendre l'ACK (l'écho)
    uint8_t response[32];
    unsigned long timeout = millis() + 400;
    int n = 0;
    while (millis() < timeout && n < sizeof(response))
    {
      if (_modbusSerial->available())
      {
        response[n++] = _modbusSerial->read();
      }
      if (_modbus_idle_callback != nullptr)
      {
        _modbus_idle_callback(); // Exécute les tâches de fond (boutons)
      }
    }

    // L'ACK pour ce BMS est une copie, SAUF le premier octet qui est 0x50 + ID.
    if (n == 8)
    {
      if (response[0] == (0x50 + slave_id) && response[1] == frame[1] && // Function code
          response[2] == frame[2] &&                                     // Addr H
          response[3] == frame[3] &&                                     // Addr L
          response[4] == frame[4] &&                                     // Val H
          response[5] == frame[5])                                       // Val L
      {
        uint16_t crc_calc = calculateCRC16(response, 6);
        uint16_t crc_recv = (uint16_t)response[6] | ((uint16_t)response[7] << 8);
        if (crc_calc == crc_recv)
        {
          // --- SUCCÈS ---
          // AJOUT DE LA PAUSE DE 5ms
          delay(AFTER_SUCCESS_DELAY_MS);
          return true; // La réponse est valide !
        }
      }
    }

    // Si échec (timeout ou réponse incorrecte), petite pause avant de réesayer
    delay(RETRY_DELAY_MS);
  }

  return false; // Échec après toutes les tentatives
}

bool modbus_write_multiple_registers(uint8_t slave_id, uint16_t start_addr, uint16_t reg_count, uint8_t *data)
{
  uint8_t frame[256];
  uint8_t byte_count = reg_count * 2;

  // 1. Construire la trame d'écriture NON-STANDARD (sans le "Byte Count")
  frame[0] = 0x80 + slave_id;
  frame[1] = CMD_WRITE_MULTIPLE;
  frame[2] = (start_addr >> 8) & 0xFF;
  frame[3] = start_addr & 0xFF;
  frame[4] = (reg_count >> 8) & 0xFF;
  frame[5] = reg_count & 0xFF;
  // La ligne "frame[6] = byte_count;" est volontairement supprimée.

  // Les données commencent donc directement à l'index 6
  memcpy(&frame[6], data, byte_count);

  // La longueur de la trame avant le CRC est donc ajustée
  int frame_len = 6 + byte_count;
  uint16_t crc = calculateCRC16(frame, frame_len);
  frame[frame_len++] = crc & 0xFF;
  frame[frame_len++] = (crc >> 8) & 0xFF;

  // --- AJOUT DE LA BOUCLE DE RÉESSAI ---
  for (int t = 0; t < MODBUS_MAX_RETRIES; t++)
  {
    // 2. Envoyer la commande
    while (_modbusSerial->available())
      _modbusSerial->read();
    modbus_enable_transmit();
    _modbusSerial->write(frame, frame_len);
    _modbusSerial->flush();
    delay(2);
    modbus_enable_receive();

    // 3. Attendre et valider l'ACK
    uint8_t response[32];
    unsigned long timeout = millis() + 400;
    int n = 0;
    while (millis() < timeout && n < sizeof(response))
    {
      if (_modbusSerial->available())
      {
        response[n++] = _modbusSerial->read();
      }
      if (_modbus_idle_callback != nullptr)
      {
        _modbus_idle_callback(); // Exécute les tâches de fond (boutons)
      }
    }

    if (n == 8)
    {
      if (response[0] == (0x50 + slave_id) && response[1] == CMD_WRITE_MULTIPLE && response[2] == frame[2] && response[3] == frame[3] && response[4] == frame[4] && response[5] == frame[5])
      {
        uint16_t crc_calc = calculateCRC16(response, 6);
        uint16_t crc_recv = (uint16_t)response[6] | ((uint16_t)response[7] << 8);
        if (crc_calc == crc_recv)
        {
          // --- SUCCÈS ---
          // AJOUT DE LA STRATÉGIE "ProgB"
          delay(MODBUS_WRITE_DELAY_MS);
          delay(AFTER_SUCCESS_DELAY_MS);
          return true;
        }
      }
    }

    // Si échec (timeout ou réponse incorrecte), petite pause avant de réesayer
    delay(RETRY_DELAY_MS);
  }
  return false; // Échec après toutes les tentatives
}