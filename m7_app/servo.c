#include <stdio.h>
#include "servo.h"
#include "fsl_pwm.h"


#define SERVO_PWM_ROOT_HZ  24000000U /* PWM root clock from OSC 24M */
#define SERVO_PWM_CLOCK_HZ 1000000U  /* Counter after prescaler: 1 count = 1 us */
#define SERVO_PERIOD_US    20000U    /* 20 ms frame = 50 Hz */

#define SERVO_ANGLE_MAX_DEG 180.0f   /* Full mechanical travel the pulse range maps onto */

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

static float servo_clamp(float value, float min, float max)
{
    if (value < min)
        return min;

    if (value > max)
        return max;

    return value;
}

static uint32_t servo_angle_to_pulse_us(const servo_config_t *config, float angle_deg)
{
    return config->pulse_min_us +
           (uint32_t)((angle_deg / SERVO_ANGLE_MAX_DEG) * (config->pulse_max_us - config->pulse_min_us) + 0.5f);
}

servo_status_t servo_init(servo_t *servo, const servo_config_t *config, float start_angle_deg)
{
    pwm_config_t pwm_config = {0};

    if (servo == NULL || config == NULL)
        return SERVO_STATUS_ERROR;

    if (config->channel >= PWM_CHANEL_MAX)
        return SERVO_STATUS_ERROR;

    if (config->pulse_min_us >= config->pulse_max_us)
        return SERVO_STATUS_ERROR;

    if (config->min_angle_deg < 0.0f || config->max_angle_deg > SERVO_ANGLE_MAX_DEG)
        return SERVO_STATUS_ERROR;

    if (config->min_angle_deg >= config->max_angle_deg)
        return SERVO_STATUS_ERROR;

    servo_pwm_channel_t *ch = &PWM_CHANNELS[config->channel];

    if (ch->in_use)
        return SERVO_STATUS_BUSY;

    ch->in_use = true;

    /* Keep a copy so the caller's config can go out of scope */
    servo->config      = *config;
    servo->angle_deg   = 0.0f;
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
    servo_status_t status = servo_set_position(servo, start_angle_deg);
    PWM_StartTimer(ch->base);

    return status;
}

servo_status_t servo_set_position(servo_t *servo, float angle_deg)
{
    if (servo == NULL || !servo->enabled)
        return SERVO_STATUS_ERROR;

    angle_deg = servo_clamp(angle_deg, servo->config.min_angle_deg, servo->config.max_angle_deg);

    float physical_deg = servo_clamp(angle_deg + servo->config.offset_deg, 0.0f, SERVO_ANGLE_MAX_DEG);

    uint32_t pulse_us = servo_angle_to_pulse_us(&servo->config, physical_deg);
    servo_pwm_channel_t *ch = &PWM_CHANNELS[servo->config.channel];


    PWM_SetSampleValue(ch->base, servo_us_to_counts(pulse_us));
    servo->angle_deg = angle_deg;

    return SERVO_STATUS_OK;
}

float servo_get_position(const servo_t *servo)
{
    if (servo == NULL)
        return 0.0f;

    return servo->angle_deg;
}

servo_status_t servo_enable(servo_t *servo)
{
    if (servo == NULL || !servo->initialized)
        return SERVO_STATUS_ERROR;

    if (servo->enabled)
        return SERVO_STATUS_OK;

    servo_pwm_channel_t *ch = &PWM_CHANNELS[servo->config.channel];

    servo->enabled        = true;
    servo_status_t status = servo_set_position(servo, servo->angle_deg);
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
