#include "telemetry.h"

#include <stddef.h>
#include <stdio.h>

#define TELEMETRY_LINE_BUFFER_SIZE 128
#define TELEMETRY_POSITION_BUFFER_SIZE 24

/* This stores the current output function.
 * If it is NULL, telemetry has nowhere to send messages yet.
 */
static telemetry_write_fn_t telemetry_writer = NULL;

/* Converts a signed number into its positive size.
 * This is used before formatting negative positions like -12.500 mm.
 */
static uint32_t telemetry_abs_i32(int32_t value)
{
    if (value < 0) {
        return (uint32_t)(-(value + 1)) + 1u;
    }

    return (uint32_t)value;
}

/* Converts a position from milli-millimeters into text.
 * Example: 50250 becomes "50.250".
 */
static void telemetry_format_milli_mm(int32_t milli_mm, char *buffer, size_t buffer_size)
{
    uint32_t magnitude = telemetry_abs_i32(milli_mm);
    const char *sign = milli_mm < 0 ? "-" : "";

    snprintf(
        buffer,
        buffer_size,
        "%s%lu.%03lu",
        sign,
        (unsigned long)(magnitude / 1000u),
        (unsigned long)(magnitude % 1000u)
    );
}

/* Resets the telemetry output path.
 * The writer must be set again before messages can actually be printed.
 */
void telemetry_init(void)
{
    telemetry_writer = NULL;
}

/* Connects telemetry to an output function.
 * The rest of the code does not need to know whether that output is UART, QEMU, or a test.
 */
void telemetry_set_writer(telemetry_write_fn_t writer)
{
    telemetry_writer = writer;
}

/* Sends one complete text line.
 * It also adds a newline so each message appears on its own line.
 */
void telemetry_send_line(const char *line)
{
    if (telemetry_writer == NULL || line == NULL) {
        return;
    }

    telemetry_writer(line);
    telemetry_writer("\n");
}

/* Sends the boot message.
 * The host can use this to confirm that firmware startup succeeded.
 */
void telemetry_send_startup(void)
{
    telemetry_send_line("OK BOOT RTOS_MOTION_FW_SIM");
}

/* Builds and sends the main telemetry message.
 * This reports time, state, position, and whether a fault is active.
 */
void telemetry_send_status(const telemetry_status_t *status)
{
    char line[TELEMETRY_LINE_BUFFER_SIZE];
    char x_text[TELEMETRY_POSITION_BUFFER_SIZE];
    char y_text[TELEMETRY_POSITION_BUFFER_SIZE];

    if (status == NULL) {
        return;
    }

    telemetry_format_milli_mm(status->x_milli_mm, x_text, sizeof(x_text));
    telemetry_format_milli_mm(status->y_milli_mm, y_text, sizeof(y_text));

    snprintf(
        line,
        sizeof(line),
        "T ms=%lu state=%s x=%s y=%s fault=%u",
        (unsigned long)status->uptime_ms,
        status->state != NULL ? status->state : "UNKNOWN",
        x_text,
        y_text,
        status->fault_active ? 1u : 0u
    );

    telemetry_send_line(line);
}
