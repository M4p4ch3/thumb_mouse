
#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <inttypes.h>

#include "utils.h"

// (X, Y) mapped values ranges
#define X_OUT_MIN (-100)
#define X_OUT_MAX 100
#define Y_OUT_MIN (-100)
#define Y_OUT_MAX 100
#define X_OUT_CENTER (X_OUT_MIN + (X_OUT_MAX - X_OUT_MIN) / 2)
#define Y_OUT_CENTER (Y_OUT_MIN + (Y_OUT_MAX - Y_OUT_MIN) / 2)

typedef struct Controller_t
{
    uint32_t magic;
} Controller_t;

Controller_t * CONTROLLER_init(void);

uint8_t CONTROLLER_getJoy(Controller_t * pInst, Coord_t * pCoord);

#endif // CONTROLLER_H
