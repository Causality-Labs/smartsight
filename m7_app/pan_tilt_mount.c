#include <stdint.h>
#include <stddef.h>
#include "pan_tilt_mount.h"

static int32_t clamp(int32_t current_cdeg, int32_t min_cdeg, int32_t max_cdeg)
{
    if (current_cdeg < min_cdeg)
        return min_cdeg;

    if (current_cdeg > max_cdeg)
        return max_cdeg;

    return current_cdeg;
}

static int32_t pan_tilt_mount_limit_step(int32_t current_cdeg, int32_t target_cdeg, int32_t max_step_cdeg)
{
    int32_t step = target_cdeg - current_cdeg;

    if (step > max_step_cdeg)
        step = max_step_cdeg;
    else if (step < -max_step_cdeg)
        step = -max_step_cdeg;

    return current_cdeg + step;
}

pan_tilt_mount_status_t pan_tilt_mount_init(pan_tilt_mount_t *mount,
                                            const pan_tilt_mount_config_t *config)
{
    if (mount == NULL || config == NULL)
        return PAN_TILT_MOUNT_STATUS_ERROR;

    if (config->max_pan_step_cdeg <= 0 || config->max_tilt_step_cdeg <= 0)
        return PAN_TILT_MOUNT_STATUS_ERROR;

    if (servo_init(&mount->pan, &config->pan, config->pan_home_cdeg) != SERVO_STATUS_OK)
        return PAN_TILT_MOUNT_STATUS_ERROR;

    if (servo_init(&mount->tilt, &config->tilt, config->tilt_home_cdeg) != SERVO_STATUS_OK)
    {
        servo_deinit(&mount->pan);
        return PAN_TILT_MOUNT_STATUS_ERROR;
    }

    mount->pan_home_cdeg  = config->pan_home_cdeg;
    mount->tilt_home_cdeg = config->tilt_home_cdeg;
    mount->max_pan_step_cdeg  = config->max_pan_step_cdeg;
    mount->max_tilt_step_cdeg  = config->max_tilt_step_cdeg;

    mount->pan_target_cdeg  = servo_get_position(&mount->pan);
    mount->tilt_target_cdeg = servo_get_position(&mount->tilt);

    return PAN_TILT_MOUNT_STATUS_OK;
}

pan_tilt_mount_status_t pan_tilt_mount_target_position(pan_tilt_mount_t *mount,
                                                       int32_t pan_position,
                                                       int32_t tilt_position)
{
    if (mount == NULL)
        return PAN_TILT_MOUNT_STATUS_ERROR;

    mount->pan_target_cdeg  = clamp(pan_position, mount->pan.config.min_angle_cdeg,
                                   mount->pan.config.max_angle_cdeg);
    mount->tilt_target_cdeg = clamp(tilt_position, mount->tilt.config.min_angle_cdeg,
                                   mount->tilt.config.max_angle_cdeg);

    return PAN_TILT_MOUNT_STATUS_OK;
}

pan_tilt_mount_status_t pan_tilt_mount_pan_target(pan_tilt_mount_t *mount, int32_t pan_cdeg)
{
    if (mount == NULL)
        return PAN_TILT_MOUNT_STATUS_ERROR;

    mount->pan_target_cdeg = clamp(pan_cdeg, mount->pan.config.min_angle_cdeg,
                                  mount->pan.config.max_angle_cdeg);

    return PAN_TILT_MOUNT_STATUS_OK;
}

pan_tilt_mount_status_t pan_tilt_mount_tilt_target(pan_tilt_mount_t *mount, int32_t tilt_cdeg)
{
    if (mount == NULL)
        return PAN_TILT_MOUNT_STATUS_ERROR;

    mount->tilt_target_cdeg = clamp(tilt_cdeg, mount->tilt.config.min_angle_cdeg,
                                   mount->tilt.config.max_angle_cdeg);

    return PAN_TILT_MOUNT_STATUS_OK;
}

bool pan_tilt_mount_pan_at_target(const pan_tilt_mount_t *mount)
{
    if (mount == NULL)
        return false;

    return mount->pan.angle_cdeg == mount->pan_target_cdeg;
}

bool pan_tilt_mount_tilt_at_target(const pan_tilt_mount_t *mount)
{
    if (mount == NULL)
        return false;

    return mount->tilt.angle_cdeg == mount->tilt_target_cdeg;
}

pan_tilt_move_status_t pan_tilt_mount_move(pan_tilt_mount_t *mount)
{
    if (mount == NULL)
        return PAN_TILT_MOVING_ERROR;

    bool pan_reached = false;
    bool tilt_reached = false;

    int32_t pan_next  = pan_tilt_mount_limit_step(mount->pan.angle_cdeg, 
                                                mount->pan_target_cdeg,
                                                mount->max_pan_step_cdeg);

    int32_t tilt_next = pan_tilt_mount_limit_step(mount->tilt.angle_cdeg,
                                                mount->tilt_target_cdeg,
                                                mount->max_tilt_step_cdeg);

    if (servo_set_position(&mount->pan, pan_next) != SERVO_STATUS_OK)
        return PAN_TILT_MOVING_ERROR;

    if (servo_set_position(&mount->tilt, tilt_next) != SERVO_STATUS_OK)
        return PAN_TILT_MOVING_ERROR;

    if (mount->pan.angle_cdeg  == mount->pan_target_cdeg)
        pan_reached = true;

    if (mount->tilt.angle_cdeg  == mount->tilt_target_cdeg)
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

    return pan_tilt_mount_target_position(mount, mount->pan_home_cdeg, mount->tilt_home_cdeg);
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

    return pan_tilt_mount_target_position(mount, mount->pan.angle_cdeg, mount->tilt.angle_cdeg);
}

pan_tilt_mount_status_t pan_tilt_mount_get_curr_pos(pan_tilt_mount_t *mount, int32_t *pan_pos, int32_t *tilt_pos)
{
    if (mount == NULL)
        return PAN_TILT_MOUNT_STATUS_ERROR;
    
    *pan_pos = mount->pan.angle_cdeg;
    *tilt_pos = mount->tilt.angle_cdeg;

    return PAN_TILT_MOUNT_STATUS_OK;
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
