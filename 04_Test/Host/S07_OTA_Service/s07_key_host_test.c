#include <stdio.h>

#include "platform_key.h"

static platform_key_id_t g_receivedKey = PLATFORM_KEY_ID_MAX;
static uint32_t g_receivedCount;

static void test_key_callback(platform_key_id_t key, void *context)
{
    uint32_t *count = (uint32_t *)context;

    g_receivedKey = key;
    *count += 1U;
}

static int test_key_dispatch(void)
{
    g_receivedCount = 0U;
    g_receivedKey = PLATFORM_KEY_ID_MAX;

    if (platform_key_init(test_key_callback, &g_receivedCount) != PLATFORM_ERR_OK) {
        return 1;
    }

    if (platform_key_event_from_isr(PLATFORM_KEY_ID_1) != PLATFORM_ERR_OK) {
        return 1;
    }

    return (g_receivedKey == PLATFORM_KEY_ID_1) &&
                   (g_receivedCount == 1U) ? 0 : 1;
}

int main(void)
{
    if (test_key_dispatch() != 0) {
        (void)printf("S07 Platform Key host test failed.\n");
        return 1;
    }

    (void)printf("S07 Platform Key host test passed.\n");
    return 0;
}
