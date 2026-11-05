#ifndef SERVO_H
#define SERVO_H

#include <stdint.h>
#include <stdbool.h>

#define KHZ 1000
#define MHZ 1000000


typedef enum 
{
    SERVO_STATUS_OK = 0,
    SERVO_STATUS_ERROR
} servo_status_t; 

typedef enum 
{
    PWM_CHANNEL_1 = 0,
    PWM_CHANNEL_2,
    PWM_CHANNEL_3,
    PWM_CHANNEL_4,
    PWM_CHANEL_MAX
} pwm_channel_t; 

typedef struct
{
    pwm_channel_t pwm_channel;
    bool enabled;
} servo_t;


servo_status_t servo_init(servo_t *servo, pwm_channel_t pwm_channel, float start_angle_deg);
servo_status_t servo_set_position(servo_t *servo, float angle_deg);

// void servo_set_limit(servo_t *handler, float min, float max);

// void servo_set_offset(servo_t *handler, float angle_deg);

// float servo_duty_ms(float angle_deg);

// void servo_set_position(servo_t *handler, float angle_deg);


// void servo_sweep(servo_t *handler, float start_angle_deg, float end_angle_deg, float step, uint32_t step_delay_ms);

#endif /* SERVO_H */
