
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <inttypes.h>
#include <stdbool.h>

#include "esp_err.h"
#ifdef CONFIG_TRACKBALL_IT
#include "driver/gpio.h"
#endif
#include "driver/i2c.h"
#include "freertos/task.h"

#include "logger.h"

#include "trackball.h"

#define PIN_I2C_SDA 4
#define PIN_I2C_SCL 5
#define PIN_I2C_INT 6

#define I2C_MASTER_NUM              0
#define I2C_MASTER_FREQ_HZ          400000
#define I2C_MASTER_TX_BUF_DISABLE   0
#define I2C_MASTER_RX_BUF_DISABLE   0
#define I2C_MASTER_TIMEOUT_MS       1000

#define TRACKBALL_ADDR 0x0A

#define TRACKBALL_REG_LED_RED 0x00
#define TRACKBALL_REG_LED_GRN 0x01
#define TRACKBALL_REG_LED_BLU 0x02
#define TRACKBALL_REG_LED_WHT 0x03

#define TRACKBALL_REG_LEFT 0x04
#define TRACKBALL_REG_RIGHT 0x05
#define TRACKBALL_REG_UP 0x06
#define TRACKBALL_REG_DOWN 0x07
#define TRACKBALL_REG_SWITCH 0x08
#define TRACKBALL_MSK_SWITCH_STATE 0b10000000

#define TRACKBALL_REG_USER_FLASH 0xD0
#define TRACKBALL_REG_FLASH_PAGE 0xF0
#define TRACKBALL_REG_INT 0xF9
#define TRACKBALL_MSK_INT_TRIGGERED 0b00000001
#define TRACKBALL_MSK_INT_OUT_EN 0b00000010
#define TRACKBALL_REG_CHIP_ID_L 0xFA
#define TRACKBALL_RED_CHIP_ID_H 0xFB
#define TRACKBALL_REG_VERSION 0xFC
#define TRACKBALL_REG_I2C_ADDR 0xFD
#define TRACKBALL_REG_CTRL 0xFE
#define TRACKBALL_MSK_CTRL_SLEEP 0b00000001
#define TRACKBALL_MSK_CTRL_RESET 0b00000010
#define TRACKBALL_MSK_CTRL_FREAD 0b00000100
#define TRACKBALL_MSK_CTRL_FWRITE 0b00001000

static const uint32_t MAGIC = 0xBAAF1010;

static void __attribute__((format (printf, 2, 3))) _log(LogLevel_e lvl, const char * sFmt, ...);

static void _log(LogLevel_e lvl, const char * sFmt, ...)
{
    va_list pArg;

    va_start(pArg, sFmt);
    LOGGER_log_va(MODULE_ID_TRACKBALL, lvl, sFmt, pArg);
    va_end(pArg);
}

#ifdef CONFIG_TRACKBALL_IT
static void gpioIsrHandler(void * pvInst)
{
    Trackball_t * pInst = NULL;

    if (!pvInst)
    {
        return;
    }

    pInst = (Trackball_t *) pvInst;
    (void) xSemaphoreGiveFromISR(pInst->semIt, NULL);
}

static uint8_t initGpio(Trackball_t * pInst)
{
    esp_err_t ret = ESP_OK;
    gpio_config_t conf;
    memset(&conf, 0, sizeof(conf));

    _log(LOG_LVL_DEBUG, "%s()", __func__);

    if (!pInst || pInst->magic != MAGIC)
    {
        _log(LOG_LVL_ERROR, "%s() Bad instance pointer", __func__);
        return 1U;
    }

    conf.intr_type = GPIO_INTR_NEGEDGE;
    conf.pin_bit_mask = 1ULL << PIN_I2C_INT;
    conf.mode = GPIO_MODE_INPUT;
    conf.pull_up_en = 1;
    conf.pull_down_en = 0;
    ret = gpio_config(&conf);
    if (ret != ESP_OK)
    {
        _log(LOG_LVL_ERROR, "%s() gpio_config FAILED", __func__);
        return 1U;
    }

    ret = gpio_set_intr_type(PIN_I2C_INT, GPIO_INTR_NEGEDGE);
    if (ret != ESP_OK)
    {
        _log(LOG_LVL_ERROR, "%s() gpio_config FAILED", __func__);
        return 1U;
    }

    ret = gpio_install_isr_service(0 /* flags */);
    if (ret != ESP_OK)
    {
        _log(LOG_LVL_ERROR, "%s() gpio_config FAILED", __func__);
        return 1U;
    }

    ret = gpio_isr_handler_add(PIN_I2C_INT, gpioIsrHandler, (void *) pInst);
    if (ret != ESP_OK)
    {
        _log(LOG_LVL_ERROR, "%s() gpio_config FAILED", __func__);
        return 1U;
    }

    return 0U;
}
#endif // CONFIG_TRACKBALL_IT

