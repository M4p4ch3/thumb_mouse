
#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <inttypes.h>

#include "freertos/semphr.h"

#include "utils.h"
// TODO remove mouse from ctrl
#include "mouse.h"

typedef struct Controller_t
{
    uint32_t magic;
    esp_timer_handle_t timer;
    SemaphoreHandle_t semSendHid;
} Controller_t;

Controller_t * CONTROLLER_init(void);

// TODO remove mouse from ctrl
void CONTROLLER_setMouse(Mouse_t * pMouse);

uint8_t CONTROLLER_start(Controller_t * pInst);
uint8_t CONTROLLER_stop(Controller_t * pInst);

// uint8_t CONTROLLER_getButton(Controller_t * pInst, uint8_t btnIdx);
// void CONTROLLER_getJoystick(Controller_t * pInst, uint8_t joyIdx, Coord_t * pCoord);

#endif // CONTROLLER_H
