#include "pid_controller.h"

#include <stdbool.h>
#include <stdio.h>

#define PID_TEST_TOLERANCE 0.000001

static int require_true(bool condition, const char *message, int line)
{
    if (!condition) {
        fprintf(stderr, "FAIL line %d: %s\n", line, message);
        return 1;
    }

    return 0;
}

#define REQUIRE_TRUE(condition, message) \
    do { \
        int assertion_result = require_true((condition), (message), __LINE__); \
        if (assertion_result != 0) { \
            return assertion_result; \
        } \
    } while (0)

static double absolute_double(double value)
{
    return value < 0.0 ? -value : value;
}

static bool values_are_close(double actual, double expected)
{
    return absolute_double(actual - expected) <= PID_TEST_TOLERANCE;
}

static pid_config_t make_config(void)
{
    pid_config_t config = {
        .kp = 0.0,
        .ki = 0.0,
        .kd = 0.0,
        .output_min = -100.0,
        .output_max = 100.0,
        .integral_min = -50.0,
        .integral_max = 50.0
    };

    return config;
}

static int test_init_rejects_invalid_inputs(void)
{
    pid_controller_t controller;
    pid_config_t config = make_config();

    REQUIRE_TRUE(!pid_controller_init(NULL, &config), "NULL controller should be rejected");
    REQUIRE_TRUE(!pid_controller_init(&controller, NULL), "NULL config should be rejected");

    config.output_min = 10.0;
    config.output_max = -10.0;
    REQUIRE_TRUE(!pid_controller_init(&controller, &config), "invalid output clamp should be rejected");

    config = make_config();
    config.integral_min = 5.0;
    config.integral_max = -5.0;
    REQUIRE_TRUE(!pid_controller_init(&controller, &config), "invalid integral clamp should be rejected");

    config = make_config();
    REQUIRE_TRUE(pid_controller_init(&controller, &config), "valid config should initialize controller");
    REQUIRE_TRUE(controller.initialized, "controller should record initialized state");
    return 0;
}

static int test_update_rejects_invalid_runtime_inputs(void)
{
    pid_controller_t controller;
    pid_config_t config = make_config();
    double output = 0.0;

    REQUIRE_TRUE(!pid_controller_update(NULL, 1.0, 0.0, 1.0, &output), "NULL controller should fail update");
    REQUIRE_TRUE(!pid_controller_update(&controller, 1.0, 0.0, 1.0, &output), "uninitialized controller should fail update");

    REQUIRE_TRUE(pid_controller_init(&controller, &config), "valid config should initialize controller");
    REQUIRE_TRUE(!pid_controller_update(&controller, 1.0, 0.0, 0.0, &output), "zero dt should fail update");
    REQUIRE_TRUE(!pid_controller_update(&controller, 1.0, 0.0, -1.0, &output), "negative dt should fail update");
    REQUIRE_TRUE(!pid_controller_update(&controller, 1.0, 0.0, 1.0, NULL), "NULL output should fail update");
    return 0;
}

static int test_zero_error_outputs_zero(void)
{
    pid_controller_t controller;
    pid_config_t config = make_config();
    double output = 1.0;

    config.kp = 2.0;
    config.ki = 1.0;
    config.kd = 1.0;

    REQUIRE_TRUE(pid_controller_init(&controller, &config), "controller should initialize");
    REQUIRE_TRUE(pid_controller_update(&controller, 10.0, 10.0, 0.5, &output), "zero-error update should pass");
    REQUIRE_TRUE(values_are_close(output, 0.0), "zero error should produce zero output");
    return 0;
}

static int test_proportional_output_uses_current_error(void)
{
    pid_controller_t controller;
    pid_config_t config = make_config();
    double output = 0.0;

    config.kp = 2.0;

    REQUIRE_TRUE(pid_controller_init(&controller, &config), "controller should initialize");
    REQUIRE_TRUE(pid_controller_update(&controller, 5.0, 2.0, 1.0, &output), "proportional update should pass");
    REQUIRE_TRUE(values_are_close(output, 6.0), "proportional output should equal kp times error");
    return 0;
}

