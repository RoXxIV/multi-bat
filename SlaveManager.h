#ifndef SLAVE_MANAGER_H
#define SLAVE_MANAGER_H

#include <Arduino.h>
#include "Config.h"
#include "BatteryLogic.h"
#include "ModbusLib.h" // Pour calculateCRC16

void initSlaveModbus();
void handleSlaveModbus();

#endif