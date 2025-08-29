// ModbusLib.h
#ifndef MODBUS_LIB_H
#define MODBUS_LIB_H

#include <Arduino.h>
#include <HardwareSerial.h>

// ——————— CONSTANTES DU PROTOCOLE BMS ———————

// Commandes Modbus standard
#define CMD_READ_HOLDING 0x03   // Lecture de registres de maintien
#define CMD_WRITE_SINGLE 0x06   // Écriture d'un registre simple
#define CMD_WRITE_MULTIPLE 0x10 // Écriture de registres multiples

// Registres de données temps réel spécifiques
#define REG_CELL_VOLTAGES_START 0x00   // Début tensions cellules (0x00~0x2F)
#define REG_TEMPERATURES_START 0x30    // Début températures capteurs (0x30~0x37)
#define REG_TOTAL_VOLTAGE 0x38         // Tension totale de la batterie
#define REG_CURRENT 0x39               // Courant charge/décharge
#define REG_SOC 0x3A                   // SOC
#define REG_HEARTBEAT 0x3B             // Heartbeat
#define REG_CELL_COUNT 0x3C            // Nombre de cellules
#define REG_TEMP_SENSOR_COUNT 0x3D     // Nombre de capteurs de température
#define REG_CHARGE_MOSFET 0x52         // État MOSFET de charge
#define REG_DISCHARGE_MOSFET 0x53      // État MOSFET de décharge
#define REG_MOS_TEMP 0x5A              // Température des MOSFET
#define REG_FAULT_STATUS1 0x66         // État des défauts groupe 1
#define REG_FAULT_STATUS2 0x67         // État des défauts groupe 2
#define REG_FAULT_STATUS3 0x68         // État desodefauts groupe 3
#define REG_SERIAL_NUMBER_START 0x018D // Début du numéro de série
#define REG_SERIAL_NUMBER_COUNT 14     // Nombre de registres pour le S/N

// Commandes d'affichage sur LCD batterie
#define DISPLAY_ID_CMD 7 // Commande pour afficher ID
#define ASCII_7 0x37     // Valeur ASCII pour "7"

// ——————— FONCTIONS DE LA BIBLIOTHÈQUE MODBUS ———————

/**
 * @brief Initialise la communication Modbus sur un port série donné.
 * @param serial Pointeur vers l'objet HardwareSerial (ex: &Serial2).
 * @param de_re_pin Le pin utilisé pour le contrôle de direction du transceiver RS485.
 */
void modbus_init(HardwareSerial *serial, int de_re_pin);

/** @brief Active le mode émission du transceiver RS485. */
void modbus_enable_transmit();

/** @brief Active le mode réception du transceiver RS485. */
void modbus_enable_receive();

/**
 * @brief Construit une trame Modbus de lecture (fonction 0x03).
 * @param buffer Le tableau d'octets où écrire la trame.
 * @param slave_id L'ID de l'esclave à interroger.
 * @param start_addr L'adresse du premier registre à lire.
 * @param reg_count Le nombre de registres à lire.
 * @return La longueur de la trame construite (toujours 8).
 */
int modbus_build_read_frame(uint8_t *buffer, uint8_t slave_id, uint16_t start_addr, uint16_t reg_count);

/**
 * @brief Affiche le contenu d'un buffer en hexadécimal pour le débogage.
 * @param label Un texte à afficher avant le buffer.
 * @param buffer Le buffer à afficher.
 * @param length La longueur du buffer.
 */
void modbus_print_buffer(const char *label, uint8_t *buffer, int length);

/**
 * @brief Calcule le CRC16 Modbus d'un buffer de données.
 * * @param data Pointeur vers le tableau de données.
 * @param length Nombre d'octets dans le tableau.
 * @return Le CRC16 calculé.
 */
uint16_t calculateCRC16(uint8_t *data, uint8_t length);

/**
 * @brief Exécute une transaction Modbus de lecture complète (envoi + réception + validation).
 * @param slave_id L'ID de l'esclave à interroger.
 * @param start_addr L'adresse du premier registre à lire.
 * @param reg_count Le nombre de registres à lire.
 * @param dest_buffer Le buffer où seront stockées les données utiles de la réponse.
 * @return Le nombre d'octets de données reçus, ou -1 en cas d'erreur (timeout, CRC...).
 */
int modbus_read_registers(uint8_t slave_id, uint16_t start_addr, uint16_t reg_count, uint8_t *dest_buffer);

/**
 * @brief Exécute une transaction Modbus d'écriture d'un seul registre.
 * @param slave_id L'ID de l'esclave.
 * @param reg_addr L'adresse du registre à écrire.
 * @param value La valeur de 16 bits à écrire.
 * @return true si l'écriture a été confirmée par un ACK, false sinon.
 */
bool modbus_write_register(uint8_t slave_id, uint16_t reg_addr, uint16_t value);

/**
 * @brief Exécute une transaction Modbus d'écriture de registres multiples.
 * @param slave_id L'ID de l'esclave.
 * @param start_addr L'adresse du premier registre à écrire.
 * @param reg_count Le nombre de registres à écrire.
 * @param data Pointeur vers un tableau d'octets contenant les données à écrire.
 * @return true si l'écriture a été confirmée par un ACK, false sinon.
 */
bool modbus_write_multiple_registers(uint8_t slave_id, uint16_t start_addr, uint16_t reg_count, uint8_t *data);

#endif