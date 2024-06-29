
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_timer.h"
#include "driver/adc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "config.h"
#include "logger.h"
// TODO remove mouse from ctrl
#include "mouse.h"

#include "controller.h"

// (X, Y) mapped values ranges
#define X_OUT_MIN (-100)
#define X_OUT_MAX 100
#define Y_OUT_MIN (-100)
#define Y_OUT_MAX 100
#define X_OUT_CENTER (X_OUT_MIN + (X_OUT_MAX - X_OUT_MIN) / 2)
#define Y_OUT_CENTER (Y_OUT_MIN + (Y_OUT_MAX - Y_OUT_MIN) / 2)

// TODO remove mouse from ctrl
static const uint32_t MOUSE_REPORT_PERIOD_US = US_PER_S / MOUSE_REPORT_FREQ_HZ;

static const uint32_t MAGIC = 561348;

// TODO remove mouse from ctrl
static Mouse_t * g_pMouse = NULL;

static void __attribute__((format (printf, 2, 3))) _log(LogLevel_e lvl, const char * sFmt, ...);

static void _log(LogLevel_e lvl, const char * sFmt, ...)
{
    va_list pArg;

    va_start(pArg, sFmt);
    LOGGER_log_va(MODULE_ID_CTRL, lvl, sFmt, pArg);
    va_end(pArg);
}

static void print_val_report(int32_t x_raw, int32_t y_raw, int32_t x_map, int32_t y_map, int32_t mouse_x, int32_t mouse_y)
{
    const char * s_x_pfx = "";
    const char * s_y_pfx = "";

    printf("x_raw   = %04ld, y_raw   = %04ld\n", x_raw, y_raw);

    if (x_map >= 0)
        s_x_pfx = " ";
    if (y_map >= 0)
        s_y_pfx = " ";
    printf("x_map   =  %s%02ld, y_map   =  %s%02ld\n", s_x_pfx, x_map, s_y_pfx, y_map);

    s_x_pfx = "";
    s_y_pfx = "";
    if (mouse_x >= 0)
        s_x_pfx = " ";
    if (mouse_y >= 0)
        s_y_pfx = " ";
    printf("mouse_x =  %s%02ld, mouse_y =  %s%02ld\n", s_x_pfx, mouse_x, s_y_pfx, mouse_y);

    printf("\n");
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

static void timerCb(void * pArg)
{
    BaseType_t ret = pdFALSE;
    Controller_t * pInst;

    if (!pArg)
    {
        _log(LOG_LVL_ERROR, "%s() pArg NULL", __func__);
        return;
    }

    pInst = (Controller_t *) pArg;

    ret = xSemaphoreGive(pInst->semSendHid);
    if (ret != pdTRUE)
    {
        _log(LOG_LVL_ERROR, "%s() xSemaphoreGive FAILED", __func__);
        return;
    }
}

static void _main(void * pArg)
{
    BaseType_t ret = pdFALSE;
    uint16_t loop_cnt = 0U;
    Controller_t * pInst = NULL;

    _log(LOG_LVL_DEBUG, "%s()", __func__);

    if (!pArg)
    {
        _log(LOG_LVL_ERROR, "%s() pArg NULL", __func__);
        UTILS_hang();
    }

    pInst = (Controller_t *) pArg;

    if (!g_pMouse)
    {
        _log(LOG_LVL_ERROR, "%s() Mouse NULL", __func__);
        UTILS_hang();
    }

    _log(LOG_LVL_DEBUG, "%s() Loop start", __func__);
    while (true)
    {
        ret = xQueueSemaphoreTake(pInst->semSendHid, portMAX_DELAY);
        if (ret != pdTRUE)
        {
            _log(LOG_LVL_ERROR, "%s() xQueueSemaphoreTake FAILED", __func__);
            continue;
        }

        uint32_t x_raw = 0;
        uint32_t y_raw = 0;
        int8_t x_map = 0;
        int8_t y_map = 0;
        int8_t mouse_x = 0;
        int8_t mouse_y = 0;
        int8_t sign = 0;

        for (uint8_t acq_idx = 0U; acq_idx < ACQ_NB; acq_idx += 1U)
        {
            x_raw += (uint32_t) adc1_get_raw(JOY_HW_X_CHAN);
            y_raw += (uint32_t) adc1_get_raw(JOY_HW_Y_CHAN);
        }

        x_raw /= ACQ_NB;
        y_raw /= ACQ_NB;

        if (x_raw < JOY_X_CENTER)
        {
            x_map = (int8_t) map(x_raw, JOY_X_MIN, JOY_X_CENTER - JOY_X_DEADZONE / 2, X_OUT_MIN, X_OUT_CENTER);
        }
        else
        {
            x_map = (int8_t) map(x_raw, JOY_X_CENTER - JOY_X_DEADZONE / 2, JOY_X_MAX, X_OUT_CENTER, X_OUT_MAX);
        }

        if (y_raw < JOY_Y_CENTER)
        {
            y_map = (int8_t) map(y_raw, JOY_Y_MIN, JOY_Y_CENTER - JOY_Y_DEADZONE / 2, Y_OUT_MIN, Y_OUT_CENTER);
        }
        else
        {
            y_map = (int8_t) map(y_raw, JOY_Y_CENTER - JOY_Y_DEADZONE / 2, JOY_Y_MAX, Y_OUT_CENTER, Y_OUT_MAX);
        }

        if (abs(x_map - X_OUT_CENTER) >= DEADZONE)
        {
            sign = JOY_X_SIGN * x_map / abs(x_map);
            mouse_x = sign * (abs(x_map - X_OUT_CENTER) - DEADZONE) / 3;
            if (abs(mouse_x) > MOUSE_SPEED_MAX)
            {
                mouse_x = sign * MOUSE_SPEED_MAX;
            }
        }

        if (abs(y_map - Y_OUT_CENTER) >= DEADZONE)
        {
            sign = JOY_Y_SIGN * y_map / abs(y_map);
            mouse_y = sign * (abs(y_map - Y_OUT_CENTER) - DEADZONE) / 3;
            if (abs(mouse_y) > MOUSE_SPEED_MAX)
            {
                mouse_y = sign * MOUSE_SPEED_MAX;
            }
        }

        MOUSE_move(g_pMouse, mouse_x, mouse_y);

        if ((LOG_LOOP_RATIO < 0xFF) && (loop_cnt == LOG_LOOP_RATIO))
        {
            print_val_report(x_raw, y_raw, x_map, y_map, mouse_x, mouse_y);
            loop_cnt = 0U;
        }

        loop_cnt += 1U;
    }
}

Controller_t * CONTROLLER_init(void)
{
    esp_err_t ret = ESP_OK;
    esp_timer_create_args_t timerArg;
    memset(&timerArg, 0, sizeof(timerArg));
    TaskHandle_t task = NULL;
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
        goto out_err;
    }

    pInst->magic = MAGIC;
    pInst->semSendHid = xSemaphoreCreateBinary();
    if (!pInst->semSendHid)
    {
        _log(LOG_LVL_ERROR, "%s() xSemaphoreCreateBinary FAILED", __func__);
        goto out_free_err;
    }

    timerArg.callback = &timerCb;
    timerArg.arg = (void *) pInst;

    _log(LOG_LVL_DEBUG, "%s() Create timer", __func__);
    ret = esp_timer_create(&timerArg, &pInst->timer);
    if ((ret != ESP_OK) || !pInst->timer)
    {
        _log(LOG_LVL_ERROR, "%s() esp_timer_create FAILED", __func__);
        goto out_free_err;
    }

    _log(LOG_LVL_DEBUG, "%s() Create main task", __func__);
    xTaskCreate(_main, "controllerMain", 0x1000U, (void *) pInst, tskIDLE_PRIORITY, &task);
    if (!task)
    {
        _log(LOG_LVL_ERROR, "%s() xTaskCreate FAILED", __func__);
        goto out_free_err;
    }

    return pInst;

out_free_err:
    if (pInst)
        free(pInst);
out_err:
    return NULL;
}

