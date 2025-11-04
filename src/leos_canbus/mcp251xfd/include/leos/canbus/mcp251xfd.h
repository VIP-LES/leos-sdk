#pragma once
#include "leos/canbus.h"
#include "MCP251XFD.h"

leos_canbus_transport_t leos_canbus_transport_mcp251xfd(MCP251XFD *dev);