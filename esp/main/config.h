
#ifndef CONFIG_H
#define CONFIG_H

// GPIO1
#define JOY_HW_X_CHAN ADC1_CHANNEL_0
// GPIO2
#define JOY_HW_Y_CHAN ADC1_CHANNEL_1

#define JOY_HW_ADA 0
#define JOY_HW_GAMEPAD 1
#define JOY_HW JOY_HW_GAMEPAD

#if (JOY_HW == JOY_HW_ADA)
    #define JOY_X_MIN        50
    #define JOY_X_CENTER     875
    #define JOY_X_MAX        1600
    #define JOY_X_DEADZONE   50
    #define JOY_X_SIGN       -1

    #define JOY_Y_MIN        50
    #define JOY_Y_CENTER     870
    #define JOY_Y_MAX        1600
    #define JOY_Y_DEADZONE   50
    #define JOY_Y_SIGN       -1
#elif (JOY_HW == JOY_HW_GAMEPAD)
    #define JOY_X_MIN        20
    #define JOY_X_CENTER     412
    #define JOY_X_MAX        800
    #define JOY_X_DEADZONE   10
    #define JOY_X_SIGN       -1

    #define JOY_Y_MIN        20
    #define JOY_Y_CENTER     400
    #define JOY_Y_MAX        800
    #define JOY_Y_DEADZONE   5
    #define JOY_Y_SIGN       1
#endif // JOY_HW

#define DEADZONE 15

// Oversampling
#define ACQ_NB 10U

#define MOUSE_REPORT_FREQ_HZ 60U
#define MOUSE_SPEED_MAX 30

#define MOUSE_LOG_LOOP_NB 20U
#define CTRL_LOG_LOOP_NB (MOUSE_LOG_LOOP_NB * 4U)

#endif // CONFIG_H
