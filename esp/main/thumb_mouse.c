
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "config.h"
#include "logger.h"
#include "controller.h"
#include "mouse.h"

#define GPIO_NUM_BTN_BOOT GPIO_NUM_0

Controller_t * g_pCtrl = NULL;
Mouse_t * g_pMouse = NULL;

static void __attribute__((format (printf, 2, 3))) _log(LogLevel_e lvl, const char * sFmt, ...);

static void _log(LogLevel_e lvl, const char * sFmt, ...)
{
    va_list pArg;

    va_start(pArg, sFmt);
    LOGGER_log_va(MODULE_ID_MAIN, lvl, sFmt, pArg);
    va_end(pArg);
}

// void clear_screen(void)
// {
//     for (uint8_t i = 0U; i < 0xFE; i += 1U)
//     {
//         printf("\n");
//     }
// }

// int32_t range(int32_t val, int32_t min, int32_t max, int32_t med, int32_t dead_med)
// {
//     if (val < min)
//     {
//         return min;
//     }

//     if (val > max)
//     {
//         return max;
//     }

//     if (abs(med - val) < dead_med)
//     {
//         return dead_med;
//     }

//     return val;
// }



// void print_axis(int32_t val_raw, char c_name)
// {
//     int32_t val_map = 0;

//     map(val_raw, JOY_X_MIN, JOY_X_MAX, X_OUT_MIN, X_OUT_MAX, &val_map);

//     printf("%c : [", c_name);
//     for (uint8_t i = 0U; i < X_OUT_MAX - X_OUT_MIN; i += 1U)
//     {
//         if (x + X_OUT_MIN == joy_x_out)
//         {
//             printf("+");
//         }
//         else
//         {
//             printf(" ");
//         }
//     }
//     printf("], raw = %04d, map = %02d\n", joy_x_in, joy_x_out);
// }

// void print_joystick(int32_t joy_x_in, int32_t joy_y_in)
// {
//     int32_t joy_x_out = 0;
//     int32_t joy_y_out = 0;

//     map(joy_x_in, joy_y_in, &joy_x_out, &joy_y_out);

//     printf("X : [");
//     for (uint8_t x = 0U; x < X_OUT_MAX - X_OUT_MIN; x += 1U)
//     {
//         if (x + X_OUT_MIN == joy_x_out)
//         {
//             printf("+");
//         }
//         else
//         {
//             printf(" ");
//         }
//     }
//     printf("], raw = %04d, map = %02d\n", joy_x_in, joy_x_out);

//     printf("Y : [");
//     for (uint8_t y = 0U; y < Y_OUT_MAX - Y_OUT_MIN; y += 1U)
//     {
//         if (y + Y_OUT_MIN == joy_y_out)
//         {
//             printf("+");
//         }
//         else
//         {
//             if (y == (Y_OUT_MAX - Y_OUT_MIN) / 2)
//             {
//                 printf("|");
//             }
//             else
//             {
//                 printf(" ");
//             }
//         }
//     }
//     printf("], raw = %04d, map = %02d\n", joy_y_in, joy_y_out);
// }

void app_main(void)
{
    uint8_t ret = 0U;
    esp_err_t espRet = ESP_OK;
    int btnBootVal = 0;
    uint8_t bCtrlEn = 0U;

    const gpio_config_t gpioConfBtnBoot =
    {
        .pin_bit_mask = BIT64(GPIO_NUM_BTN_BOOT),
        .mode = GPIO_MODE_INPUT,
        .intr_type = GPIO_INTR_DISABLE,
        .pull_up_en = false,
        .pull_down_en = true,
    };

    ret = LOGGER_init(LOG_LVL_DEBUG);
    if (ret)
    {
        _log(LOG_LVL_ERROR, "%s() LOGGER_init FAILED", __func__);
        UTILS_hang();
    }

    LOGGER_setLevel(MODULE_ID_MAIN, LOG_LVL_DEBUG);

    espRet = gpio_config(&gpioConfBtnBoot);
    if (espRet != ESP_OK)
    {
        _log(LOG_LVL_ERROR, "%s() gpio_config FAILED", __func__);
        UTILS_hang();
    }

    _log(LOG_LVL_DEBUG, "%s() MOUSE_init", __func__);
    g_pMouse = MOUSE_init();
    if (!g_pMouse)
    {
        _log(LOG_LVL_ERROR, "%s() MOUSE_init FAILED", __func__);
        UTILS_hang();
    }

    // TODO remove mouse from ctrl
    CONTROLLER_setMouse(g_pMouse);

    _log(LOG_LVL_DEBUG, "%s() CONTROLLER_init", __func__);
    g_pCtrl = CONTROLLER_init();
    if (!g_pCtrl)
    {
        _log(LOG_LVL_ERROR, "%s() CONTROLLER_init FAILED", __func__);
        UTILS_hang();
    }

    _log(LOG_LVL_DEBUG, "%s() Loop start", __func__);
    while (true)
    {
        if ((1 - gpio_get_level(GPIO_NUM_BTN_BOOT)) != btnBootVal)
        {
            // Boot button state change
            btnBootVal = 1 - btnBootVal;

            if (btnBootVal)
            {
                // Boot button state is ON

                // Controller state change
                bCtrlEn = 1U - bCtrlEn;

                if (bCtrlEn)
                {
                    _log(LOG_LVL_INFO, "Enable controller");
                    ret = CONTROLLER_start(g_pCtrl);
                    if (ret)
                    {
                        _log(LOG_LVL_ERROR, "%s() CONTROLLER_start FAILED", __func__);
                    }
                }
                else
                {
                    _log(LOG_LVL_INFO, "Disable controller");
                    ret = CONTROLLER_stop(g_pCtrl);
                    if (ret)
                    {
                        _log(LOG_LVL_ERROR, "%s() CONTROLLER_stop FAILED", __func__);
                    }
                }
            }
        }

        vTaskDelay(100U / portTICK_PERIOD_MS);
    }
}
