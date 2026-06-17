#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <stdbool.h>
#include <stdint.h>

/* A writer function is the output path for telemetry text.
 * Later this can point to UART, QEMU console output, or a test logger.
 */
typedef void (*telemetry_write_fn_t)(const char *message);

/* One snapshot of the firmware's current status.
 * Positions are stored in milli-millimeters, so 10.250 mm is stored as 10250.
 */
typedef struct {
    uint32_t uptime_ms;
    const char *state;
    int32_t x_milli_mm;
    int32_t y_milli_mm;
    bool fault_active;
} telemetry_status_t;

/* Resets the telemetry module to a known startup state. */
void telemetry_init(void);

/* Selects where telemetry text should be sent. */
void telemetry_set_writer(telemetry_write_fn_t writer);

/* Sends one line of telemetry or response text. */
void telemetry_send_line(const char *line);

/* Sends a startup message so the host knows the firmware booted. */
void telemetry_send_startup(void);

/* Converts a status snapshot into a readable telemetry message and sends it. */
void telemetry_send_status(const telemetry_status_t *status);

#endif
