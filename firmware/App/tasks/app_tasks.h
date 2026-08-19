#ifndef APP_TASKS_H
#define APP_TASKS_H

#include "app_messages.h"
#include "rtos_port.h"
#include "telemetry.h"

#include <stddef.h>
#include <stdint.h>

typedef enum {
    APP_TASK_STATUS_OK = 0,
    APP_TASK_STATUS_NO_MESSAGE,
    APP_TASK_STATUS_ERROR_NULL,
    APP_TASK_STATUS_ERROR_PARSE,
    APP_TASK_STATUS_ERROR_QUEUE_FULL,
    APP_TASK_STATUS_ERROR_MUTEX
} app_task_status_t;

typedef struct {
    rtos_queue_t command_queue;
    app_command_message_t command_queue_storage[APP_COMMAND_QUEUE_CAPACITY];
    rtos_queue_t telemetry_queue;
    app_telemetry_message_t telemetry_queue_storage[APP_TELEMETRY_QUEUE_CAPACITY];
    rtos_mutex_t state_mutex;
    rtos_event_group_t event_group;
    uint32_t uptime_ms;
} app_context_t;

/* The app context owns task communication resources so host and FreeRTOS builds
 * share the same queue, mutex, and event-flag boundaries.
 */
bool app_tasks_init(app_context_t *context, telemetry_write_fn_t telemetry_writer);

/* CommandTask step: parse one raw command line and enqueue a validated command
 * message for the motion/safety task path.
 */
app_task_status_t app_command_task_step(app_context_t *context, const char *line);

/* MotionTask step: consume at most one queued command and advance simulated
 * motion by the provided scheduler tick duration.
 */
app_task_status_t app_motion_task_step(app_context_t *context, uint32_t delta_ms);

/* TelemetryTask step: drain queued task responses and publish one state
 * snapshot without letting other tasks write directly to the output path.
 */
app_task_status_t app_telemetry_task_step(app_context_t *context);

size_t app_tasks_command_queue_count(const app_context_t *context);
size_t app_tasks_telemetry_queue_count(const app_context_t *context);
rtos_event_bits_t app_tasks_get_events(const app_context_t *context);

#endif
