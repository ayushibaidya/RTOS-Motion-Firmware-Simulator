#include "app_scheduler.h"

#if defined(USE_FREERTOS) && (USE_FREERTOS == 1)

#include "FreeRTOS.h"
#include "app_tasks.h"
#include "task.h"

#include <stddef.h>

/* Fixed task periods make the first scheduler demo deterministic enough for CI
 * while still modeling periodic firmware work.
 */
#define APP_SCHEDULER_MOTION_PERIOD_MS 100u
#define APP_SCHEDULER_COMMAND_PERIOD_MS 500u
#define APP_SCHEDULER_TELEMETRY_PERIOD_MS 500u
#define APP_SCHEDULER_DEMO_RUNTIME_MS 3500u
#define APP_SCHEDULER_SHUTDOWN_GRACE_MS 100u

/* Motion gets the highest application priority because control-loop timing is
 * more safety-critical than command intake or status reporting.
 */
#define APP_SCHEDULER_COMMAND_TASK_PRIORITY 3u
#define APP_SCHEDULER_MOTION_TASK_PRIORITY 4u
#define APP_SCHEDULER_TELEMETRY_TASK_PRIORITY 2u
#define APP_SCHEDULER_SUPERVISOR_TASK_PRIORITY 1u

/* Static stacks preserve the embedded design constraint that firmware-owned
 * RTOS objects should not depend on heap allocation.
 */
#define APP_SCHEDULER_TASK_STACK_WORDS configMINIMAL_STACK_SIZE

/* This scripted input stands in for UART during the current software-only
 * phase, giving the scheduler path a repeatable smoke test before hardware IO.
 */
static const char *const APP_SCHEDULER_DEMO_SCRIPT[] = {
    "PING",
    "MOVE X=50 Y=20 F=25",
    "ESTOP",
    "CLEAR_FAULT",
    "STATUS"
};

#define APP_SCHEDULER_DEMO_SCRIPT_LENGTH \
    (sizeof(APP_SCHEDULER_DEMO_SCRIPT) / sizeof(APP_SCHEDULER_DEMO_SCRIPT[0]))

static app_context_t app_scheduler_context;
static volatile bool app_scheduler_stop_requested;

/* Static task storage keeps the FreeRTOS demo aligned with the no-dynamic-
 * allocation policy used by the wrapper and configuration layer.
 */
static StaticTask_t app_scheduler_command_tcb;
static StaticTask_t app_scheduler_motion_tcb;
static StaticTask_t app_scheduler_telemetry_tcb;
static StaticTask_t app_scheduler_supervisor_tcb;

static StackType_t app_scheduler_command_stack[APP_SCHEDULER_TASK_STACK_WORDS];
static StackType_t app_scheduler_motion_stack[APP_SCHEDULER_TASK_STACK_WORDS];
static StackType_t app_scheduler_telemetry_stack[APP_SCHEDULER_TASK_STACK_WORDS];
static StackType_t app_scheduler_supervisor_stack[APP_SCHEDULER_TASK_STACK_WORDS];

/* The command task uses scripted input for now so scheduler behavior can be
 * validated before the UART driver exists.
 */
static void app_scheduler_command_task(void *parameters)
{
    app_context_t *context = (app_context_t *)parameters;
    size_t command_index = 0u;
    TickType_t last_wake_time = xTaskGetTickCount();

    while (!app_scheduler_stop_requested) {
        if (command_index < APP_SCHEDULER_DEMO_SCRIPT_LENGTH) {
            (void)app_command_task_step(
                context,
                APP_SCHEDULER_DEMO_SCRIPT[command_index]
            );
            command_index++;
        }

        vTaskDelayUntil(
            &last_wake_time,
            pdMS_TO_TICKS(APP_SCHEDULER_COMMAND_PERIOD_MS)
        );
    }

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(APP_SCHEDULER_SHUTDOWN_GRACE_MS));
    }
}

/* Motion stays on a periodic task because machine-control firmware needs a
 * predictable update cadence for position and fault behavior.
 */
