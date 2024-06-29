
#ifndef UTILS_H
#define UTILS_H

#include <inttypes.h>

static const uint32_t MS_PER_S = 1000U;
static const uint32_t US_PER_MS = 1000U;
static const uint32_t US_PER_S = US_PER_MS * MS_PER_S;

typedef struct Coord_t
{
    int32_t x;
    int32_t y;
} Coord_t;

void UTILS_hang(void);

#endif // UTILS_H
