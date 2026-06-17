#ifndef MOTION_CONTROLLER_H
#define MOTION_CONTROLLER_H

#include <stdbool.h>
#include <stdint.h>

/* The high-level condition of the simulated motion system. */
typedef enum {
    MOTION_STATE_IDLE = 0,
    MOTION_STATE_MOVING,
    MOTION_STATE_STOPPED,
    MOTION_STATE_DONE,
    MOTION_STATE_FAULT
} motion_state_t;

/* One snapshot of the simulated stage.
 * Positions are stored in milli-millimeters, so 1 mm is stored as 1000.
 */
typedef struct {
    int32_t current_x_milli_mm;
    int32_t current_y_milli_mm;
    int32_t target_x_milli_mm;
    int32_t target_y_milli_mm;
    uint32_t feedrate_milli_mm_per_s;
    motion_state_t state;
} motion_status_t;

/* Resets the simulated stage to position 0,0 and state IDLE. */
void motion_controller_init(void);

/* Starts a new move toward the requested X/Y target at the requested speed. */
bool motion_controller_start_move(
    int32_t target_x_milli_mm,
    int32_t target_y_milli_mm,
    uint32_t feedrate_milli_mm_per_s
);

/* Advances the simulated movement by the amount of time that has passed. */
void motion_controller_update(uint32_t delta_ms);

/* Stops movement at the current simulated position without entering fault state. */
void motion_controller_stop(void);

/* Forces the motion system into FAULT and prevents normal movement. */
void motion_controller_set_fault(void);

/* Clears the FAULT state and returns the controller to IDLE. */
void motion_controller_clear_fault(void);

/* Copies the current motion state into the caller's status variable. */
void motion_controller_get_status(motion_status_t *status);

/* Converts a motion state value into readable text for telemetry. */
const char *motion_controller_state_to_string(motion_state_t state);

#endif
