#include "app_tasks.h"
#include "telemetry.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define MAIN_TICK_MS 100u
#define MAIN_SIMULATION_STEPS 20u

static const char *const MAIN_DEMO_SCRIPT[] = {
    "PING",
    "MOVE X=50 Y=20 F=25",
    "ESTOP",
    "CLEAR_FAULT",
    "STATUS"
};

#define MAIN_DEMO_SCRIPT_LENGTH (sizeof(MAIN_DEMO_SCRIPT) / sizeof(MAIN_DEMO_SCRIPT[0]))

/* The MVP uses stdout as the telemetry sink so module integration can be
 * verified before UART/QEMU output is added.
 */
static void main_telemetry_writer(const char *message){
    if(message == NULL){
        return; 
    }
    printf("%s", message); 
}

/* The demo submits one command through the same task-style path that future
 * FreeRTOS task entry points will use, keeping main focused on scripted timing.
 */
static bool main_run_command(app_context_t *context, const char *line, uint32_t delta_ms)
{
    if (app_command_task_step(context, line) != APP_TASK_STATUS_OK) {
        return false;
    }

    if (app_motion_task_step(context, delta_ms) != APP_TASK_STATUS_OK) {
        return false;
    }

    return app_telemetry_task_step(context) == APP_TASK_STATUS_OK;
}

/* Scheduler ticks are represented explicitly in the host demo so motion timing
 * stays visible until a real FreeRTOS tick source owns this cadence.
 */
static bool main_run_motion_tick(app_context_t *context)
{
    app_task_status_t status = app_motion_task_step(context, MAIN_TICK_MS);

    return
        status == APP_TASK_STATUS_OK ||
        status == APP_TASK_STATUS_NO_MESSAGE;
}

/* The finite demo loop now drives task-step functions instead of duplicating
 * command, motion, fault, and telemetry orchestration in main.
 */
int main(void){
    app_context_t context;
    uint32_t step;

    if (!app_tasks_init(&context, main_telemetry_writer)) {
        return 1;
    }

    telemetry_send_startup();

    for (step = 0u; step < MAIN_DEMO_SCRIPT_LENGTH; step++) {
        if (!main_run_command(&context, MAIN_DEMO_SCRIPT[step], 0u)) {
            return 1;
        }

        if (step == 1u) {
            uint32_t simulation_step;

            for (simulation_step = 0u; simulation_step < MAIN_SIMULATION_STEPS; simulation_step++) {
                if (!main_run_motion_tick(&context)) {
                    return 1;
                }

                if (app_telemetry_task_step(&context) != APP_TASK_STATUS_OK) {
                    return 1;
                }
            }
        }

    }

    return 0;
}
