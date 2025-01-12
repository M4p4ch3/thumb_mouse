
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "esp_err.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "config.h"
#include "logger.h"
#include "controller.h"
#include "trackball.h"
#include "mouse.h"

#define GPIO_NUM_BTN_BOOT GPIO_NUM_0

#define I2C_MASTER_SCL_IO           5       /*!< GPIO number used for I2C master clock */
#define I2C_MASTER_SDA_IO           4       /*!< GPIO number used for I2C master data  */
#define I2C_MASTER_NUM              0       /*!< I2C master i2c port number, the number of i2c peripheral interfaces available will depend on the chip */
#define I2C_MASTER_FREQ_HZ          400000  /*!< I2C master clock frequency */
#define I2C_MASTER_TX_BUF_DISABLE   0       /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0       /*!< I2C master doesn't need buffer */
#define I2C_MASTER_TIMEOUT_MS       1000

// static const uint32_t MOUSE_MOVE_PERIOD_US = US_PER_S / MOUSE_REPORT_FREQ_HZ;

// // Initial mouse state
// static const uint8_t MOUSE_STATE_INIT = 0U;

static SemaphoreHandle_t g_semMoveMouse = NULL;

static Controller_t * g_pCtrl = NULL;
static Trackball_t * g_pTrackball = NULL;
static Mouse_t * g_pMouse = NULL;

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

// static void print_val_report(int32_t x_raw, int32_t y_raw, int32_t x_map, int32_t y_map, int32_t mouse_x, int32_t mouse_y, uint16_t acqNb)
// {
//     _log(LOG_LVL_DEBUG, "joy.x  =  %04ld, joy.y  =  %04ld", x_map, y_map);
//     _log(LOG_LVL_DEBUG, "mouse.x = %04ld, mouse.y = %04ld", mouse_x, mouse_y);
//     _log(LOG_LVL_DEBUG, "acqNb = %u", acqNb);
// }

static void moveMouseFromCtrlMain(void *)
{
    uint8_t uRet = 0U;
    BaseType_t baseRet = pdFALSE;

    uint16_t loopCnt = 0U;

    // int8_t sign = 0U;
    uint16_t ctrlJoyAcqNb = 0U;
    Coord_t coordCtrlJoy;
    memset(&coordCtrlJoy, 0, sizeof(coordCtrlJoy));
    Coord_t coordCtrlJoyAcc;
    memset(&coordCtrlJoyAcc, 0, sizeof(coordCtrlJoyAcc));
    Coord_t coordMouse;
    memset(&coordMouse, 0, sizeof(coordMouse));

    while (true)
    {
        // Can't go below 5 ms, as 1 tick
        baseRet = xQueueSemaphoreTake(g_semMoveMouse, 5U / portTICK_PERIOD_MS);
        if (baseRet && ctrlJoyAcqNb)
        {
            coordMouse.x = 0;
            coordMouse.y = 0;

            coordCtrlJoy.x = coordCtrlJoyAcc.x / ctrlJoyAcqNb;
            coordCtrlJoy.y = coordCtrlJoyAcc.y / ctrlJoyAcqNb;

            if (coordCtrlJoy.x < X_OUT_CENTER - DEADZONE)
            {
                coordMouse.x = - (X_OUT_CENTER - coordCtrlJoy.x - DEADZONE) / 3;
            }
            else if (coordCtrlJoy.x > X_OUT_CENTER + DEADZONE)
            {
                coordMouse.x = (coordCtrlJoy.x - X_OUT_CENTER - DEADZONE) / 3;
            }

            if (coordCtrlJoy.y < Y_OUT_CENTER - DEADZONE)
            {
                coordMouse.y = - (Y_OUT_CENTER - coordCtrlJoy.y - DEADZONE) / 3;
            }
            else if (coordCtrlJoy.y > Y_OUT_CENTER + DEADZONE)
            {
                coordMouse.y = (coordCtrlJoy.y - Y_OUT_CENTER - DEADZONE) / 3;
            }

            uRet = MOUSE_move(g_pMouse, (int8_t) coordMouse.x, (int8_t) coordMouse.y);
            if (uRet)
            {
                _log(LOG_LVL_ERROR, "%s() MOUSE_move FAILED", __func__);
                vTaskDelay(1000U / portTICK_PERIOD_MS);
                continue;
            }

            if ((MOUSE_LOG_LOOP_NB < 0xFF) && (loopCnt == MOUSE_LOG_LOOP_NB))
            {
                _log(LOG_LVL_DEBUG, "joy.x  =  %04ld, joy.y  =  %04ld", coordCtrlJoy.x, coordCtrlJoy.y);
                _log(LOG_LVL_DEBUG, "mouse.x = %04ld, mouse.y = %04ld", coordMouse.x, coordMouse.y);
                _log(LOG_LVL_DEBUG, "acqNb = %u", ctrlJoyAcqNb);
                loopCnt = 0U;
            }

            ctrlJoyAcqNb = 0U;
            coordCtrlJoyAcc.x = 0;
            coordCtrlJoyAcc.y = 0;

            loopCnt += 1U;
        }

        uRet = CONTROLLER_getJoy(g_pCtrl, &coordCtrlJoy);
        if (uRet)
        {
            _log(LOG_LVL_ERROR, "%s() CONTROLLER_getJoy FAILED", __func__);
            vTaskDelay(1000U / portTICK_PERIOD_MS);
            continue;
        }

        ctrlJoyAcqNb += 1U;
        coordCtrlJoyAcc.x += coordCtrlJoy.x;
        coordCtrlJoyAcc.y += coordCtrlJoy.y;

        // vTaskDelay(10U / portTICK_PERIOD_MS);
    }
}