// TODO remove mouse from ctrl
void CONTROLLER_setMouse(Mouse_t * pMouse)
{
    g_pMouse = pMouse;
}

uint8_t CONTROLLER_start(Controller_t * pInst)
{
    esp_err_t ret = 0;

    _log(LOG_LVL_DEBUG, "%s()", __func__);

    if (!pInst || pInst->magic != MAGIC)
    {
        _log(LOG_LVL_ERROR, "%s() Bad instance pointer", __func__);
        return 1U;
    }

    ret = esp_timer_start_periodic(pInst->timer, MOUSE_REPORT_PERIOD_US);
    if (ret != ESP_OK)
    {
        _log(LOG_LVL_ERROR, "esp_timer_start_periodic FAILED");
        return 1U;
    }

    return 0U;
}

uint8_t CONTROLLER_stop(Controller_t * pInst)
{
    esp_err_t ret = 0;

    _log(LOG_LVL_DEBUG, "%s()", __func__);

    if (!pInst || pInst->magic != MAGIC)
    {
        _log(LOG_LVL_ERROR, "%s() Bad instance pointer", __func__);
        return 1U;
    }

    ret = esp_timer_stop(pInst->timer);
    if (ret != ESP_OK)
    {
        _log(LOG_LVL_ERROR, "esp_timer_stop FAILED");
        return 1U;
    }

    return 0U;
}
