
#include <inttypes.h>
#include <stdarg.h>

#include "tinyusb.h"
#include "class/hid/hid_device.h"

#include "logger.h"

#include "mouse.h"

/************* TinyUSB descriptors ****************/

#define TUSB_DESC_TOTAL_LEN      (TUD_CONFIG_DESC_LEN + CFG_TUD_HID * TUD_HID_DESC_LEN)

/**
 * @brief HID report descriptor
 *
 * In this example we implement Keyboard + Mouse HID device,
 * so we must define both report descriptors
 */
const uint8_t hid_report_descriptor[] = {
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(HID_ITF_PROTOCOL_KEYBOARD)),
    TUD_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(HID_ITF_PROTOCOL_MOUSE))
};

/**
 * @brief String descriptor
 */
const char* hid_string_descriptor[5] = {
    // array of pointer to string descriptors
    (char[]){0x09, 0x04},  // 0: is supported language is English (0x0409)
    "TinyUSB",             // 1: Manufacturer
    "TinyUSB Device",      // 2: Product
    "123456",              // 3: Serials, should use chip ID
    "Example HID interface",  // 4: HID
};

/**
 * @brief Configuration descriptor
 *
 * This is a simple configuration descriptor that defines 1 configuration and 1 HID interface
 */
static const uint8_t hid_configuration_descriptor[] = {
    // Configuration number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, TUSB_DESC_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

    // Interface number, string index, boot protocol, report descriptor len, EP In address, size & polling interval
    TUD_HID_DESCRIPTOR(0, 4, false, sizeof(hid_report_descriptor), 0x81, 16, 10),
};

/********* TinyUSB HID callbacks ***************/

// Invoked when received GET HID REPORT DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance)
{
    // We use only one interface and one HID report descriptor, so we can ignore parameter 'instance'
    return hid_report_descriptor;
}

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen)
{
    (void) instance;
    (void) report_id;
    (void) report_type;
    (void) buffer;
    (void) reqlen;

    return 0;
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize)
{
}

/************* TinyUSB ****************/

static const uint32_t MAGIC = 561348;

static void __attribute__((format (printf, 2, 3))) _log(LogLevel_e lvl, const char * sFmt, ...);

static void _log(LogLevel_e lvl, const char * sFmt, ...)
{
    va_list pArg;

    va_start(pArg, sFmt);
    LOGGER_log_va(MODULE_ID_MOUSE, lvl, sFmt, pArg);
    va_end(pArg);
}

Mouse_t * MOUSE_init(uint8_t bEn)
{
    int ret = 0;
    Mouse_t * pInst = NULL;

    const tinyusb_config_t usbConf =
    {
        .device_descriptor = NULL,
        .string_descriptor = hid_string_descriptor,
        .string_descriptor_count = sizeof(hid_string_descriptor) / sizeof(hid_string_descriptor[0]),
        .external_phy = false,
        .configuration_descriptor = hid_configuration_descriptor,
    };

    LOGGER_setLevel(MODULE_ID_MOUSE, LOG_LVL_DEBUG);

    _log(LOG_LVL_DEBUG, "%s()", __func__);

    pInst = (Mouse_t *) malloc(sizeof(Mouse_t));
    if (!pInst)
    {
        _log(LOG_LVL_ERROR, "%s() malloc %u Bytes for Mouse_t FAILED", __func__, sizeof(Mouse_t));
        goto out_err;
    }

    pInst->magic = MAGIC;
    pInst->bEn = bEn;

    _log(LOG_LVL_DEBUG, "%s() Init USB", __func__);

    ret = tinyusb_driver_install(&usbConf);
    if (ret != ESP_OK)
    {
        _log(LOG_LVL_ERROR, "%s() tinyusb_driver_install FAILED", __func__);
        goto out_free_err;
    }

    return pInst;

out_free_err:
    if (pInst)
        free(pInst);
out_err:
    return NULL;
}

void MOUSE_setEnabled(Mouse_t * pInst, uint8_t bEn)
{
    if (!pInst || pInst->magic != MAGIC)
    {
        _log(LOG_LVL_ERROR, "%s() Bad instance pointer", __func__);
        return;
    }

    pInst->bEn = bEn;

    if (pInst->bEn)
    {
        _log(LOG_LVL_INFO, "Enabled");
    }
    else
    {
        _log(LOG_LVL_INFO, "Disabled");
    }
}

uint8_t MOUSE_getEnabled(Mouse_t * pInst)
{
    if (!pInst || pInst->magic != MAGIC)
    {
        _log(LOG_LVL_ERROR, "%s() Bad instance pointer", __func__);
        return 0U;
    }

    return pInst->bEn;
}

uint8_t MOUSE_move(Mouse_t * pInst, int8_t x, int8_t y)
{
    if (!pInst || pInst->magic != MAGIC)
    {
        _log(LOG_LVL_ERROR, "%s() Bad instance pointer", __func__);
        return 1U;
    }

    if (pInst->bEn == 0U)
    {
        return 0U;
    }

    if ((x == 0U) && (y == 0U))
    {
        return 0U;
    }

    tud_hid_mouse_report(HID_ITF_PROTOCOL_MOUSE, 0x00, x, y, 0, 0);

    return 0U;
}