static void timerCb(void * pArg)
{
    BaseType_t ret = pdFALSE;

    if (!g_semMoveMouse)
    {
        _log(LOG_LVL_ERROR, "%s() g_semMoveMouse NULL", __func__);
        return;
    }

    ret = xSemaphoreGiveFromISR(g_semMoveMouse, NULL);
    if (ret != pdTRUE)
    {
        _log(LOG_LVL_ERROR, "%s() xSemaphoreGive FAILED", __func__);
        return;
    }
}

// Apply acceleration and speed to value
int8_t applyAccelSpeed(int8_t val, float daccel, float accel, float speed)
{
    int16_t magnitude = 0U;
    float fVal = 0.0;

    if (!val)
    {
        return 0;
    }

    fVal = (float) abs(val);

    magnitude = daccel * fVal * fVal * fVal + accel * fVal * fVal + speed * fVal;
    if (magnitude > 127)
    {
        magnitude = 127;
    }

    return val / abs(val) * (int8_t) magnitude;
}

typedef enum MouseModeId
{
    MOUSE_MODE_ID_SLOW = 0,
    MOUSE_MODE_ID_NORMAL,
    MOUSE_MODE_ID_FAST,
    MOUSE_MODE_ID_NB,
} MouseModeId_e;

typedef struct TrackBallColor
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t w;
} TrackBallColor_t;

typedef struct MouseMode
{
    MouseModeId_e id;
    float daccel;
    float accel;
    float speed;
    TrackBallColor_t color;
} MouseMode_t;

static const MouseMode_t MOUSE_MODE_LIST[] =
{
    {
        .id = MOUSE_MODE_ID_SLOW,
        .daccel = 0.3,
        .accel = 0.4,
        .speed = 0.3,
        .color =
        {
            .r = 0x00,
            .g = 0x80,
            .b = 0x00,
            .w = 0x80,
        }
    },
    {
        .id = MOUSE_MODE_ID_NORMAL,
        .daccel = 0.4,
        .accel = 0.3,
        .speed = 0.3,
        .color =
        {
            .r = 0x00,
            .g = 0x00,
            .b = 0x80,
            .w = 0x80,
        }
    },
    {
        .id = MOUSE_MODE_ID_FAST,
        .daccel = 0.5,
        .accel = 0.25,
        .speed = 0.25,
        .color =
        {
            .r = 0x80,
            .g = 0x00,
            .b = 0x00,
            .w = 0x80,
        }
    },
};