static int test_integral_output_accumulates_over_time(void)
{
    pid_controller_t controller;
    pid_config_t config = make_config();
    double output = 0.0;

    config.ki = 2.0;

    REQUIRE_TRUE(pid_controller_init(&controller, &config), "controller should initialize");
    REQUIRE_TRUE(pid_controller_update(&controller, 2.0, 0.0, 0.5, &output), "first integral update should pass");
    REQUIRE_TRUE(values_are_close(output, 2.0), "first output should use accumulated integral");

    REQUIRE_TRUE(pid_controller_update(&controller, 2.0, 0.0, 0.5, &output), "second integral update should pass");
    REQUIRE_TRUE(values_are_close(output, 4.0), "second output should include accumulated history");
    return 0;
}

static int test_derivative_output_reacts_to_error_change(void)
{
    pid_controller_t controller;
    pid_config_t config = make_config();
    double output = 0.0;

    config.kd = 0.5;

    REQUIRE_TRUE(pid_controller_init(&controller, &config), "controller should initialize");
    REQUIRE_TRUE(pid_controller_update(&controller, 1.0, 0.0, 1.0, &output), "first derivative update should pass");
    REQUIRE_TRUE(values_are_close(output, 0.5), "first derivative output should use initial error change");

    REQUIRE_TRUE(pid_controller_update(&controller, 3.0, 0.0, 1.0, &output), "second derivative update should pass");
    REQUIRE_TRUE(values_are_close(output, 1.0), "derivative should react to faster error growth");
    return 0;
}

static int test_output_is_clamped(void)
{
    pid_controller_t controller;
    pid_config_t config = make_config();
    double output = 0.0;

    config.kp = 10.0;
    config.output_min = -5.0;
    config.output_max = 5.0;

    REQUIRE_TRUE(pid_controller_init(&controller, &config), "controller should initialize");
    REQUIRE_TRUE(pid_controller_update(&controller, 10.0, 0.0, 1.0, &output), "clamped update should pass");
    REQUIRE_TRUE(values_are_close(output, 5.0), "output should clamp to configured maximum");

    REQUIRE_TRUE(pid_controller_update(&controller, -10.0, 0.0, 1.0, &output), "negative clamped update should pass");
    REQUIRE_TRUE(values_are_close(output, -5.0), "output should clamp to configured minimum");
    return 0;
}

static int test_integral_is_clamped(void)
{
    pid_controller_t controller;
    pid_config_t config = make_config();
    double output = 0.0;

    config.ki = 1.0;
    config.integral_min = -2.0;
    config.integral_max = 2.0;

    REQUIRE_TRUE(pid_controller_init(&controller, &config), "controller should initialize");
    REQUIRE_TRUE(pid_controller_update(&controller, 10.0, 0.0, 1.0, &output), "first integral clamp update should pass");
    REQUIRE_TRUE(values_are_close(output, 2.0), "positive integral should clamp to maximum");

    REQUIRE_TRUE(pid_controller_update(&controller, -10.0, 0.0, 1.0, &output), "negative integral update should pass");
    REQUIRE_TRUE(values_are_close(output, -2.0), "negative integral should clamp to minimum after enough error");
    return 0;
}

static int test_reset_clears_control_history(void)
{
    pid_controller_t controller;
    pid_config_t config = make_config();
    double output = 0.0;

    config.ki = 1.0;
    config.kd = 1.0;

    REQUIRE_TRUE(pid_controller_init(&controller, &config), "controller should initialize");
    REQUIRE_TRUE(pid_controller_update(&controller, 2.0, 0.0, 1.0, &output), "first update should pass");
    REQUIRE_TRUE(!values_are_close(controller.integral, 0.0), "integral should hold history before reset");
    REQUIRE_TRUE(!values_are_close(controller.previous_error, 0.0), "previous error should hold history before reset");

    REQUIRE_TRUE(pid_controller_reset(&controller), "reset should pass after initialization");
    REQUIRE_TRUE(values_are_close(controller.integral, 0.0), "reset should clear integral history");
    REQUIRE_TRUE(values_are_close(controller.previous_error, 0.0), "reset should clear previous error history");
    return 0;
}

int main(void)
{
    int failures = 0;

    failures += test_init_rejects_invalid_inputs();
    failures += test_update_rejects_invalid_runtime_inputs();
    failures += test_zero_error_outputs_zero();
    failures += test_proportional_output_uses_current_error();
    failures += test_integral_output_accumulates_over_time();
    failures += test_derivative_output_reacts_to_error_change();
    failures += test_output_is_clamped();
    failures += test_integral_is_clamped();
    failures += test_reset_clears_control_history();

    if (failures == 0) {
        printf("pid controller tests passed\n");
    }

    return failures;
}
