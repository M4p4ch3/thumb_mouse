/* GPIO Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>

#include "esp_bit_defs.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "driver/adc.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "tinyusb.h"
#include "class/hid/hid_device.h"

#define BOOT_BUTTON_ID GPIO_NUM_0

#define JOY_HW_ADA 0
#define JOY_HW_GAMEPAD 1
#define JOY_HW JOY_HW_GAMEPAD

#if (JOY_HW == JOY_HW_ADA)
    #define X_IN_MIN        50
    #define X_IN_CENTER     875
    #define X_IN_MAX        1600
    #define X_IN_DEADZONE   50
    #define X_IN_SIGN       -1

    #define Y_IN_MIN        50
    #define Y_IN_CENTER     870
    #define Y_IN_MAX        1600
    #define Y_IN_DEADZONE   50
    #define Y_IN_SIGN       -1
#elif (JOY_HW == JOY_HW_GAMEPAD)
    #define X_IN_MIN        20
    #define X_IN_CENTER     402
    #define X_IN_MAX        800
    #define X_IN_DEADZONE   10
    #define X_IN_SIGN       1

    #define Y_IN_MIN        20
    #define Y_IN_CENTER     413
    #define Y_IN_MAX        800
    #define Y_IN_DEADZONE   5
    #define Y_IN_SIGN       1
#endif // JOY_HW

#define X_OUT_MIN (-100)
#define X_OUT_MAX 100
#define X_OUT_CENTER (X_OUT_MIN + (X_OUT_MAX - X_OUT_MIN) / 2)
#define Y_OUT_MIN (-100)
#define Y_OUT_MAX 100
#define Y_OUT_CENTER (Y_OUT_MIN + (Y_OUT_MAX - Y_OUT_MIN) / 2)

#define DEADZONE 15

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

void clear_screen(void)
{
    for (uint8_t i = 0U; i < 0xFE; i += 1U)
    {
        printf("\n");
    }
}

int32_t range(int32_t val, int32_t min, int32_t max, int32_t med, int32_t dead_med)
{
    if (val < min)
    {
        return min;
    }

    if (val > max)
    {
        return max;
    }

    if (abs(med - val) < dead_med)
    {
        return dead_med;
    }

    return val;
}

int32_t map(int32_t in, int32_t in_min, int32_t in_max, int32_t out_min, int32_t out_max)
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

// void print_axis(int32_t val_raw, char c_name)
// {
//     int32_t val_map = 0;

//     map(val_raw, X_IN_MIN, X_IN_MAX, X_OUT_MIN, X_OUT_MAX, &val_map);

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

#define LOG_LOOP_RATIO 20U

#define MOUSE_SPEED_MAX 30

void print_val_report(int32_t x_raw, int32_t y_raw, int32_t x_map, int32_t y_map, int32_t mouse_x, int32_t mouse_y)
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

void control_mouse_task_main(void * p_arg)
{
    BaseType_t ret = pdFALSE;
    uint16_t loop_cnt = 0U;
    SemaphoreHandle_t * p_sem_send_hid_report = NULL;

    if (!p_arg)
    {
        return;
    }

    p_sem_send_hid_report = (SemaphoreHandle_t *) p_arg;

    while (true)
    {
        ret = xQueueSemaphoreTake(*p_sem_send_hid_report, portMAX_DELAY);
        if (ret != pdTRUE)
        {
            printf("ERROR %s() xQueueSemaphoreTake FAILED\n", __func__);
            continue;
        }

        static const uint8_t ACQ_NB = 64U;

        uint32_t x_raw = 0;
        uint32_t y_raw = 0;
        int8_t x_map = 0;
        int8_t y_map = 0;
        int8_t mouse_x = 0;
        int8_t mouse_y = 0;
        int8_t sign = 0;

        for (uint8_t acq_idx = 0U; acq_idx < ACQ_NB; acq_idx += 1U)
        {
            x_raw += (uint32_t) adc1_get_raw(ADC1_CHANNEL_0);
            y_raw += (uint32_t) adc1_get_raw(ADC1_CHANNEL_1);
        }

        x_raw /= ACQ_NB;
        y_raw /= ACQ_NB;

        if (x_raw < X_IN_CENTER)
        {
            x_map = (int8_t) map(x_raw, X_IN_MIN, X_IN_CENTER - X_IN_DEADZONE / 2, X_OUT_MIN, X_OUT_CENTER);
        }
        else
        {
            x_map = (int8_t) map(x_raw, X_IN_CENTER - X_IN_DEADZONE / 2, X_IN_MAX, X_OUT_CENTER, X_OUT_MAX);
        }

        if (y_raw < Y_IN_CENTER)
        {
            y_map = (int8_t) map(y_raw, Y_IN_MIN, Y_IN_CENTER - Y_IN_DEADZONE / 2, Y_OUT_MIN, Y_OUT_CENTER);
        }
        else
        {
            y_map = (int8_t) map(y_raw, Y_IN_CENTER - Y_IN_DEADZONE / 2, Y_IN_MAX, Y_OUT_CENTER, Y_OUT_MAX);
        }

        if (abs(x_map - X_OUT_CENTER) >= DEADZONE)
        {
            sign = X_IN_SIGN * x_map / abs(x_map);
            mouse_x = sign * (abs(x_map - X_OUT_CENTER) - DEADZONE) / 3;
            if (abs(mouse_x) > MOUSE_SPEED_MAX)
            {
                mouse_x = sign * MOUSE_SPEED_MAX;
            }
        }

        if (abs(y_map - Y_OUT_CENTER) >= DEADZONE)
        {
            sign = Y_IN_SIGN * y_map / abs(y_map);
            mouse_y = sign * (abs(y_map - Y_OUT_CENTER) - DEADZONE) / 3;
            if (abs(mouse_y) > MOUSE_SPEED_MAX)
            {
                mouse_y = sign * MOUSE_SPEED_MAX;
            }
        }

        tud_hid_mouse_report(HID_ITF_PROTOCOL_MOUSE, 0x00, mouse_x, mouse_y, 0, 0);

        if ((LOG_LOOP_RATIO < 0xFF) && (loop_cnt == LOG_LOOP_RATIO))
        {
            print_val_report(x_raw, y_raw, x_map, y_map, mouse_x, mouse_y);
            loop_cnt = 0U;
        }

        loop_cnt += 1U;
    }
}

static void periodic_timer_callback(void * p_arg)
{
    BaseType_t ret = pdFALSE;
    SemaphoreHandle_t * p_sem_send_hid_report = NULL;

    if (!p_arg)
    {
        printf("ERROR %s() p_arg NULL\n", __func__);
        return;
    }

    p_sem_send_hid_report = (SemaphoreHandle_t *) p_arg;
    ret = xSemaphoreGive(*p_sem_send_hid_report);
    if (ret != pdTRUE)
    {
        printf("ERROR %s() xSemaphoreGive FAILED\n", __func__);
        return;
    }
}

#define MS_PER_S 1000U
#define US_PER_MS 1000U
#define US_PER_S (US_PER_MS * MS_PER_S)
#define MOUSE_REPORT_FREQ_HZ 60U
#define MOUSE_REPORT_PERIOD_US (US_PER_S / MOUSE_REPORT_FREQ_HZ)

void app_main(void)
{
    esp_err_t ret = ESP_OK;

    printf("DEBUG init HW\n");

    printf("DEBUG init boot button\n");

    const gpio_config_t boot_button_config = {
        .pin_bit_mask = BIT64(BOOT_BUTTON_ID),
        .mode = GPIO_MODE_INPUT,
        .intr_type = GPIO_INTR_DISABLE,
        .pull_up_en = true,
        .pull_down_en = false,
    };

    ret = gpio_config(&boot_button_config);
    if (ret != ESP_OK)
    {
        printf("ERROR gpio_config FAILED\n");
        return;
    }

    printf("DEBUG init boot button OK\n");

    printf("DEBUG init ADC\n");

    adc1_config_width(ADC_WIDTH_BIT_13);
    adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_DB_0);
    adc1_config_channel_atten(ADC1_CHANNEL_1, ADC_ATTEN_DB_0);

    printf("DEBUG init ADC OK\n");

    printf("DEBUG init USB\n");

    const tinyusb_config_t tusb_cfg = {
        .device_descriptor = NULL,
        .string_descriptor = hid_string_descriptor,
        .string_descriptor_count = sizeof(hid_string_descriptor) / sizeof(hid_string_descriptor[0]),
        .external_phy = false,
        .configuration_descriptor = hid_configuration_descriptor,
    };

    ret = tinyusb_driver_install(&tusb_cfg);
    if (ret != ESP_OK)
    {
        printf("ERROR tinyusb_driver_install FAILED\n");
        return;
    }

    printf("DEBUG USB init OK\n");

    printf("DEBUG init HW OK\n");

    SemaphoreHandle_t sem_send_hid_report = NULL;
    sem_send_hid_report = xSemaphoreCreateBinary();
    if (!sem_send_hid_report)
    {
        printf("ERROR xSemaphoreCreateBinary FAILED\n");
        return;
    }

    printf("DEBUG init timer\n");

    const esp_timer_create_args_t periodic_timer_args = {
        .callback = &periodic_timer_callback,
        .arg = (void *) sem_send_hid_report,
    };

    esp_timer_handle_t periodic_timer;
    ret = esp_timer_create(&periodic_timer_args, &periodic_timer);
    if (ret != ESP_OK)
    {
        printf("ERROR esp_timer_create FAILED\n");
        return;
    }

    printf("DEBUG init timer OK\n");

    printf("DEBUG init task\n");

    TaskHandle_t task_control_mouse = NULL;
    xTaskCreate(control_mouse_task_main, "control_mouse_task",
        0x1000U, (void *) &sem_send_hid_report, tskIDLE_PRIORITY, &task_control_mouse);
    if (!task_control_mouse)
    {
        printf("ERROR xTaskCreate FAILED\n");
        return;
    }

    printf("DEBUG init task OK\n");

    bool boot_btn_val = false;
    bool mouse_enabled = false;

    while (1)
    {
        if (!gpio_get_level(BOOT_BUTTON_ID) != boot_btn_val)
        {
            boot_btn_val = !boot_btn_val;
            if (boot_btn_val)
            {
                mouse_enabled = !mouse_enabled;
                if (mouse_enabled)
                {
                    printf("INFO enable mouse\n");
                    printf("DEUG start timer\n");
                    ret = esp_timer_start_periodic(periodic_timer, MOUSE_REPORT_PERIOD_US);
                    if (ret != ESP_OK)
                    {
                        printf("ERROR esp_timer_start_periodic FAILED\n");
                        return;
                    }
                }
                else
                {
                    printf("INFO disable mouse\n");
                    printf("DEBUG stop timer\n");
                    ret = esp_timer_stop(periodic_timer);
                    if (ret != ESP_OK)
                    {
                        printf("ERROR esp_timer_stop FAILED\n");
                        return;
                    }
                }
            }
        }

        vTaskDelay(100U / portTICK_PERIOD_MS);
    }
}
