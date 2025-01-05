
#ifndef TRACKBALL_H
#define TRACKBALL_H

#include <inttypes.h>

typedef struct Trackball_t
{
    uint32_t magic;
} Trackball_t;

Trackball_t * TRACKBALL_init(void);
uint8_t TRACKBALL_setColor(Trackball_t * pInst, uint8_t red, uint8_t green, uint8_t blue, uint8_t white);

#endif // TRACKBALL_H
