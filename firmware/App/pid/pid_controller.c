#include "pid_controller.h"

#include <stddef.h>

static bool pid_range_is_valid(double minimum, double maximum)
{
    return minimum <= maximum;
}

static double pid_clamp(double value, double minimum, double maximum)
{
    if (value < minimum) {
        return minimum;
    }

    if (value > maximum) {
        return maximum;
    }

    return value;
}

static bool pid_config_is_valid(const pid_config_t *config)
{
    return
        config != NULL &&
        pid_range_is_valid(config->output_min, config->output_max) &&
        pid_range_is_valid(config->integral_min, config->integral_max);
}

bool pid_controller_init(pid_controller_t *controller, const pid_config_t *config)
{
    if (controller == NULL || !pid_config_is_valid(config)) {
        return false;
    }

    controller->config = *config;
    controller->previous_error = 0.0;
    controller->integral = 0.0;
    controller->initialized = true;
    return true;
}

bool pid_controller_reset(pid_controller_t *controller)
{
    if (controller == NULL || !controller->initialized) {
        return false;
    }

    controller->previous_error = 0.0;
    controller->integral = 0.0;
    return true;
}

bool pid_controller_update(
    pid_controller_t *controller,
    double setpoint,
    double measurement,
    double dt_seconds,
    double *output
)
{
    double error;
    double derivative;
    double proportional_term;
    double integral_term;
    double derivative_term;
    double unclamped_output;

    if (
        controller == NULL ||
        output == NULL ||
        !controller->initialized ||
        dt_seconds <= 0.0
    ) {
        return false;
    }

    error = setpoint - measurement;

    controller->integral += error * dt_seconds;
    controller->integral = pid_clamp(
        controller->integral,
        controller->config.integral_min,
        controller->config.integral_max
    );

    derivative = (error - controller->previous_error) / dt_seconds;

    proportional_term = controller->config.kp * error;
    integral_term = controller->config.ki * controller->integral;
    derivative_term = controller->config.kd * derivative;
    unclamped_output = proportional_term + integral_term + derivative_term;

    *output = pid_clamp(
        unclamped_output,
        controller->config.output_min,
        controller->config.output_max
    );

    controller->previous_error = error;
    return true;
}
