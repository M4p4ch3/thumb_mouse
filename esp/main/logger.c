
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "logger.h"

#define MSG_SIZE_MAX 200U

static const LogLevel_e LOG_LVL_DFLT = LOG_LVL_INFO;

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

static LogLevel_e g_pLvlModule[MODULE_ID_NB] = {
    LOG_LVL_DFLT,
    LOG_LVL_DFLT,
    LOG_LVL_DFLT,
    LOG_LVL_DFLT,
};

uint8_t LOGGER_init(LogLevel_e lvl)
{
    LOGGER_setLevel(MODULE_ID_NONE, lvl);

    return 0U;
}

void LOGGER_setLevel(ModuleId_e moduleId, LogLevel_e lvl)
{
    uint8_t moduleIdIt = 0U;

    if (moduleId >= MODULE_ID_NB)
    {
        printf("ERROR %s() moduleId out of range (%u >= %u)\n", __func__, moduleId, MODULE_ID_NB);
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
    char sMsg[MSG_SIZE_MAX] = {'\0'};

    if (moduleId >= MODULE_ID_NB)
    {
        printf("ERROR %s() moduleId out of range (%u >= %u)\n", __func__, moduleId, MODULE_ID_NB);
        return;
    }

    if (lvl >= LOG_LVL_NB)
    {
        printf("ERROR %s() lvl out of range (%u >= %u)\n", __func__, lvl, LOG_LVL_NB);
        return;
    }

    if (lvl > g_pLvlModule[moduleId])
    {
        return;
    }

    vsnprintf(sMsg, MSG_SIZE_MAX, sFmt, pArg);
    sMsg[MSG_SIZE_MAX - 1U] = '\0';

    printf("[%s][%10s] %s\n", LEVEL_PFX_LIST[(uint8_t) lvl], MODULE_NAME_LIST[(uint8_t) moduleId], sMsg);
}

void LOGGER_log(ModuleId_e moduleId, LogLevel_e lvl, const char * sFmt, ...)
{
    va_list pArg;

    if (moduleId >= MODULE_ID_NB)
    {
        printf("ERROR %s() moduleId out of range (%u >= %u)\n", __func__, moduleId, MODULE_ID_NB);
        return;
    }

    if (lvl >= LOG_LVL_NB)
    {
        printf("ERROR %s() lvl out of range (%u >= %u)\n", __func__, lvl, LOG_LVL_NB);
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
