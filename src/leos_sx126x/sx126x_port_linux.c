#include "sx126x_priv.h"

#include <string.h>

uint64_t leos_sx126x_platform_now_ms(void)
{
    return 0u;
}

void leos_sx126x_platform_idle(void)
{
}

void leos_sx126x_link_handle(leos_sx126x_ctx_t *ctx)
{
    if (ctx == NULL)
    {
        return;
    }

    memset(&ctx->handle, 0, sizeof(ctx->handle));
}
