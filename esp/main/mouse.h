
#ifndef MOUSE_H
#define MOUSE_H

#include <inttypes.h>

typedef struct Mouse_t
{
    uint32_t magic;
    uint8_t bEn;
} Mouse_t;

Mouse_t * MOUSE_init(uint8_t bEn);

void MOUSE_setEnabled(Mouse_t * pInst, uint8_t bEn);
uint8_t MOUSE_getEnabled(Mouse_t * pInst);

uint8_t MOUSE_move(Mouse_t * pInst, int8_t x, int8_t y);

#endif // MOUSE_H
