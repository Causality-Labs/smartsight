#include <stdio.h>
#include "servo.h"
#include "fsl_pwm.h"


#define SERVO_PWM_ROOT_HZ  24000000U /* PWM root clock from OSC 24M */
#define SERVO_PWM_CLOCK_HZ 1000000U  /* Counter after prescaler: 1 count = 1 us */
#define SERVO_PERIOD_US    20000U    /* 20 ms frame = 50 Hz */

#define SERVO_ANGLE_MAX_CDEG 18000   /* Full mechanical travel the pulse range maps onto */

typedef struct
{
    PWM_Type            *base;
    clock_root_control_t clock_root;
    bool                 in_use; /* One servo per channel. Not thread-safe: init servos from a single task */
} servo_pwm_channel_t;

static servo_pwm_channel_t PWM_CHANNELS[PWM_CHANEL_MAX] = {
    {PWM1, kCLOCK_RootPwm1, false},
    {PWM2, kCLOCK_RootPwm2, false},
    {PWM3, kCLOCK_RootPwm3, false},
    {PWM4, kCLOCK_RootPwm4, false},
};

static uint32_t servo_us_to_counts(uint32_t time_us)
{
    return (uint32_t)(((uint64_t)time_us * SERVO_PWM_CLOCK_HZ + 500000U) / 1000000U);
}

static uint32_t servo_period_us_to_register(uint32_t period_us)
{
    return servo_us_to_counts(period_us) - 2U;
}

static int32_t servo_clamp(int32_t value, int32_t min, int32_t max)
{
    if (value < min)
        return min;

    if (value > max)
        return max;

    return value;
}

static uint32_t servo_angle_to_pulse_us(const servo_config_t *config, int32_t angle_cdeg)
{
    return config->pulse_min_us +
           (uint32_t)((angle_cdeg * (config->pulse_max_us - config->pulse_min_us) + (SERVO_ANGLE_MAX_CDEG / 2)))/ SERVO_ANGLE_MAX_CDEG;
}

servo_status_t servo_init(servo_t *servo, const servo_config_t *config, int32_t start_angle_cdeg)
{
    pwm_config_t pwm_config = {0};

    if (servo == NULL || config == NULL)
        return SERVO_STATUS_ERROR;

    if (config->channel >= PWM_CHANEL_MAX)
        return SERVO_STATUS_ERROR;

    if (config->pulse_min_us >= config->pulse_max_us)
        return SERVO_STATUS_ERROR;

    if (config->min_angle_cdeg < 0 || config->max_angle_cdeg > SERVO_ANGLE_MAX_CDEG)
        return SERVO_STATUS_ERROR;

    if (config->min_angle_cdeg >= config->max_angle_cdeg)
        return SERVO_STATUS_ERROR;

    servo_pwm_channel_t *ch = &PWM_CHANNELS[config->channel];

    if (ch->in_use)
        return SERVO_STATUS_BUSY;

    ch->in_use = true;

    /* Keep a copy so the caller's config can go out of scope */
    servo->config      = *config;
    servo->angle_cdeg   = 0;
    servo->initialized = false;
    servo->enabled     = false;

    CLOCK_SetRootMux(ch->clock_root, kCLOCK_PwmRootmuxOsc24M);
    CLOCK_SetRootDivider(ch->clock_root, 1U, 1U);

    PWM_GetDefaultConfig(&pwm_config);
    pwm_config.clockSource = kPWM_HighFrequencyClock;
    pwm_config.prescale    = (SERVO_PWM_ROOT_HZ / SERVO_PWM_CLOCK_HZ) - 1U;
    if (PWM_Init(ch->base, &pwm_config) != kStatus_Success)
    {
        ch->in_use = false; /* Release so it can be retried */
        return SERVO_STATUS_ERROR;
    }

    PWM_SetPeriodValue(ch->base, servo_period_us_to_register(SERVO_PERIOD_US));

    servo->initialized = true;

    /* Queue the start position BEFORE starting, so the very first pulse is correct */
    servo->enabled        = true;
    servo_status_t status = servo_set_position(servo, start_angle_cdeg);
    PWM_StartTimer(ch->base);

    return status;
}

servo_status_t servo_set_position(servo_t *servo, int32_t angle_cdeg)
{
    if (servo == NULL || !servo->enabled)
        return SERVO_STATUS_ERROR;

    angle_cdeg = servo_clamp(angle_cdeg, servo->config.min_angle_cdeg, servo->config.max_angle_cdeg);

    int32_t physical_cdeg = servo_clamp(angle_cdeg + servo->config.offset_cdeg, 0, SERVO_ANGLE_MAX_CDEG);

    uint32_t pulse_us = servo_angle_to_pulse_us(&servo->config, physical_cdeg);
    servo_pwm_channel_t *ch = &PWM_CHANNELS[servo->config.channel];


    PWM_SetSampleValue(ch->base, servo_us_to_counts(pulse_us));
    servo->angle_cdeg = angle_cdeg;

    return SERVO_STATUS_OK;
}

int32_t servo_get_position(const servo_t *servo)
{
    if (servo == NULL)
        return 0;

    return servo->angle_cdeg;
}

servo_status_t servo_enable(servo_t *servo)
{
    if (servo == NULL || !servo->initialized)
        return SERVO_STATUS_ERROR;

    if (servo->enabled)
        return SERVO_STATUS_OK;

    servo_pwm_channel_t *ch = &PWM_CHANNELS[servo->config.channel];

    servo->enabled        = true;
    servo_status_t status = servo_set_position(servo, servo->angle_cdeg);
    PWM_StartTimer(ch->base);

    return status;
}

servo_status_t servo_disable(servo_t *servo)
{
    if (servo == NULL || !servo->initialized)
        return SERVO_STATUS_ERROR;

    servo_pwm_channel_t *ch = &PWM_CHANNELS[servo->config.channel];

    PWM_StopTimer(ch->base);
    servo->enabled = false;

    return SERVO_STATUS_OK;
}

servo_status_t servo_deinit(servo_t *servo)
{
    if (servo == NULL || !servo->initialized)
        return SERVO_STATUS_ERROR;

    servo_pwm_channel_t *ch = &PWM_CHANNELS[servo->config.channel];

    PWM_StopTimer(ch->base);
    PWM_Deinit(ch->base);
    ch->in_use         = false;
    servo->initialized = false;
    servo->enabled     = false;

    return SERVO_STATUS_OK;
}
