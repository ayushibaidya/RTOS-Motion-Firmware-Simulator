#include "app_tasks.h"

#include "command_parser.h"
#include "fault_manager.h"
#include "motion_controller.h"
#include "telemetry.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

static const char APP_RESPONSE_PONG[] = "OK PONG";
static const char APP_RESPONSE_STATUS[] = "OK STATUS";
static const char APP_RESPONSE_MOVE_QUEUED[] = "OK MOVE QUEUED";
static const char APP_RESPONSE_STOPPED[] = "OK STOPPED";
static const char APP_RESPONSE_ESTOP[] = "OK ESTOP";
static const char APP_RESPONSE_FAULT_CLEARED[] = "OK FAULT CLEARED";
static const char APP_ERROR_FAULT_ACTIVE[] = "ERR FAULT_ACTIVE";
static const char APP_ERROR_LIMIT_EXCEEDED[] = "ERR LIMIT_EXCEEDED";
static const char APP_ERROR_MOVE_REJECTED[] = "ERR MOVE_REJECTED";
static const char APP_ERROR_UNKNOWN_COMMAND[] = "ERR UNKNOWN_COMMAND";

static bool app_context_is_valid(const app_context_t *context)
{
    return context != NULL;
}

static bool app_tasks_enqueue_telemetry_line(app_context_t *context, const char *line)
{
    app_telemetry_message_t message;
    size_t line_length;

    /* Responses are queued so command/motion work cannot directly own the
     * output path once real RTOS tasks run concurrently.
     */
    if (!app_context_is_valid(context) || line == NULL) {
        return false;
    }

    line_length = strlen(line);

    if (line_length >= sizeof(message.text)) {
        return false;
    }

    memcpy(message.text, line, line_length + 1u);
    return rtos_queue_send(&context->telemetry_queue, &message);
}

static void app_tasks_drain_telemetry_queue(app_context_t *context)
{
    app_telemetry_message_t message;

    /* TelemetryTask is the only place that writes queued task responses, which
     * keeps reporting decoupled from command parsing and motion updates.
     */
    while (rtos_queue_receive(&context->telemetry_queue, &message)) {
        telemetry_send_line(message.text);
    }
}

static void app_tasks_refresh_motion_events(app_context_t *context)
{
    motion_status_t motion_status;

    if (!app_context_is_valid(context)) {
        return;
    }

    motion_controller_get_status(&motion_status);

    if (motion_status.state == MOTION_STATE_MOVING) {
        rtos_event_group_set_bits(&context->event_group, APP_EVENT_MOVING);
    } else {
        rtos_event_group_clear_bits(&context->event_group, APP_EVENT_MOVING);
    }

    if (fault_manager_is_fault_active()) {
        rtos_event_group_set_bits(&context->event_group, APP_EVENT_FAULTED);
    } else {
        rtos_event_group_clear_bits(&context->event_group, APP_EVENT_FAULTED);
    }
}

static const char *app_tasks_handle_command(app_context_t *context, const command_t *command)
{
    if (!app_context_is_valid(context) || command == NULL) {
        return APP_ERROR_UNKNOWN_COMMAND;
    }

    switch (command->type) {
    case COMMAND_TYPE_PING:
        return APP_RESPONSE_PONG;

    case COMMAND_TYPE_STATUS:
        return APP_RESPONSE_STATUS;

    case COMMAND_TYPE_MOVE:
        if (fault_manager_is_fault_active()) {
            return APP_ERROR_FAULT_ACTIVE;
        }

        if (!motion_controller_is_target_in_bounds(command->x_milli_mm, command->y_milli_mm)) {
            fault_manager_set_fault(FAULT_REASON_LIMIT_EXCEEDED);
            motion_controller_set_fault();
            app_tasks_refresh_motion_events(context);
            return APP_ERROR_LIMIT_EXCEEDED;
        }

        rtos_event_group_clear_bits(&context->event_group, APP_EVENT_STOP_REQUESTED);

        if (motion_controller_start_move(
                command->x_milli_mm,
                command->y_milli_mm,
                command->feedrate_milli_mm_per_s
            )) {
            app_tasks_refresh_motion_events(context);
            return APP_RESPONSE_MOVE_QUEUED;
        }

        return APP_ERROR_MOVE_REJECTED;

    case COMMAND_TYPE_STOP:
        motion_controller_stop();
        rtos_event_group_set_bits(&context->event_group, APP_EVENT_STOP_REQUESTED);
        app_tasks_refresh_motion_events(context);
        return APP_RESPONSE_STOPPED;

    case COMMAND_TYPE_ESTOP:
        fault_manager_trigger_estop();
        motion_controller_set_fault();
        rtos_event_group_clear_bits(&context->event_group, APP_EVENT_MOVING);
        rtos_event_group_set_bits(&context->event_group, APP_EVENT_FAULTED);
        return APP_RESPONSE_ESTOP;

    case COMMAND_TYPE_CLEAR_FAULT:
        fault_manager_clear();
        motion_controller_clear_fault();
        rtos_event_group_clear_bits(
            &context->event_group,
            APP_EVENT_FAULTED | APP_EVENT_MOVING | APP_EVENT_STOP_REQUESTED
        );
        return APP_RESPONSE_FAULT_CLEARED;

    case COMMAND_TYPE_NONE:
    default:
        return APP_ERROR_UNKNOWN_COMMAND;
    }
}

