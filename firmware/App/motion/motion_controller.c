#include "motion_controller.h"

#include <stddef.h>

static motion_status_t motion_status;
static int32_t motion_start_x_milli_mm;
static int32_t motion_start_y_milli_mm;
static uint32_t motion_path_length_milli_mm;
static uint32_t motion_distance_traveled_milli_mm;
static uint32_t motion_remainder_milli_mm_ms;

/* Converts a signed distance into its positive size.
 * This lets the motion math work with movement in either direction.
 */
static uint32_t motion_abs_i32(int32_t value)
{
    if (value < 0) {
        return (uint32_t)(-(value + 1)) + 1u;
    }

    return (uint32_t)value;
}

/* Calculates the integer square root of a 64-bit number.
 * This avoids floating point while still letting us estimate diagonal distance.
 */
static uint32_t motion_isqrt_u64(uint64_t value)
{
    uint64_t result = 0;
    uint64_t bit = 1ull << 62u;

    while (bit > value) {
        bit >>= 2u;
    }

    while (bit != 0u) {
        if (value >= result + bit) {
            value -= result + bit;
            result = (result >> 1u) + bit;
        } else {
            result >>= 1u;
        }

        bit >>= 2u;
    }

    return (uint32_t)result;
}

/* Calculates the straight-line distance between the start and target point.
 * This tells the controller how long the whole move is.
 */
static uint32_t motion_calculate_path_length(
    int32_t start_x_milli_mm,
    int32_t start_y_milli_mm,
    int32_t target_x_milli_mm,
    int32_t target_y_milli_mm
)
{
    uint32_t dx = motion_abs_i32(target_x_milli_mm - start_x_milli_mm);
    uint32_t dy = motion_abs_i32(target_y_milli_mm - start_y_milli_mm);
    uint64_t dx_squared = (uint64_t)dx * (uint64_t)dx;
    uint64_t dy_squared = (uint64_t)dy * (uint64_t)dy;

    return motion_isqrt_u64(dx_squared + dy_squared);
}

/* Finds the current X or Y position along the path.
 * Example: if the move is halfway done, this returns the halfway position.
 */
static int32_t motion_interpolate_position(
    int32_t start_milli_mm,
    int32_t target_milli_mm,
    uint32_t distance_traveled_milli_mm,
    uint32_t path_length_milli_mm
)
{
    int64_t delta = (int64_t)target_milli_mm - (int64_t)start_milli_mm;
    int64_t scaled_delta = delta * (int64_t)distance_traveled_milli_mm;

    if (path_length_milli_mm == 0u) {
        return target_milli_mm;
    }

    return (int32_t)((int64_t)start_milli_mm + (scaled_delta / (int64_t)path_length_milli_mm));
}

/* Completes the active move.
 * The simulated position snaps exactly to the target and the state becomes DONE.
 */
static void motion_finish_move(void)
{
    motion_status.current_x_milli_mm = motion_status.target_x_milli_mm;
    motion_status.current_y_milli_mm = motion_status.target_y_milli_mm;
    motion_status.feedrate_milli_mm_per_s = 0u;
    motion_status.state = MOTION_STATE_DONE;
    motion_path_length_milli_mm = 0u;
    motion_distance_traveled_milli_mm = 0u;
    motion_remainder_milli_mm_ms = 0u;
}

/* Initializes all motion values.
 * The fake stage starts at X=0, Y=0 and is not moving.
 */
void motion_controller_init(void)
{
    motion_status.current_x_milli_mm = 0;
    motion_status.current_y_milli_mm = 0;
    motion_status.target_x_milli_mm = 0;
    motion_status.target_y_milli_mm = 0;
    motion_status.feedrate_milli_mm_per_s = 0u;
    motion_status.state = MOTION_STATE_IDLE;

    motion_start_x_milli_mm = 0;
    motion_start_y_milli_mm = 0;
    motion_path_length_milli_mm = 0u;
    motion_distance_traveled_milli_mm = 0u;
    motion_remainder_milli_mm_ms = 0u;
}

/* Starts a new simulated move.
 * It records the start point, target point, feedrate, and total path length.
 */
bool motion_controller_start_move(
    int32_t target_x_milli_mm,
    int32_t target_y_milli_mm,
    uint32_t feedrate_milli_mm_per_s
)
{
    if (motion_status.state == MOTION_STATE_FAULT || feedrate_milli_mm_per_s == 0u) {
        return false;
    }

    motion_start_x_milli_mm = motion_status.current_x_milli_mm;
    motion_start_y_milli_mm = motion_status.current_y_milli_mm;
    motion_status.target_x_milli_mm = target_x_milli_mm;
    motion_status.target_y_milli_mm = target_y_milli_mm;
    motion_status.feedrate_milli_mm_per_s = feedrate_milli_mm_per_s;
    motion_path_length_milli_mm = motion_calculate_path_length(
        motion_start_x_milli_mm,
        motion_start_y_milli_mm,
        target_x_milli_mm,
        target_y_milli_mm
    );
    motion_distance_traveled_milli_mm = 0u;
    motion_remainder_milli_mm_ms = 0u;

    if (motion_path_length_milli_mm == 0u) {
        motion_finish_move();
        return true;
    }

    motion_status.state = MOTION_STATE_MOVING;
    return true;
}

