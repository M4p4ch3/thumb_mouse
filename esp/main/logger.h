
#ifndef LOGGER_H
#define LOGGER_H

#include <inttypes.h>
#include <stdarg.h>

typedef enum LogLevel_e
{
    LOG_LVL_ERROR = 0,
    LOG_LVL_WARN,
    LOG_LVL_INFO,
    LOG_LVL_DEBUG,
    LOG_LVL_NB,
} LogLevel_e;

typedef enum ModuleId_e
{
    MODULE_ID_NONE = 0,
    MODULE_ID_MAIN,
    MODULE_ID_CTRL,
    MODULE_ID_MOUSE,
    MODULE_ID_NB,
} ModuleId_e;

uint8_t LOGGER_init(LogLevel_e lvl);

void LOGGER_setLevel(ModuleId_e moduleId, LogLevel_e lvl);

void LOGGER_log_va(ModuleId_e moduleId, LogLevel_e lvl, const char * sFmt, va_list pArg);

void LOGGER_log(ModuleId_e moduleId, LogLevel_e lvl, const char * sFmt, ...);

#endif // LOGGER_H
