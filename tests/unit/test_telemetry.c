#include "telemetry.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define OUTPUT_BUFFER_SIZE 512

static char output_buffer[OUTPUT_BUFFER_SIZE];
static size_t output_length;

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
        int result = require_true((condition), (message), __LINE__); \
        if (result != 0) { \
            return result; \
        } \
    } while (0)

static void reset_output(void)
{
    output_buffer[0] = '\0';
    output_length = 0u;
}

static void capture_writer(const char *message)
{
    size_t message_length;
    size_t remaining;

    if (message == NULL) {
        return;
    }

    message_length = strlen(message);
    remaining = OUTPUT_BUFFER_SIZE - output_length - 1u;

    if (message_length > remaining) {
        message_length = remaining;
    }

    memcpy(&output_buffer[output_length], message, message_length);
    output_length += message_length;
    output_buffer[output_length] = '\0';
}

static int test_send_line_adds_newline(void)
{
    telemetry_init();
    telemetry_set_writer(capture_writer);
    reset_output();

    telemetry_send_line("OK TEST");

    REQUIRE_TRUE(strcmp(output_buffer, "OK TEST\n") == 0, "telemetry_send_line should append newline");
    return 0;
}

static int test_startup_message(void)
{
    telemetry_init();
    telemetry_set_writer(capture_writer);
    reset_output();

    telemetry_send_startup();

    REQUIRE_TRUE(
        strcmp(output_buffer, "OK BOOT RTOS_MOTION_FW_SIM\n") == 0,
        "startup message should identify firmware boot"
    );
    return 0;
}

static int test_status_formatting(void)
{
    telemetry_status_t status = {
        .uptime_ms = 123u,
        .state = "MOVING",
        .x_milli_mm = 10250,
        .y_milli_mm = -4500,
        .fault_active = true,
    };

    telemetry_init();
    telemetry_set_writer(capture_writer);
    reset_output();

    telemetry_send_status(&status);

    REQUIRE_TRUE(
        strcmp(output_buffer, "T ms=123 state=MOVING x=10.250 y=-4.500 fault=1\n") == 0,
        "status telemetry should format time, state, position, and fault"
    );
    return 0;
}

static int test_null_status_is_ignored(void)
{
    telemetry_init();
    telemetry_set_writer(capture_writer);
    reset_output();

    telemetry_send_status(NULL);

    REQUIRE_TRUE(strcmp(output_buffer, "") == 0, "NULL status should not write telemetry");
    return 0;
}

int main(void)
{
    int failures = 0;

    failures += test_send_line_adds_newline();
    failures += test_startup_message();
    failures += test_status_formatting();
    failures += test_null_status_is_ignored();

    if (failures == 0) {
        printf("telemetry tests passed\n");
    }

    return failures;
}