static void app_scheduler_motion_task(void *parameters)
{
    app_context_t *context = (app_context_t *)parameters;
    TickType_t last_wake_time = xTaskGetTickCount();

    while (!app_scheduler_stop_requested) {
        (void)app_motion_task_step(context, APP_SCHEDULER_MOTION_PERIOD_MS);

        vTaskDelayUntil(
            &last_wake_time,
            pdMS_TO_TICKS(APP_SCHEDULER_MOTION_PERIOD_MS)
        );
    }

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(APP_SCHEDULER_SHUTDOWN_GRACE_MS));
    }
}

/* Telemetry is isolated from motion control so slow reporting cannot become
 * part of the safety-critical motion update path.
 */
static void app_scheduler_telemetry_task(void *parameters)
{
    app_context_t *context = (app_context_t *)parameters;
    TickType_t last_wake_time = xTaskGetTickCount();

    while (!app_scheduler_stop_requested) {
        (void)app_telemetry_task_step(context);

        vTaskDelayUntil(
            &last_wake_time,
            pdMS_TO_TICKS(APP_SCHEDULER_TELEMETRY_PERIOD_MS)
        );
    }

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(APP_SCHEDULER_SHUTDOWN_GRACE_MS));
    }
}

/* The POSIX FreeRTOS demo is intentionally finite so CI can verify scheduler
 * startup without hanging like production firmware's forever-loop model.
 */
static void app_scheduler_supervisor_task(void *parameters)
{
    (void)parameters;

    vTaskDelay(pdMS_TO_TICKS(APP_SCHEDULER_DEMO_RUNTIME_MS));
    app_scheduler_stop_requested = true;
    vTaskDelay(pdMS_TO_TICKS(APP_SCHEDULER_SHUTDOWN_GRACE_MS));
    vTaskEndScheduler();

    for (;;) {
    }
}

static bool app_scheduler_create_task(
    TaskFunction_t task_function,
    const char *task_name,
    UBaseType_t priority,
    StackType_t *stack,
    StaticTask_t *control_block
)
{
    /* Centralized task creation prevents stack/priority policy from drifting
     * across task definitions as the scheduler layer grows.
     */
    return xTaskCreateStatic(
        task_function,
        task_name,
        APP_SCHEDULER_TASK_STACK_WORDS,
        &app_scheduler_context,
        priority,
        stack,
        control_block
    ) != NULL;
}

static bool app_scheduler_create_tasks(void)
{
    if (!app_scheduler_create_task(
            app_scheduler_command_task,
            "CmdTask",
            APP_SCHEDULER_COMMAND_TASK_PRIORITY,
            app_scheduler_command_stack,
            &app_scheduler_command_tcb
        )) {
        return false;
    }

    if (!app_scheduler_create_task(
            app_scheduler_motion_task,
            "MotionTask",
            APP_SCHEDULER_MOTION_TASK_PRIORITY,
            app_scheduler_motion_stack,
            &app_scheduler_motion_tcb
        )) {
        return false;
    }

    if (!app_scheduler_create_task(
            app_scheduler_telemetry_task,
            "TelemTask",
            APP_SCHEDULER_TELEMETRY_TASK_PRIORITY,
            app_scheduler_telemetry_stack,
            &app_scheduler_telemetry_tcb
        )) {
        return false;
    }

    if (!app_scheduler_create_task(
            app_scheduler_supervisor_task,
            "SupTask",
            APP_SCHEDULER_SUPERVISOR_TASK_PRIORITY,
            app_scheduler_supervisor_stack,
            &app_scheduler_supervisor_tcb
        )) {
        return false;
    }

    return true;
}

bool app_scheduler_start(telemetry_write_fn_t telemetry_writer)
{
    /* Startup owns context initialization and scheduler launch so higher-level
     * demos do not need to know the internal task topology.
     */
    app_scheduler_stop_requested = false;

    if (!app_tasks_init(&app_scheduler_context, telemetry_writer)) {
        return false;
    }

    telemetry_send_startup();

    if (!app_scheduler_create_tasks()) {
        return false;
    }

    vTaskStartScheduler();
    return true;
}

#else

bool app_scheduler_start(telemetry_write_fn_t telemetry_writer)
{
    /* Host builds use explicit task-step calls, so the stub makes accidental
     * scheduler startup fail clearly unless the real FreeRTOS backend is built.
     */
    (void)telemetry_writer;
    return false;
}

#endif
