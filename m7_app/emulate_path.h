#ifndef EMULATE_PATH_H
#define EMULATE_PATH_H

#include <stdint.h>
#include <math.h>
#include "pan_tilt_mount.h"

#define PAN_MIN_DEG   (0.0f)
#define PAN_MAX_DEG   (180.0f)
#define TILT_MIN_DEG   (150.0f)
#define TILT_MAX_DEG   (180.0f)

#define PAN_HOME_DEG  (90.0f)
#define TILT_HOME_DEG  (165.0f)

#define PAN_STEP_DEG  (1.5f)
#define TILT_STEP_DEG  (0.5f)

uint32_t rng_state = 0x12345678u;

uint32_t rng_next(void)
{
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

/* uniform float in [-1, 1] */
float rng_unit(void)
{
    return ((float)(rng_next() & 0xFFFFu) / 32767.5f) - 1.0f;
}

void sine_trace_generator(pan_tilt_position_t *tgt, float t)
{
    tgt->pan_position   = 90.0f + 90.0f * sinf(2.0f * 3.14f * 0.1f * t);
    tgt->tilt_position = 165 + 15 * sinf(2.0f * 3.14f * 0.3f * t);
}

float reflect(float value, float min, float max)
{
    if (value < min)
        return min + (min - value);
    if (value > max)
        return max - (value - max);
    return value;
}

void random_walk_generator(pan_tilt_position_t *tgt)
{
    static pan_tilt_position_t pos = {PAN_HOME_DEG, TILT_HOME_DEG };

    pos.pan_position += PAN_STEP_DEG * rng_unit();
    pos.tilt_position += TILT_STEP_DEG * rng_unit();

    pos.pan_position   = reflect(pos.pan_position, PAN_MIN_DEG, PAN_MAX_DEG);
    pos.tilt_position = reflect(pos.tilt_position, TILT_MIN_DEG, TILT_MAX_DEG);

    *tgt = pos;
}


#endif /* EMULATE_PATH_H */