#ifndef SERVO_H
#define SERVO_H

#include <stdint.h>
#include <stdbool.h>

#define KHZ 1000
#define MHZ 1000000


typedef enum 
{
    SERVO_STATUS_OK = 0,
    SERVO_STATUS_ERROR,
    SERVO_STATUS_BUSY    /* PWM channel already owned by another servo */
} servo_status_t;

typedef enum 
{
    PWM_CHANNEL_1 = 0,
    PWM_CHANNEL_2,
    PWM_CHANNEL_3,
    PWM_CHANNEL_4,
    PWM_CHANEL_MAX
} pwm_channel_t;

typedef struct {
    pwm_channel_t channel;
    uint16_t pulse_min_us;
    uint16_t pulse_max_us;
    float    min_angle_deg;
    float max_angle_deg;
    float    offset_deg;
} servo_config_t;


typedef struct {
    servo_config_t config;
    float angle_deg;
    bool initialized;
    bool enabled;
} servo_t;


servo_status_t servo_init(servo_t *servo, const servo_config_t *config, float start_angle_deg);
servo_status_t servo_set_position(servo_t *servo, float angle_deg);
float          servo_get_position(const servo_t *servo);
servo_status_t servo_enable(servo_t *servo);
servo_status_t servo_disable(servo_t *servo);
servo_status_t servo_deinit(servo_t *servo);

#endif /* SERVO_H */