static void taskReadTrackball(void * pvArg)
{
    uint8_t ret = 0U;
    bool trackballSwPrev = false;
    bool trackballSw = false;
    int8_t trackballX = 0;
    int8_t trackballY = 0;
    int8_t mouseX = 0;
    int8_t mouseY = 0;
    MouseModeId_e mouseModeId = MOUSE_MODE_ID_SLOW;
    float daccel = 0.0;
    float accel = 0.0;
    float speed = 0.0;

    (void) pvArg;

    daccel = MOUSE_MODE_LIST[mouseModeId].daccel;
    accel = MOUSE_MODE_LIST[mouseModeId].accel;
    speed = MOUSE_MODE_LIST[mouseModeId].speed;
    TRACKBALL_setColor(g_pTrackball,
        MOUSE_MODE_LIST[mouseModeId].color.r,
        MOUSE_MODE_LIST[mouseModeId].color.g,
        MOUSE_MODE_LIST[mouseModeId].color.b,
        MOUSE_MODE_LIST[mouseModeId].color.w);

    while (true)
    {
        trackballSwPrev = trackballSw;

        ret = TRACKBALL_getData(g_pTrackball, &trackballX, &trackballY, &trackballSw);
        if (ret)
        {
            _log(LOG_LVL_ERROR, "%s() TRACKBALL_getData FAILED", __func__);
            vTaskDelay(1000U / portTICK_PERIOD_MS);
            continue;
        }

        if (trackballSw && !trackballSwPrev)
        {
            mouseModeId += 1U;
            if (mouseModeId == MOUSE_MODE_ID_NB)
            {
                mouseModeId = MOUSE_MODE_ID_SLOW;
            }

            daccel = MOUSE_MODE_LIST[mouseModeId].daccel;
            accel = MOUSE_MODE_LIST[mouseModeId].accel;
            speed = MOUSE_MODE_LIST[mouseModeId].speed;
            TRACKBALL_setColor(g_pTrackball,
                MOUSE_MODE_LIST[mouseModeId].color.r,
                MOUSE_MODE_LIST[mouseModeId].color.g,
                MOUSE_MODE_LIST[mouseModeId].color.b,
                MOUSE_MODE_LIST[mouseModeId].color.w);
        }

        if (!trackballX && !trackballY)
        {
            vTaskDelay(10U / portTICK_PERIOD_MS);
            continue;
        }

        mouseX = applyAccelSpeed(trackballX, daccel, accel, speed);
        mouseY = applyAccelSpeed(trackballY, daccel, accel, speed);

        // _log(LOG_LVL_DEBUG, "%s()", __func__);
        // _log(LOG_LVL_DEBUG, "  Trackball : X = %+04d, Y = %+04d", trackballX, trackballY);
        // _log(LOG_LVL_DEBUG, "  Mouse     : X = %+04d, Y = %+04d", mouseX, mouseY);

        ret = MOUSE_move(g_pMouse, mouseX, mouseY);
        if (ret)
        {
            _log(LOG_LVL_ERROR, "%s() MOUSE_move FAILED", __func__);
            vTaskDelay(1000U / portTICK_PERIOD_MS);
            continue;
        }

        vTaskDelay(10U / portTICK_PERIOD_MS);
    }
}

