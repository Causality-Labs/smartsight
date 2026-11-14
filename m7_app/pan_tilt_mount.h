#ifndef PAN_TILT_MOUNT_H
#define PAN_TILT_MOUNT_H

#include <stdbool.h>
#include "servo.h"

typedef enum
{
    PAN_TILT_MOUNT_STATUS_OK = 0,
    PAN_TILT_MOUNT_STATUS_ERROR
} pan_tilt_mount_status_t;

typedef enum
{
    PAN_TILT_MOVING = 0,
    PAN_TILT_MOVING_COMPLETE,
    PAN_TILT_MOVING_ERROR
} pan_tilt_move_status_t;

typedef struct
{
    servo_config_t pan;
    servo_config_t tilt;
    int32_t pan_home_cdeg;
    int32_t tilt_home_cdeg;
    int32_t max_pan_step_cdeg;
    int32_t max_tilt_step_cdeg;
} pan_tilt_mount_config_t;

typedef struct
{
    servo_t pan;
    servo_t tilt;
    int32_t pan_home_cdeg;
    int32_t tilt_home_cdeg;
    int32_t max_pan_step_cdeg;
    int32_t max_tilt_step_cdeg;
    int32_t pan_target_cdeg;
    int32_t tilt_target_cdeg;
} pan_tilt_mount_t;

typedef struct {
    int32_t pan_position_cdeg;
    int32_t tilt_position_cdeg;
} pan_tilt_position_t;

pan_tilt_mount_status_t pan_tilt_mount_init(pan_tilt_mount_t *mount,
                                            const pan_tilt_mount_config_t *config);
pan_tilt_mount_status_t pan_tilt_mount_target_position(pan_tilt_mount_t *mount,
                                                       int32_t pan_position,
                                                       int32_t tilt_position);
pan_tilt_mount_status_t pan_tilt_mount_pan_target(pan_tilt_mount_t *mount, int32_t pan_cdeg);
pan_tilt_mount_status_t pan_tilt_mount_tilt_target(pan_tilt_mount_t *mount, int32_t tilt_cdeg);
bool pan_tilt_mount_pan_at_target(const pan_tilt_mount_t *mount);
bool pan_tilt_mount_tilt_at_target(const pan_tilt_mount_t *mount);
pan_tilt_move_status_t pan_tilt_mount_move(pan_tilt_mount_t *mount);
pan_tilt_mount_status_t pan_tilt_mount_stop(pan_tilt_mount_t *mount);
pan_tilt_mount_status_t pan_tilt_mount_home(pan_tilt_mount_t *mount);
pan_tilt_mount_status_t pan_tilt_mount_get_curr_pos(pan_tilt_mount_t *mount, int32_t *pan_pos, int32_t *tilt_pos);
pan_tilt_mount_status_t pan_tilt_mount_enable(pan_tilt_mount_t *mount);
pan_tilt_mount_status_t pan_tilt_mount_disable(pan_tilt_mount_t *mount);
pan_tilt_mount_status_t pan_tilt_mount_deinit(pan_tilt_mount_t *mount);


#endif /* PAN_TILT_MOUNT_H */
