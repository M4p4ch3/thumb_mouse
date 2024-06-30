
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "driver/adc.h"

#include "config.h"
#include "logger.h"

#include "controller.h"

static const uint32_t MAGIC = 561348;

static void __attribute__((format (printf, 2, 3))) _log(LogLevel_e lvl, const char * sFmt, ...);

static void _log(LogLevel_e lvl, const char * sFmt, ...)
{
    va_list pArg;

    va_start(pArg, sFmt);
    LOGGER_log_va(MODULE_ID_CTRL, lvl, sFmt, pArg);
    va_end(pArg);
}

static int32_t map(int32_t in, int32_t in_min, int32_t in_max, int32_t out_min, int32_t out_max)
{
    if (in < in_min)
    {
        return out_min;
    }

    if (in > in_max)
    {
        return out_max;
    }

    return (in - in_min) * (out_max - out_min) / in_max + out_min;
}

Controller_t * CONTROLLER_init(void)
{
    Controller_t * pInst = NULL;

    LOGGER_setLevel(MODULE_ID_CTRL, LOG_LVL_DEBUG);

    _log(LOG_LVL_DEBUG, "%s()", __func__);

    _log(LOG_LVL_DEBUG, "%s() Configure ADC", __func__);
    adc1_config_width(ADC_WIDTH_BIT_13);
    adc1_config_channel_atten(JOY_HW_X_CHAN, ADC_ATTEN_DB_0);
    adc1_config_channel_atten(JOY_HW_Y_CHAN, ADC_ATTEN_DB_0);

    pInst = (Controller_t *) malloc(sizeof(Controller_t));
    if (!pInst)
    {
        _log(LOG_LVL_ERROR, "%s() malloc %u Bytes for Controller_t FAILED", __func__, sizeof(Controller_t));
        return NULL;
    }

    pInst->magic = MAGIC;

    return pInst;
}

uint8_t CONTROLLER_getJoy(Controller_t * pInst, Coord_t * pCoord)
{
    static uint16_t callCnt = 0U;

    if (!pInst || pInst->magic != MAGIC)
    {
        _log(LOG_LVL_ERROR, "%s() Bad instance pointer", __func__);
        return 1U;
    }

    if (!pCoord)
    {
        _log(LOG_LVL_ERROR, "%s() pCoord NULL", __func__);
        return 1U;
    }

    pCoord->x = 0;
    pCoord->y = 0;

    for (uint8_t acq_idx = 0U; acq_idx < ACQ_NB; acq_idx += 1U)
    {
        pCoord->x += (int32_t) adc1_get_raw(JOY_HW_X_CHAN);
        pCoord->y += (int32_t) adc1_get_raw(JOY_HW_Y_CHAN);
    }

    pCoord->x /= ACQ_NB;
    pCoord->y /= ACQ_NB;

    if ((CTRL_LOG_LOOP_NB < 0xFF) && (callCnt == CTRL_LOG_LOOP_NB))
    {
        _log(LOG_LVL_DEBUG, "(raw) x = %04ld, y = %04ld", pCoord->x, pCoord->y);
    }

    if (pCoord->x < JOY_X_CENTER)
    {
        pCoord->x = (int8_t) map(pCoord->x, JOY_X_MIN, JOY_X_CENTER - JOY_X_DEADZONE / 2, X_OUT_MIN, X_OUT_CENTER);
    }
    else
    {
        pCoord->x = (int8_t) map(pCoord->x, JOY_X_CENTER - JOY_X_DEADZONE / 2, JOY_X_MAX, X_OUT_CENTER, X_OUT_MAX);
    }

    if (pCoord->y < JOY_Y_CENTER)
    {
        pCoord->y = (int8_t) map(pCoord->y, JOY_Y_MIN, JOY_Y_CENTER - JOY_Y_DEADZONE / 2, Y_OUT_MIN, Y_OUT_CENTER);
    }
    else
    {
        pCoord->y = (int8_t) map(pCoord->y, JOY_Y_CENTER - JOY_Y_DEADZONE / 2, JOY_Y_MAX, Y_OUT_CENTER, Y_OUT_MAX);
    }

    if (JOY_X_SIGN < 0)
    {
        pCoord->x = -pCoord->x;
    }

    if (JOY_Y_SIGN < 0)
    {
        pCoord->y = -pCoord->y;
    }

    if ((CTRL_LOG_LOOP_NB < 0xFF) && (callCnt == CTRL_LOG_LOOP_NB))
    {
        _log(LOG_LVL_DEBUG, "(map) x = %04ld, y = %04ld", pCoord->x, pCoord->y);
        callCnt = 0U;
    }

    callCnt += 1U;

    return 0U;
}
