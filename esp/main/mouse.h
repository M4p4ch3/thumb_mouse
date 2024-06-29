
#ifndef MOUSE_H
#define MOUSE_H

#include <inttypes.h>

typedef struct Mouse_t
{
    uint32_t magic;
} Mouse_t;

Mouse_t * MOUSE_init(void);

void MOUSE_move(Mouse_t * pInst, int8_t x, int8_t y);

#endif // MOUSE_H
