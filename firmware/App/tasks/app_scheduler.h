#ifndef APP_SCHEDULER_H
#define APP_SCHEDULER_H

#include "telemetry.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The scheduler boundary exists so application startup has one RTOS entry
 * point while task priorities, stack sizing, and scheduling policy stay owned
 * by the embedded task layer.
 */
bool app_scheduler_start(telemetry_write_fn_t telemetry_writer);

#ifdef __cplusplus
}
#endif

#endif
