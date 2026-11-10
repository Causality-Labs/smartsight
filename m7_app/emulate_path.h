#ifndef EMULATE_PATH_H
#define EMULATE_PATH_H

#include "pan_tilt_mount.h"

void sine_trace_generator(pan_tilt_position_t *tgt, float t);
void random_walk_generator(pan_tilt_position_t *tgt);

#endif /* EMULATE_PATH_H */