bool app_tasks_init(app_context_t *context, telemetry_write_fn_t telemetry_writer)
{
    if (!app_context_is_valid(context)) {
        return false;
    }

    if (!rtos_queue_init(
            &context->command_queue,
            context->command_queue_storage,
            sizeof(context->command_queue_storage[0]),
            APP_COMMAND_QUEUE_CAPACITY
        )) {
        return false;
    }

    if (!rtos_queue_init(
            &context->telemetry_queue,
            context->telemetry_queue_storage,
            sizeof(context->telemetry_queue_storage[0]),
            APP_TELEMETRY_QUEUE_CAPACITY
        )) {
        return false;
    }

    if (!rtos_mutex_init(&context->state_mutex)) {
        return false;
    }

    if (!rtos_event_group_init(&context->event_group)) {
        return false;
    }

    context->uptime_ms = 0u;

    telemetry_init();
    telemetry_set_writer(telemetry_writer);
    motion_controller_init();
    fault_manager_init();
    return true;
}

app_task_status_t app_command_task_step(app_context_t *context, const char *line)
{
    app_command_message_t message;
    command_parse_result_t parse_result;

    if (!app_context_is_valid(context) || line == NULL) {
        return APP_TASK_STATUS_ERROR_NULL;
    }

    parse_result = command_parser_parse(line, &message.command);

    if (parse_result != COMMAND_PARSE_OK) {
        if (!app_tasks_enqueue_telemetry_line(
                context,
                command_parser_result_to_string(parse_result)
            )) {
            return APP_TASK_STATUS_ERROR_QUEUE_FULL;
        }

        return APP_TASK_STATUS_ERROR_PARSE;
    }

    if (!rtos_queue_send(&context->command_queue, &message)) {
        return APP_TASK_STATUS_ERROR_QUEUE_FULL;
    }

    return APP_TASK_STATUS_OK;
}

app_task_status_t app_motion_task_step(app_context_t *context, uint32_t delta_ms)
{
    app_command_message_t message;
    const char *response = NULL;
    bool has_message;

    if (!app_context_is_valid(context)) {
        return APP_TASK_STATUS_ERROR_NULL;
    }

    if (!rtos_mutex_lock(&context->state_mutex)) {
        return APP_TASK_STATUS_ERROR_MUTEX;
    }

    has_message = rtos_queue_receive(&context->command_queue, &message);

    if (has_message) {
        response = app_tasks_handle_command(context, &message.command);
    }

    motion_controller_update(delta_ms);
    context->uptime_ms += delta_ms;
    app_tasks_refresh_motion_events(context);

    if (!rtos_mutex_unlock(&context->state_mutex)) {
        return APP_TASK_STATUS_ERROR_MUTEX;
    }

    if (response != NULL && !app_tasks_enqueue_telemetry_line(context, response)) {
        return APP_TASK_STATUS_ERROR_QUEUE_FULL;
    }

    if (!has_message) {
        return APP_TASK_STATUS_NO_MESSAGE;
    }

    return APP_TASK_STATUS_OK;
}

app_task_status_t app_telemetry_task_step(app_context_t *context)
{
    motion_status_t motion_status;
    telemetry_status_t telemetry_status;

    if (!app_context_is_valid(context)) {
        return APP_TASK_STATUS_ERROR_NULL;
    }

    if (!rtos_mutex_lock(&context->state_mutex)) {
        return APP_TASK_STATUS_ERROR_MUTEX;
    }

    motion_controller_get_status(&motion_status);

    telemetry_status.uptime_ms = context->uptime_ms;
    telemetry_status.state = motion_controller_state_to_string(motion_status.state);
    telemetry_status.x_milli_mm = motion_status.current_x_milli_mm;
    telemetry_status.y_milli_mm = motion_status.current_y_milli_mm;
    telemetry_status.fault_active = fault_manager_is_fault_active();

    if (!rtos_mutex_unlock(&context->state_mutex)) {
        return APP_TASK_STATUS_ERROR_MUTEX;
    }

    app_tasks_drain_telemetry_queue(context);
    telemetry_send_status(&telemetry_status);
    return APP_TASK_STATUS_OK;
}

size_t app_tasks_command_queue_count(const app_context_t *context)
{
    if (!app_context_is_valid(context)) {
        return 0u;
    }

    return rtos_queue_count(&context->command_queue);
}

size_t app_tasks_telemetry_queue_count(const app_context_t *context)
{
    if (!app_context_is_valid(context)) {
        return 0u;
    }

    return rtos_queue_count(&context->telemetry_queue);
}

rtos_event_bits_t app_tasks_get_events(const app_context_t *context)
{
    if (!app_context_is_valid(context)) {
        return 0u;
    }

    return rtos_event_group_get_bits(&context->event_group);
}
