#include <stdint.h>
#include <stddef.h>
#include "pan_tilt_mount.h"

static float clamp(float current_deg, float min_deg, float max_deg)
{
    if (current_deg < min_deg)
        return min_deg;

    if (current_deg > max_deg)
        return max_deg;

    return current_deg;
}

static float pan_tilt_mount_limit_step(float current_deg, float target_deg, float max_step_deg)
{
    float step = target_deg - current_deg;

    if (step > max_step_deg)
        step = max_step_deg;
    else if (step < -max_step_deg)
        step = -max_step_deg;

    return current_deg + step;
}

pan_tilt_mount_status_t pan_tilt_mount_init(pan_tilt_mount_t *mount,
                                            const pan_tilt_mount_config_t *config)
{
    if (mount == NULL || config == NULL)
        return PAN_TILT_MOUNT_STATUS_ERROR;

    if (config->max_pan_step_deg <= 0.0f || config->max_tilt_step_deg <= 0.0f)
        return PAN_TILT_MOUNT_STATUS_ERROR;

    if (servo_init(&mount->pan, &config->pan, config->pan_home_deg) != SERVO_STATUS_OK)
        return PAN_TILT_MOUNT_STATUS_ERROR;

    if (servo_init(&mount->tilt, &config->tilt, config->tilt_home_deg) != SERVO_STATUS_OK)
    {
        servo_deinit(&mount->pan);
        return PAN_TILT_MOUNT_STATUS_ERROR;
    }

    mount->pan_home_deg  = config->pan_home_deg;
    mount->tilt_home_deg = config->tilt_home_deg;
    mount->max_pan_step_deg  = config->max_pan_step_deg;
    mount->max_tilt_step_deg  = config->max_tilt_step_deg;

    mount->pan_target_deg  = servo_get_position(&mount->pan);
    mount->tilt_target_deg = servo_get_position(&mount->tilt);

    return PAN_TILT_MOUNT_STATUS_OK;
}

pan_tilt_mount_status_t pan_tilt_mount_target_position(pan_tilt_mount_t *mount,
                                                       float pan_position,
                                                       float tilt_position)
{
    if (mount == NULL)
        return PAN_TILT_MOUNT_STATUS_ERROR;

    mount->pan_target_deg  = clamp(pan_position, mount->pan.config.min_angle_deg,
                                   mount->pan.config.max_angle_deg);
    mount->tilt_target_deg = clamp(tilt_position, mount->tilt.config.min_angle_deg,
                                   mount->tilt.config.max_angle_deg);

    return PAN_TILT_MOUNT_STATUS_OK;
}

pan_tilt_mount_status_t pan_tilt_mount_pan_target(pan_tilt_mount_t *mount, float pan_deg)
{
    if (mount == NULL)
        return PAN_TILT_MOUNT_STATUS_ERROR;

    mount->pan_target_deg = clamp(pan_deg, mount->pan.config.min_angle_deg,
                                  mount->pan.config.max_angle_deg);

    return PAN_TILT_MOUNT_STATUS_OK;
}

pan_tilt_mount_status_t pan_tilt_mount_tilt_target(pan_tilt_mount_t *mount, float tilt_deg)
{
    if (mount == NULL)
        return PAN_TILT_MOUNT_STATUS_ERROR;

    mount->tilt_target_deg = clamp(tilt_deg, mount->tilt.config.min_angle_deg,
                                   mount->tilt.config.max_angle_deg);

    return PAN_TILT_MOUNT_STATUS_OK;
}

bool pan_tilt_mount_pan_at_target(const pan_tilt_mount_t *mount)
{
    if (mount == NULL)
        return false;

    return mount->pan.angle_deg == mount->pan_target_deg;
}

bool pan_tilt_mount_tilt_at_target(const pan_tilt_mount_t *mount)
{
    if (mount == NULL)
        return false;

    return mount->tilt.angle_deg == mount->tilt_target_deg;
}

pan_tilt_move_status_t pan_tilt_mount_move(pan_tilt_mount_t *mount)
{
    if (mount == NULL)
        return PAN_TILT_MOVING_ERROR;

    bool pan_reached = false;
    bool tilt_reached = false;

    float pan_next  = pan_tilt_mount_limit_step(mount->pan.angle_deg, 
                                                mount->pan_target_deg,
                                                mount->max_pan_step_deg);

    float tilt_next = pan_tilt_mount_limit_step(mount->tilt.angle_deg,
                                                mount->tilt_target_deg,
                                                mount->max_tilt_step_deg);

    if (servo_set_position(&mount->pan, pan_next) != SERVO_STATUS_OK)
        return PAN_TILT_MOVING_ERROR;

    if (servo_set_position(&mount->tilt, tilt_next) != SERVO_STATUS_OK)
        return PAN_TILT_MOVING_ERROR;

    if (mount->pan.angle_deg  == mount->pan_target_deg)
        pan_reached = true;

    if (mount->tilt.angle_deg  == mount->tilt_target_deg)
        tilt_reached = true;

    if (pan_reached && tilt_reached)
        return PAN_TILT_MOVING_COMPLETE;
    else
        return PAN_TILT_MOVING;
}

pan_tilt_mount_status_t pan_tilt_mount_home(pan_tilt_mount_t *mount)
{
    if (mount == NULL)
        return PAN_TILT_MOUNT_STATUS_ERROR;

    return pan_tilt_mount_target_position(mount, mount->pan_home_deg, mount->tilt_home_deg);
}


pan_tilt_mount_status_t pan_tilt_mount_enable(pan_tilt_mount_t *mount)
{
    if (mount == NULL)
        return PAN_TILT_MOUNT_STATUS_ERROR;

    servo_status_t pan_status  = servo_enable(&mount->pan);
    servo_status_t tilt_status = servo_enable(&mount->tilt);

    if (pan_status != SERVO_STATUS_OK || tilt_status != SERVO_STATUS_OK)
        return PAN_TILT_MOUNT_STATUS_ERROR;

    return PAN_TILT_MOUNT_STATUS_OK;
}

pan_tilt_mount_status_t pan_tilt_mount_stop(pan_tilt_mount_t *mount)
{
    if (mount == NULL)
        return PAN_TILT_MOUNT_STATUS_ERROR;

    return pan_tilt_mount_target_position(mount, mount->pan.angle_deg, mount->tilt.angle_deg);
}
pan_tilt_mount_status_t pan_tilt_mount_disable(pan_tilt_mount_t *mount)
{
    if (mount == NULL)
        return PAN_TILT_MOUNT_STATUS_ERROR;

    servo_status_t pan_status  = servo_disable(&mount->pan);
    servo_status_t tilt_status = servo_disable(&mount->tilt);

    if (pan_status != SERVO_STATUS_OK || tilt_status != SERVO_STATUS_OK)
        return PAN_TILT_MOUNT_STATUS_ERROR;

    return PAN_TILT_MOUNT_STATUS_OK;
}

pan_tilt_mount_status_t pan_tilt_mount_deinit(pan_tilt_mount_t *mount)
{
    if (mount == NULL)
        return PAN_TILT_MOUNT_STATUS_ERROR;

    servo_status_t pan_status  = servo_deinit(&mount->pan);
    servo_status_t tilt_status = servo_deinit(&mount->tilt);

    if (pan_status != SERVO_STATUS_OK || tilt_status != SERVO_STATUS_OK)
        return PAN_TILT_MOUNT_STATUS_ERROR;

    return PAN_TILT_MOUNT_STATUS_OK;
}