static uint8_t initI2c(Trackball_t * pInst)
{
    esp_err_t espRet = ESP_OK;
    i2c_config_t conf;
    memset(&conf, 0, sizeof(conf));

    if (!pInst || pInst->magic != MAGIC)
    {
        _log(LOG_LVL_ERROR, "%s() Bad instance pointer", __func__);
        return 1U;
    }

    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = PIN_I2C_SDA;
    conf.scl_io_num = PIN_I2C_SCL;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    espRet = i2c_param_config(I2C_MASTER_NUM, &conf);
    if (espRet != ESP_OK)
    {
        _log(LOG_LVL_ERROR, "%s() i2c_param_config FAILED", __func__);
        return 1U;
    }

    espRet = i2c_driver_install(I2C_MASTER_NUM, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
    if (espRet != ESP_OK)
    {
        _log(LOG_LVL_ERROR, "%s() i2c_driver_install FAILED", __func__);
        return 1U;
    }

    return 0U;
}

static uint8_t read(Trackball_t * pInst, uint8_t addr, uint8_t * pData, uint16_t dataLen)
{
    esp_err_t ret = ESP_OK;

    if (!pInst || pInst->magic != MAGIC)
    {
        _log(LOG_LVL_ERROR, "%s() Bad instance pointer", __func__);
        return 1U;
    }

    ret = i2c_master_write_read_device(
        I2C_MASTER_NUM, TRACKBALL_ADDR, &addr, 1U, pData, dataLen, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
    if (ret != ESP_OK)
    {
        _log(LOG_LVL_ERROR, "%s() i2c_master_write_to_device FAILED. ret = %d = 0x%X", __func__, ret, ret);
        return 1U;
    }

    return 0U;
}

static uint8_t write(Trackball_t * pInst, uint8_t * pData, uint16_t dataLen)
{
    esp_err_t ret = ESP_OK;

    if (!pInst || pInst->magic != MAGIC)
    {
        _log(LOG_LVL_ERROR, "%s() Bad instance pointer", __func__);
        return 1U;
    }

    ret = i2c_master_write_to_device(
        I2C_MASTER_NUM, TRACKBALL_ADDR, pData, dataLen, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
    if (ret != ESP_OK)
    {
        _log(LOG_LVL_ERROR, "%s() i2c_master_write_to_device FAILED", __func__);
        return 1U;
    }

    return 0U;
}

#ifdef CONFIG_TRACKBALL_IT
static uint8_t setIt(Trackball_t * pInst, bool state)
{
    uint8_t ret = 0U;
    uint8_t dataRd = 0x00;
    uint8_t dataWr[2U] = {0x00};

    _log(LOG_LVL_DEBUG, "%s(state=%d)", __func__, state);

    if (!pInst || pInst->magic != MAGIC)
    {
        _log(LOG_LVL_ERROR, "%s() Bad instance pointer", __func__);
        return 1U;
    }

    ret = read(pInst, TRACKBALL_REG_INT, &dataRd, 1U);
    if (ret)
    {
        _log(LOG_LVL_ERROR, "%s() read FAILED", __func__);
        return 1U;
    }

#ifdef CONFIG_LOG_I2C
    _log(LOG_LVL_DEBUG, "  dataRd = 0x%08X", dataRd);
#endif

    dataWr[0U] = TRACKBALL_REG_INT;
    dataWr[1U] = dataRd;
    dataWr[1U] = dataWr[1U] & ~TRACKBALL_MSK_INT_TRIGGERED;
    dataWr[1U] = dataWr[1U] & ~TRACKBALL_MSK_INT_OUT_EN;
    if (state)
    {
        dataWr[1U] |= TRACKBALL_MSK_INT_OUT_EN;
    }

#ifdef CONFIG_LOG_I2C
    _log(LOG_LVL_DEBUG, "  dataWr = [0x%08X, 0x%08X]", dataWr[0U], dataWr[1U]);
#endif

    ret = write(pInst, &dataWr[0U], 2U);
    if (ret)
    {
        _log(LOG_LVL_ERROR, "%s() write FAILED", __func__);
        return 1U;
    }

#ifdef CONFIG_LOG_I2C
    ret = read(pInst, TRACKBALL_REG_INT, &dataRd, 1U);
    if (ret)
    {
        _log(LOG_LVL_ERROR, "%s() read FAILED", __func__);
        return 1U;
    }

    _log(LOG_LVL_DEBUG, "  dataRd = 0x%08X", dataRd);
#endif

    return 0U;
}

static void getDataTaskEntry(void * pvInst)
{
    uint8_t ret = 0U;
    uint8_t data[5U] = {0x00};
    BaseType_t retBase = pdFALSE;
    Trackball_t * pInst = NULL;

    pInst = (Trackball_t *) pvInst;
    if (!pInst || pInst->magic != MAGIC)
    {
        _log(LOG_LVL_ERROR, "%s() Bad instance pointer", __func__);
        return;
    }

    while (true)
    {
        retBase = xSemaphoreTake(pInst->semIt, portMAX_DELAY);
        if (retBase != pdTRUE)
        {
            _log(LOG_LVL_ERROR, "%s() xSemaphoreTake FAILED", __func__);
            vTaskDelay(1000U / portTICK_PERIOD_MS);
            continue;
        }

        // Read INT reg to clear IT ?
        // Clear IT by writing in INT reg ?

        ret = read(pInst, TRACKBALL_REG_LEFT, &data[0U], 5U);
        if (ret)
        {
            _log(LOG_LVL_ERROR, "%s() read FAILED", __func__);
            continue;
        }

        _log(LOG_LVL_DEBUG, "data = [0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X]",
            data[0U], data[1U], data[2U], data[3U], data[4U]);
    }
}
#endif // CONFIG_TRACKBALL_IT

Trackball_t * TRACKBALL_init(void)
{
    uint8_t ret = 0U;
#ifdef CONFIG_TRACKBALL_IT
    TaskHandle_t task = NULL;
#endif
    Trackball_t * pInst = NULL;

    LOGGER_setLevel(MODULE_ID_TRACKBALL, LOG_LVL_DEBUG);

    _log(LOG_LVL_DEBUG, "%s()", __func__);

    pInst = (Trackball_t *) malloc(sizeof(Trackball_t));
    if (!pInst)
    {
        _log(LOG_LVL_ERROR, "%s() malloc %u Bytes for Trackball_t FAILED", __func__, (uint16_t) sizeof(Trackball_t));
        goto out_err;
    }

    pInst->magic = MAGIC;

#ifdef CONFIG_TRACKBALL_IT
    pInst->semIt = xSemaphoreCreateBinary();
    if (!pInst->semIt)
    {
        _log(LOG_LVL_ERROR, "%s() xSemaphoreCreateBinary FAILED", __func__);
        goto out_free_err;
    }
#endif

    ret = initI2c(pInst);
    if (ret)
    {
        _log(LOG_LVL_ERROR, "%s() initI2c FAILED", __func__);
        goto out_free_err;
    }

#ifdef CONFIG_TRACKBALL_IT
    ret = initGpio(pInst);
    if (ret)
    {
        _log(LOG_LVL_ERROR, "%s() initGpio FAILED", __func__);
        goto out_free_err;
    }
#endif

    ret = TRACKBALL_reset(pInst);
    if (ret)
    {
        _log(LOG_LVL_ERROR, "%s() reset FAILED", __func__);
        goto out_free_err;
    }

#ifdef CONFIG_TRACKBALL_IT
    ret = setIt(pInst, true);
    if (ret)
    {
        _log(LOG_LVL_ERROR, "%s() setIt FAILED", __func__);
        goto out_free_err;
    }

    xTaskCreate(getDataTaskEntry, "trackballGetData", 0x1000U, pInst, 1U, &task);
    if (!task)
    {
        _log(LOG_LVL_ERROR, "%s() xTaskCreate FAILED", __func__);
        goto out_free_err;
    }
#endif

    return pInst;

out_free_err:
    if (pInst)
        free(pInst);
out_err:
    return NULL;
}

uint8_t TRACKBALL_reset(Trackball_t * pInst)
{
    uint8_t ret = 0U;
    uint8_t data[2U] = {0x00};

    _log(LOG_LVL_DEBUG, "%s()", __func__);

    if (!pInst || pInst->magic != MAGIC)
    {
        _log(LOG_LVL_ERROR, "%s() Bad instance pointer", __func__);
        return 1U;
    }

    data[0U] = TRACKBALL_REG_CTRL;
    data[1U] = TRACKBALL_MSK_CTRL_RESET;
    ret = write(pInst, &data[0U], 2U);
    if (ret)
    {
        _log(LOG_LVL_ERROR, "%s() i2c_master_write_to_device FAILED", __func__);
        return 1U;
    }

    vTaskDelay(100U / portTICK_PERIOD_MS);

    return 0U;
}

uint8_t TRACKBALL_getData(Trackball_t * pInst, int8_t * axisX, int8_t * axisY, bool * bSwitch)
{
#define DATA_LEN            (TRACKBALL_REG_SWITCH - TRACKBALL_REG_LEFT + 1)
#define DATA_IDX_LEFT       (TRACKBALL_REG_LEFT - TRACKBALL_REG_LEFT)
#define DATA_IDX_RIGHT      (TRACKBALL_REG_RIGHT - TRACKBALL_REG_LEFT)
#define DATA_IDX_UP         (TRACKBALL_REG_UP - TRACKBALL_REG_LEFT)
#define DATA_IDX_DOWN       (TRACKBALL_REG_DOWN - TRACKBALL_REG_LEFT)
#define DATA_IDX_SWITCH     (TRACKBALL_REG_SWITCH - TRACKBALL_REG_LEFT)

    uint8_t ret = 0U;
    uint8_t data[DATA_LEN] = {0x00};

    if (!pInst || pInst->magic != MAGIC)
    {
        _log(LOG_LVL_ERROR, "%s() Bad instance pointer", __func__);
        return 1U;
    }

    if (!axisX || !axisY)
    {
        _log(LOG_LVL_ERROR, "%s() Input arg not defined", __func__);
        return 1U;
    }

    *axisX = 0;
    *axisY = 0;
    *bSwitch = false;

    ret = read(pInst, TRACKBALL_REG_LEFT, &data[0U], DATA_LEN);
    if (ret)
    {
        _log(LOG_LVL_ERROR, "%s() read FAILED", __func__);
        return 1U;
    }

#ifdef CONFIG_LOG_I2C
    _log(LOG_LVL_DEBUG, "%s() data = [0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X]",
        __func__, data[0U], data[1U], data[2U], data[3U], data[4U]);
#endif

    if (data[DATA_IDX_LEFT] || data[DATA_IDX_RIGHT])
    {
        if (data[DATA_IDX_LEFT] > data[DATA_IDX_RIGHT])
        {
            *axisX = -data[DATA_IDX_LEFT];
        }
        else
        {
            *axisX = data[DATA_IDX_RIGHT];
        }
    }

    if (data[DATA_IDX_UP] || data[DATA_IDX_DOWN])
    {
        if (data[DATA_IDX_UP] > data[DATA_IDX_DOWN])
        {
            *axisY = -data[DATA_IDX_UP];
        }
        else
        {
            *axisY = data[DATA_IDX_DOWN];
        }
    }

    if (data[DATA_IDX_SWITCH] & TRACKBALL_MSK_SWITCH_STATE)
    {
        *bSwitch = true;
    }

    return 0U;
}

uint8_t TRACKBALL_setColor(Trackball_t * pInst, uint8_t red, uint8_t green, uint8_t blue, uint8_t white)
{
    uint8_t ret = 0U;
    uint8_t data[5U] = {0x00};

    if (!pInst || pInst->magic != MAGIC)
    {
        _log(LOG_LVL_ERROR, "%s() Bad instance pointer", __func__);
        return 1U;
    }

    data[0U] = TRACKBALL_REG_LED_RED;
    data[1U] = red;
    data[2U] = green;
    data[3U] = blue;
    data[4U] = white;
    ret = write(pInst, &data[0U], 5U);
    if (ret)
    {
        _log(LOG_LVL_ERROR, "%s() i2c_master_write_to_device FAILED", __func__);
        return 1U;
    }

    return 0U;
}