void app_main(void)
{
    uint8_t ret = 0U;
    esp_err_t espRet = ESP_OK;
    int btnBootVal = 0;

    const gpio_config_t gpioConfBtnBoot =
    {
        .pin_bit_mask = BIT64(GPIO_NUM_BTN_BOOT),
        .mode = GPIO_MODE_INPUT,
        .intr_type = GPIO_INTR_DISABLE,
        .pull_up_en = false,
        .pull_down_en = true,
    };

    esp_timer_handle_t timerMouse = NULL;
    esp_timer_create_args_t timerArg;
    memset(&timerArg, 0, sizeof(timerArg));
    TaskHandle_t task = NULL;

    ret = LOGGER_init(LOG_LVL_DEBUG);
    if (ret)
    {
        _log(LOG_LVL_ERROR, "%s() LOGGER_init FAILED", __func__);
        UTILS_hang();
    }

    LOGGER_setLevel(MODULE_ID_MAIN, LOG_LVL_DEBUG);

    // espRet = gpio_config(&gpioConfBtnBoot);
    // if (espRet != ESP_OK)
    // {
    //     _log(LOG_LVL_ERROR, "%s() gpio_config FAILED", __func__);
    //     UTILS_hang();
    // }

    // _log(LOG_LVL_DEBUG, "%s() CONTROLLER_init", __func__);
    // g_pCtrl = CONTROLLER_init();
    // if (!g_pCtrl)
    // {
    //     _log(LOG_LVL_ERROR, "%s() CONTROLLER_init FAILED", __func__);
    //     UTILS_hang();
    // }

    _log(LOG_LVL_DEBUG, "%s() TRACKBALL_init", __func__);
    g_pTrackball = TRACKBALL_init();
    if (!g_pTrackball)
    {
        _log(LOG_LVL_ERROR, "%s() TRACKBALL_init FAILED", __func__);
        UTILS_hang();
    }

    _log(LOG_LVL_DEBUG, "%s() MOUSE_init", __func__);
    g_pMouse = MOUSE_init(true /* bEn */);
    if (!g_pMouse)
    {
        _log(LOG_LVL_ERROR, "%s() MOUSE_init FAILED", __func__);
        UTILS_hang();
    }

    // g_semMoveMouse = xSemaphoreCreateBinary();
    // if (!g_semMoveMouse)
    // {
    //     _log(LOG_LVL_ERROR, "%s() xSemaphoreCreateBinary FAILED", __func__);
    //     UTILS_hang();
    // }

    // _log(LOG_LVL_DEBUG, "%s() Create moveMouseFromCtrl task", __func__);
    // xTaskCreate(moveMouseFromCtrlMain, "moveMouseFromCtrl", 0x1000U, NULL, configMAX_PRIORITIES - 5U, &task);
    // if (!task)
    // {
    //     _log(LOG_LVL_ERROR, "%s() xTaskCreate FAILED", __func__);
    //     UTILS_hang();
    // }

    _log(LOG_LVL_DEBUG, "%s() Create taskReadTrackball task", __func__);
    xTaskCreate(taskReadTrackball, "readTrackball", 0x1000U, NULL, configMAX_PRIORITIES - 5U, &task);
    if (!task)
    {
        _log(LOG_LVL_ERROR, "%s() xTaskCreate FAILED", __func__);
        UTILS_hang();
    }

    // timerArg.callback = &timerCb;
    // timerArg.arg = NULL;

    // _log(LOG_LVL_DEBUG, "%s() Create mouse timer", __func__);
    // espRet = esp_timer_create(&timerArg, &timerMouse);
    // if ((espRet != ESP_OK) || !timerMouse)
    // {
    //     _log(LOG_LVL_ERROR, "%s() esp_timer_create FAILED", __func__);
    //     UTILS_hang();
    // }

    // espRet = esp_timer_start_periodic(timerMouse, MOUSE_MOVE_PERIOD_US);
    // if (espRet != ESP_OK)
    // {
    //     _log(LOG_LVL_ERROR, "%s() esp_timer_start_periodic FAILED", __func__);
    //     UTILS_hang();
    // }

    // ret = TRACKBALL_setColor(g_pTrackball, 0x80, 0x00, 0x00, 0x80);
    // if (ret)
    // {
    //     _log(LOG_LVL_ERROR, "%s() TRACKBALL_setColor FAILED", __func__);
    //     UTILS_hang();
    // }

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

                // // Mouse state change
                // MOUSE_setEnabled(g_pMouse, 1U - MOUSE_getEnabled(g_pMouse));
            }
        }

        // dataWr[0U] = TRACKBALL_REG_LEFT;
        // espRet = i2c_master_write_read_device(I2C_MASTER_NUM, TRACKBALL_ADDR,
        //     &dataWr[0U], 1U, &dataRd[0U], 5U,
        //     I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
        // if (espRet != ESP_OK)
        // {
        //     _log(LOG_LVL_ERROR, "%s() i2c_master_write_read_device FAILED", __func__);
        //     vTaskDelay(1000U / portTICK_PERIOD_MS);
        // }

        // _log(LOG_LVL_DEBUG, "data = [0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X]",
        //     dataRd[0U], dataRd[1U], dataRd[2U], dataRd[3U], dataRd[4U]);

        vTaskDelay(100U / portTICK_PERIOD_MS);
    }
}
