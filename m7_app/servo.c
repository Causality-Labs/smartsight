#include "servo.h"
#include "fsl_pwm.h"


#define SERVO_PWM_ROOT_HZ  24000000U /* PWM root clock from OSC 24M */
#define SERVO_PWM_CLOCK_HZ 1000000U  /* Counter after prescaler: 1 count = 1 us */
#define SERVO_FRAME_HZ     50U

/* Hobby servo (SG90-style): 0.5 ms pulse = 0 deg, 2.4 ms pulse = 180 deg */
#define SERVO_PULSE_MIN_US  500U
#define SERVO_PULSE_MAX_US  2400U
#define SERVO_ANGLE_MAX_DEG 180.0f

static PWM_Type *const PWM_ADDRESSES[PWM_CHANEL_MAX] = {PWM1, PWM2, PWM3, PWM4};
static const clock_root_control_t PWM_CLOCK_ROOTS[PWM_CHANEL_MAX] = {kCLOCK_RootPwm1, kCLOCK_RootPwm2,
                                                                     kCLOCK_RootPwm3, kCLOCK_RootPwm4};

static uint32_t servo_us_to_counts(uint32_t time_us)
{
    return (uint32_t)(((uint64_t)time_us * SERVO_PWM_CLOCK_HZ + 500000U) / 1000000U);
}

static uint32_t servo_period_us_to_register(uint32_t period_us)
{
    return servo_us_to_counts(period_us) - 2U;
}

static uint32_t servo_angle_to_pulse_us(float angle_deg)
{
    return SERVO_PULSE_MIN_US +
           (uint32_t)((angle_deg / SERVO_ANGLE_MAX_DEG) * (SERVO_PULSE_MAX_US - SERVO_PULSE_MIN_US) + 0.5f);
}

servo_status_t servo_init(servo_t *servo, pwm_channel_t pwm_channel, float start_angle_deg)
{
    pwm_config_t pwm_config ={0};

    if (servo == NULL || pwm_channel >= PWM_CHANEL_MAX)
        return SERVO_STATUS_ERROR;


    servo->pwm_channel = pwm_channel;
    servo->enabled = false;

    PWM_Type *base = PWM_ADDRESSES[servo->pwm_channel];
    clock_root_control_t clock_root = PWM_CLOCK_ROOTS[pwm_channel];

    /* PWM root clock = 24 MHz OSC. PWM_Init ungates it */
    CLOCK_SetRootMux(clock_root, kCLOCK_PwmRootmuxOsc24M);
    CLOCK_SetRootDivider(clock_root, 1U, 1U);

    PWM_GetDefaultConfig(&pwm_config);
    pwm_config.clockSource = kPWM_HighFrequencyClock;
    pwm_config.prescale    = (SERVO_PWM_ROOT_HZ / SERVO_PWM_CLOCK_HZ) - 1U; /* Divide by prescale + 1 */
    if (PWM_Init(base, &pwm_config) != kStatus_Success)
        return SERVO_STATUS_ERROR;

    /* 20 ms frame: 20000 counts at 1 MHz is 50 Hz */
    PWM_SetPeriodValue(base, servo_period_us_to_register(1000000U / SERVO_FRAME_HZ));
    PWM_StartTimer(base);

    servo->enabled = true;

    return servo_set_position(servo, start_angle_deg);
}

servo_status_t servo_set_position(servo_t *servo, float angle_deg)
{
    if(servo == NULL || !servo->enabled)
        return SERVO_STATUS_ERROR;

    if(angle_deg < 0.0f || angle_deg > SERVO_ANGLE_MAX_DEG)
        return SERVO_STATUS_ERROR;

    uint32_t pulse_us = servo_angle_to_pulse_us(angle_deg);
    PWM_Type *base = PWM_ADDRESSES[servo->pwm_channel];

    PWM_SetSampleValue(base, servo_us_to_counts(pulse_us));

    return SERVO_STATUS_OK;
}
