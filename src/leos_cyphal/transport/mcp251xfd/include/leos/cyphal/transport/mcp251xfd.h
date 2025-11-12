#pragma once
#include "leos/cyphal/transport.h"
#include <MCP251XFD.h>

leos_cyphal_transport_t leos_cyphal_transport_mcp251xfd(MCP251XFD *dev);