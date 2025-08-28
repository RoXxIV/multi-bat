// ModbusLib.cpp
#include "ModbusLib.h"

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

void modbus_print_buffer(const char *label, uint8_t *buffer, int length)
{
    Serial.printf("%s [%d bytes]: ", label, length);
    for (int i = 0; i < length; i++)
    {
        Serial.printf("%02X ", buffer[i]);
    }
    Serial.println();
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