/* Updates the simulated position based on elapsed time.
 * This is what makes the fake robot appear to move over multiple ticks.
 */
void motion_controller_update(uint32_t delta_ms)
{
    uint64_t distance_numerator;
    uint32_t distance_step_milli_mm;

    if (motion_status.state != MOTION_STATE_MOVING || delta_ms == 0u) {
        return;
    }

    distance_numerator =
        ((uint64_t)motion_status.feedrate_milli_mm_per_s * (uint64_t)delta_ms) +
        (uint64_t)motion_remainder_milli_mm_ms;
    distance_step_milli_mm = (uint32_t)(distance_numerator / 1000u);
    motion_remainder_milli_mm_ms = (uint32_t)(distance_numerator % 1000u);

    if (distance_step_milli_mm == 0u) {
        return;
    }

    if (motion_distance_traveled_milli_mm + distance_step_milli_mm >= motion_path_length_milli_mm) {
        motion_finish_move();
        return;
    }

    motion_distance_traveled_milli_mm += distance_step_milli_mm;
    motion_status.current_x_milli_mm = motion_interpolate_position(
        motion_start_x_milli_mm,
        motion_status.target_x_milli_mm,
        motion_distance_traveled_milli_mm,
        motion_path_length_milli_mm
    );
    motion_status.current_y_milli_mm = motion_interpolate_position(
        motion_start_y_milli_mm,
        motion_status.target_y_milli_mm,
        motion_distance_traveled_milli_mm,
        motion_path_length_milli_mm
    );
}

/* Stops the move at the current position.
 * This is a normal stop, not an emergency fault.
 */
void motion_controller_stop(void)
{
    if (motion_status.state == MOTION_STATE_FAULT) {
        return;
    }

    motion_status.target_x_milli_mm = motion_status.current_x_milli_mm;
    motion_status.target_y_milli_mm = motion_status.current_y_milli_mm;
    motion_status.feedrate_milli_mm_per_s = 0u;
    motion_status.state = MOTION_STATE_STOPPED;
    motion_path_length_milli_mm = 0u;
    motion_distance_traveled_milli_mm = 0u;
    motion_remainder_milli_mm_ms = 0u;
}

/* Immediately enters fault mode.
 * The current position becomes the target so the simulated stage no longer moves.
 */
void motion_controller_set_fault(void)
{
    motion_status.target_x_milli_mm = motion_status.current_x_milli_mm;
    motion_status.target_y_milli_mm = motion_status.current_y_milli_mm;
    motion_status.feedrate_milli_mm_per_s = 0u;
    motion_status.state = MOTION_STATE_FAULT;
    motion_path_length_milli_mm = 0u;
    motion_distance_traveled_milli_mm = 0u;
    motion_remainder_milli_mm_ms = 0u;
}

/* Clears fault mode.
 * This does not move the stage; it only allows future movement again.
 */
void motion_controller_clear_fault(void)
{
    if (motion_status.state == MOTION_STATE_FAULT) {
        motion_status.state = MOTION_STATE_IDLE;
    }
}

/* Gives another module a copy of the current motion status.
 * Returning a copy prevents outside code from directly changing motion internals.
 */
void motion_controller_get_status(motion_status_t *status)
{
    if (status == NULL) {
        return;
    }

    *status = motion_status;
}

/* Converts the internal state number into readable text.
 * Telemetry uses this so people see "MOVING" instead of a raw integer.
 */
const char *motion_controller_state_to_string(motion_state_t state)
{
    switch (state) {
    case MOTION_STATE_IDLE:
        return "IDLE";
    case MOTION_STATE_MOVING:
        return "MOVING";
    case MOTION_STATE_STOPPED:
        return "STOPPED";
    case MOTION_STATE_DONE:
        return "DONE";
    case MOTION_STATE_FAULT:
        return "FAULT";
    default:
        return "UNKNOWN";
    }
}

/* REQ-007: Bounds are checked separately from starting motion so callers can
 * decide whether an out-of-range target should become a latched fault.
 */
bool motion_controller_is_target_in_bounds(
    int32_t target_x_milli_mm,
    int32_t target_y_milli_mm
)
{
    return
        target_x_milli_mm >= MOTION_MIN_X_MILLI_MM &&
        target_x_milli_mm <= MOTION_MAX_X_MILLI_MM &&
        target_y_milli_mm >= MOTION_MIN_Y_MILLI_MM &&
        target_y_milli_mm <= MOTION_MAX_Y_MILLI_MM;
}
