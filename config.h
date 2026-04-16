#pragma once
#include "leos/mcp251xfd/config.h"

// CAN DEFINITIONS - SUBJECT TO MOVE
#define CAN_ID_RADIO_MODULE 11
#define CAN_ID_SENSOR_MODULE 12


#define CAN_TOPIC_BARO_PRESSURE 135

extern leos_mcp251xfd_hw_t can_hw_config;
extern leos_mcp251xfd_config_t can_config;

