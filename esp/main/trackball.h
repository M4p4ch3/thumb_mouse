
#ifndef TRACKBALL_H
#define TRACKBALL_H

#include <inttypes.h>
#include <stdbool.h>

#include "freertos/semphr.h"

#undef CONFIG_TRACKBALL_IT
#undef CONFIG_LOG_I2C

typedef struct Trackball_t
{
    uint32_t magic;
#ifdef CONFIG_TRACKBALL_IT
    SemaphoreHandle_t semIt;
#endif
} Trackball_t;

Trackball_t * TRACKBALL_init(void);
uint8_t TRACKBALL_reset(Trackball_t * pInst);
uint8_t TRACKBALL_getData(Trackball_t * pInst, int8_t * axisX, int8_t * axisY, bool * bSwitch);
uint8_t TRACKBALL_setColor(Trackball_t * pInst, uint8_t red, uint8_t green, uint8_t blue, uint8_t white);

#endif // TRACKBALL_H
