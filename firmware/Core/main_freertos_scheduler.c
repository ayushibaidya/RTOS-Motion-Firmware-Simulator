#include "app_scheduler.h"

#include <stddef.h>
#include <stdio.h>

/* The scheduler demo still writes to stdout because this phase proves RTOS
 * task startup before a UART or board-specific console driver exists.
 */
static void main_scheduler_telemetry_writer(const char *message)
{
    if (message == NULL) {
        return;
    }

    printf("%s", message);
}

int main(void)
{
    return app_scheduler_start(main_scheduler_telemetry_writer) ? 0 : 1;
}
