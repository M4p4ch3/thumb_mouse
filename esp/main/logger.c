
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "utils.h"

#include "logger.h"

#define BUF_SIZE_MAX 200U

typedef struct Msg_t
{
    struct timespec tp;
    LogLevel_e lvl;
    ModuleId_e moduleId;
    char sBuf[BUF_SIZE_MAX];
} Msg_t;

static const LogLevel_e LOG_LVL_DFLT = LOG_LVL_INFO;

static const uint16_t MSG_NB_MAX = 50U;

static const char * LEVEL_PFX_LIST[] =
{
    "ERROR",
    "WARN ",
    "INFO ",
    "DEBUG",
    "UNKNOWN",
};

static const char * MODULE_NAME_LIST[] =
{
    "NONE",
    "MAIN",
    "CTRL",
    "MOUSE",
    "UNKNOWN",
};

static QueueHandle_t g_queue = NULL;

static LogLevel_e g_pLvlModule[MODULE_ID_NB] = {
    LOG_LVL_DFLT,
    LOG_LVL_DFLT,
    LOG_LVL_DFLT,
    LOG_LVL_DFLT,
};

static void _main(void * pArg)
{
    BaseType_t baseRet = pdTRUE;
    Msg_t * pMsg = NULL;

    (void) pArg;

    printf("DEBUG Logger %s()\n", __func__);

    if (!g_queue)
    {
        printf("ERROR Logger %s() g_queue NULL", __func__);
        UTILS_hang();
    }

    printf("DEBUG Logger %s() main loop\n", __func__);
    while (true)
    {
        baseRet = xQueueReceive(g_queue, (void *) &pMsg, portMAX_DELAY);
        if (baseRet != pdPASS)
        {
            continue;
        }

        if (!pMsg)
        {
            printf("ERROR Logger %s() pMsg NULL\n", __func__);
            continue;
        }

        if (pMsg->moduleId >= MODULE_ID_NB)
        {
            printf("ERROR Logger %s() moduleId out of range (%u >= %u)\n", __func__, pMsg->moduleId, MODULE_ID_NB);
            continue;
        }

        if (pMsg->lvl >= LOG_LVL_NB)
        {
            printf("ERROR Logger %s() lvl out of range (%u >= %u)\n", __func__, pMsg->lvl, LOG_LVL_NB);
            continue;
        }

        if (pMsg->lvl > g_pLvlModule[pMsg->moduleId])
        {
            continue;
        }

        printf("%04lld.%03lu [%s][%10s] %s\n",
            pMsg->tp.tv_sec, pMsg->tp.tv_nsec / NS_PER_MS,
            LEVEL_PFX_LIST[(uint8_t) pMsg->lvl], MODULE_NAME_LIST[(uint8_t) pMsg->moduleId], pMsg->sBuf);

        free(pMsg);
        pMsg = NULL;
    }
}

uint8_t LOGGER_init(LogLevel_e lvl)
{
    TaskHandle_t task = NULL;

    LOGGER_setLevel(MODULE_ID_NONE, lvl);

    g_queue = xQueueCreate(MSG_NB_MAX, sizeof(Msg_t *));
    if (!g_queue)
    {
        printf("ERROR Logger %s() xQueueCreate FAILED", __func__);
        return 1U;
    }

    printf("DEBUG Logger %s() Create main task\n", __func__);
    xTaskCreate(_main, "loggerMain", 0x1000U, NULL, 1U, &task);
    if (!task)
    {
        printf("ERROR Logger %s() xTaskCreate FAILED", __func__);
        return 1U;
    }

    return 0U;
}

void LOGGER_setLevel(ModuleId_e moduleId, LogLevel_e lvl)
{
    uint8_t moduleIdIt = 0U;

    if (moduleId >= MODULE_ID_NB)
    {
        printf("ERROR Logger %s() moduleId out of range (%u >= %u)\n", __func__, moduleId, MODULE_ID_NB);
        return;
    }

    if (lvl >= LOG_LVL_NB)
    {
        lvl = LOG_LVL_DEBUG;
    }

    if (moduleId == MODULE_ID_NONE)
    {
        for (moduleIdIt = 0U; moduleIdIt < (uint8_t) MODULE_ID_NB; moduleIdIt += 1U)
        {
            g_pLvlModule[moduleIdIt] = lvl;
        }

        return;
    }

    g_pLvlModule[moduleId] = lvl;
}

void LOGGER_log_va(ModuleId_e moduleId, LogLevel_e lvl, const char * sFmt, va_list pArg)
{
    int iRet = 0;
    BaseType_t baseRet = pdFALSE;
    Msg_t * pMsg = NULL;

    pMsg = (Msg_t *) malloc(sizeof(Msg_t));
    if (!pMsg)
    {
        printf("ERROR Logger %s() malloc %u Bytes for Msg_t FAILED\n", __func__, sizeof(Msg_t));
        return;
    }

    memset(pMsg, 0, sizeof(Msg_t));

    iRet = clock_gettime(CLOCK_MONOTONIC, &pMsg->tp);
    if (iRet != 0)
    {
        printf("ERROR Logger %s() clock_gettime FAILED", __func__);
        return;
    }

    pMsg->lvl = lvl;
    pMsg->moduleId = moduleId;

    vsnprintf(pMsg->sBuf, BUF_SIZE_MAX, sFmt, pArg);
    pMsg->sBuf[BUF_SIZE_MAX - 1U] = '\0';

    if (!g_queue)
    {
        printf("ERROR Logger %s() g_queue NULL", __func__);
        return;
    }

    baseRet = xQueueSend(g_queue, (void *) &pMsg, 10U);
    if (!baseRet)
    {
        printf("ERROR Logger %s() xQueueSend FAILED\n", __func__);
        return;
    }
}

void LOGGER_log(ModuleId_e moduleId, LogLevel_e lvl, const char * sFmt, ...)
{
    va_list pArg;

    if (moduleId >= MODULE_ID_NB)
    {
        printf("ERROR Logger %s() moduleId out of range (%u >= %u)\n", __func__, moduleId, MODULE_ID_NB);
        return;
    }

    if (lvl >= LOG_LVL_NB)
    {
        printf("ERROR Logger %s() lvl out of range (%u >= %u)\n", __func__, lvl, LOG_LVL_NB);
        return;
    }

    if (lvl > g_pLvlModule[moduleId])
    {
        return;
    }

    va_start(pArg, sFmt);
    LOGGER_log_va(moduleId, lvl, sFmt, pArg);
    va_end(pArg);
}
