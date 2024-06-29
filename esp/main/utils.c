
#include "freertos/FreeRTOS.h"

void UTILS_hang(void)
{
    while (1)
    {
        vTaskDelay(100U / portTICK_PERIOD_MS);
    }
}
