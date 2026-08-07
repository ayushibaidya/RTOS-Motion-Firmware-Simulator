#include "app_tasks.h"
#include "app_messages.h"
#include "fault_manager.h"
#include "motion_controller.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define OUTPUT_BUFFER_SIZE 1024

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
        int assertion_result = require_true((condition), (message), __LINE__); \
        if (assertion_result != 0) { \
            return assertion_result; \
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

static bool output_contains(const char *fragment)
{
    return fragment != NULL && strstr(output_buffer, fragment) != NULL;
}

static int init_context(app_context_t *context)
{
    reset_output();
    REQUIRE_TRUE(app_tasks_init(context, capture_writer), "app task init should succeed");
    return 0;
}

static int test_init_starts_with_empty_resources(void)
{
    app_context_t context;

    REQUIRE_TRUE(init_context(&context) == 0, "context init helper should pass");
    REQUIRE_TRUE(app_tasks_command_queue_count(&context) == 0u, "command queue should start empty");
    REQUIRE_TRUE(app_tasks_get_events(&context) == 0u, "event flags should start clear");
    REQUIRE_TRUE(!fault_manager_is_fault_active(), "fault manager should start clear");
    return 0;
}

static int test_command_task_queues_valid_command(void)
{
    app_context_t context;

    REQUIRE_TRUE(init_context(&context) == 0, "context init helper should pass");
    REQUIRE_TRUE(
        app_command_task_step(&context, "MOVE X=10 Y=20 F=5") == APP_TASK_STATUS_OK,
        "valid command should enqueue"
    );
    REQUIRE_TRUE(app_tasks_command_queue_count(&context) == 1u, "queued command count should increase");
    return 0;
}

static int test_command_task_reports_parse_error(void)
{
    app_context_t context;

    REQUIRE_TRUE(init_context(&context) == 0, "context init helper should pass");
    REQUIRE_TRUE(
        app_command_task_step(&context, "MOVE X=10 Y=20") == APP_TASK_STATUS_ERROR_PARSE,
        "invalid command should return parse error"
    );
    REQUIRE_TRUE(app_tasks_command_queue_count(&context) == 0u, "invalid command should not enqueue");
    REQUIRE_TRUE(output_contains("ERR_MISSING_ARGUMENT"), "parse error should be reported through telemetry");
    return 0;
}

static int test_motion_task_processes_move_and_updates_position(void)
{
    app_context_t context;
    motion_status_t status;

    REQUIRE_TRUE(init_context(&context) == 0, "context init helper should pass");
    REQUIRE_TRUE(app_command_task_step(&context, "MOVE X=10 Y=0 F=5") == APP_TASK_STATUS_OK, "MOVE should enqueue");
    REQUIRE_TRUE(app_motion_task_step(&context, 1000u) == APP_TASK_STATUS_OK, "motion task should process MOVE");

    motion_controller_get_status(&status);

    REQUIRE_TRUE(output_contains("OK MOVE QUEUED"), "motion task should report queued move");
    REQUIRE_TRUE(status.state == MOTION_STATE_MOVING, "one second should leave slow move in MOVING state");
    REQUIRE_TRUE(status.current_x_milli_mm == 5000, "position should advance by feedrate over elapsed time");
    REQUIRE_TRUE((app_tasks_get_events(&context) & APP_EVENT_MOVING) != 0u, "MOVING event bit should be set");
    return 0;
}

static int test_stop_command_stops_without_faulting(void)
{
    app_context_t context;
    motion_status_t status;

    REQUIRE_TRUE(init_context(&context) == 0, "context init helper should pass");
    REQUIRE_TRUE(app_command_task_step(&context, "MOVE X=10 Y=0 F=5") == APP_TASK_STATUS_OK, "MOVE should enqueue");
    REQUIRE_TRUE(app_motion_task_step(&context, 1000u) == APP_TASK_STATUS_OK, "motion task should process MOVE");
    REQUIRE_TRUE(app_command_task_step(&context, "STOP") == APP_TASK_STATUS_OK, "STOP should enqueue");
    REQUIRE_TRUE(app_motion_task_step(&context, 0u) == APP_TASK_STATUS_OK, "motion task should process STOP");

    motion_controller_get_status(&status);

    REQUIRE_TRUE(status.state == MOTION_STATE_STOPPED, "STOP should place motion controller in STOPPED state");
    REQUIRE_TRUE(!fault_manager_is_fault_active(), "STOP should not create a fault");
    REQUIRE_TRUE((app_tasks_get_events(&context) & APP_EVENT_STOP_REQUESTED) != 0u, "STOP_REQUESTED bit should be set");
    REQUIRE_TRUE((app_tasks_get_events(&context) & APP_EVENT_FAULTED) == 0u, "FAULTED bit should stay clear");
    return 0;
}

static int test_estop_and_clear_fault_flow(void)
{
    app_context_t context;
    motion_status_t status;

    REQUIRE_TRUE(init_context(&context) == 0, "context init helper should pass");
    REQUIRE_TRUE(app_command_task_step(&context, "ESTOP") == APP_TASK_STATUS_OK, "ESTOP should enqueue");
    REQUIRE_TRUE(app_motion_task_step(&context, 0u) == APP_TASK_STATUS_OK, "motion task should process ESTOP");

    motion_controller_get_status(&status);

    REQUIRE_TRUE(status.state == MOTION_STATE_FAULT, "ESTOP should force FAULT state");
    REQUIRE_TRUE(fault_manager_is_fault_active(), "ESTOP should latch fault manager");
    REQUIRE_TRUE((app_tasks_get_events(&context) & APP_EVENT_FAULTED) != 0u, "FAULTED bit should be set");
    REQUIRE_TRUE(output_contains("OK ESTOP"), "ESTOP response should be emitted");

    REQUIRE_TRUE(app_command_task_step(&context, "MOVE X=1 Y=0 F=1") == APP_TASK_STATUS_OK, "MOVE should enqueue");
    REQUIRE_TRUE(app_motion_task_step(&context, 0u) == APP_TASK_STATUS_OK, "faulted MOVE should be processed");
    REQUIRE_TRUE(output_contains("ERR FAULT_ACTIVE"), "MOVE should be blocked while faulted");

    REQUIRE_TRUE(app_command_task_step(&context, "CLEAR_FAULT") == APP_TASK_STATUS_OK, "CLEAR_FAULT should enqueue");
    REQUIRE_TRUE(app_motion_task_step(&context, 0u) == APP_TASK_STATUS_OK, "motion task should process CLEAR_FAULT");

    motion_controller_get_status(&status);

    REQUIRE_TRUE(status.state == MOTION_STATE_IDLE, "CLEAR_FAULT should return motion state to IDLE");
    REQUIRE_TRUE(!fault_manager_is_fault_active(), "CLEAR_FAULT should clear fault manager");
    REQUIRE_TRUE((app_tasks_get_events(&context) & APP_EVENT_FAULTED) == 0u, "FAULTED bit should be cleared");
    return 0;
}

static int test_out_of_bounds_move_faults_system(void)
{
    app_context_t context;
    motion_status_t status;

    REQUIRE_TRUE(init_context(&context) == 0, "context init helper should pass");
    REQUIRE_TRUE(app_command_task_step(&context, "MOVE X=101 Y=0 F=5") == APP_TASK_STATUS_OK, "bad MOVE should enqueue");
    REQUIRE_TRUE(app_motion_task_step(&context, 0u) == APP_TASK_STATUS_OK, "motion task should process bad MOVE");

    motion_controller_get_status(&status);

    REQUIRE_TRUE(status.state == MOTION_STATE_FAULT, "out-of-bounds MOVE should fault motion state");
    REQUIRE_TRUE(fault_manager_get_reason() == FAULT_REASON_LIMIT_EXCEEDED, "fault reason should be LIMIT_EXCEEDED");
    REQUIRE_TRUE(output_contains("ERR LIMIT_EXCEEDED"), "limit fault should be reported");
    return 0;
}

static int test_telemetry_task_reports_snapshot(void)
{
    app_context_t context;

    REQUIRE_TRUE(init_context(&context) == 0, "context init helper should pass");
    REQUIRE_TRUE(app_command_task_step(&context, "MOVE X=10 Y=0 F=5") == APP_TASK_STATUS_OK, "MOVE should enqueue");
    REQUIRE_TRUE(app_motion_task_step(&context, 1000u) == APP_TASK_STATUS_OK, "motion task should process MOVE");
    reset_output();
    REQUIRE_TRUE(app_telemetry_task_step(&context) == APP_TASK_STATUS_OK, "telemetry task should emit status");

    REQUIRE_TRUE(output_contains("T ms=1000"), "telemetry should include task uptime");
    REQUIRE_TRUE(output_contains("state=MOVING"), "telemetry should include motion state");
    REQUIRE_TRUE(output_contains("x=5.000"), "telemetry should include current X position");
    REQUIRE_TRUE(output_contains("fault=0"), "telemetry should include fault flag");
    return 0;
}

int main(void)
{
    int failures = 0;

    failures += test_init_starts_with_empty_resources();
    failures += test_command_task_queues_valid_command();
    failures += test_command_task_reports_parse_error();
    failures += test_motion_task_processes_move_and_updates_position();
    failures += test_stop_command_stops_without_faulting();
    failures += test_estop_and_clear_fault_flow();
    failures += test_out_of_bounds_move_faults_system();
    failures += test_telemetry_task_reports_snapshot();

    if (failures == 0) {
        printf("app task tests passed\n");
    }

    return failures;
}
