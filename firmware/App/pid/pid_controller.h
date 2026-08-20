#ifndef PID_CONTROLLER_H
#define PID_CONTROLLER_H

#include <stdbool.h>

typedef struct {
    double kp;
    double ki;
    double kd;
    double output_min;
    double output_max;
    double integral_min;
    double integral_max;
} pid_config_t;

typedef struct {
    pid_config_t config;
    double previous_error;
    double integral;
    bool initialized;
} pid_controller_t;

/* PID configuration is explicit so tuning values and clamps are visible to
 * tests before this module is connected to motion control.
 */
bool pid_controller_init(pid_controller_t *controller, const pid_config_t *config);

/* Reset preserves tuning while clearing accumulated control history, which is
 * required when motion/fault recovery starts a fresh control attempt.
 */
bool pid_controller_reset(pid_controller_t *controller);

/* Update is side-effect limited to controller history and output so the module
 * can be unit-tested before it owns any motor or simulated plant behavior.
 */
bool pid_controller_update(
    pid_controller_t *controller,
    double setpoint,
    double measurement,
    double dt_seconds,
    double *output
);

#endif
