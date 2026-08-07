#include "motion_controller.h"

#include <stdbool.h>
#include <stdio.h>

static int require_true(bool condition, const char *message, int line)
{
    if (!condition) {
        fprintf(stderr, "FAIL line %d: %s\n", line, message);
        return 1;
    }

    return 0;
}

#define REQUIRE_TRUE(condition, message) \
    do { \
        int result = require_true((condition), (message), __LINE__); \
        if (result != 0) { \
            return result; \
        } \
    } while (0)

static int test_init_sets_idle_origin(void)
{
    motion_status_t status;

    motion_controller_init();
    motion_controller_get_status(&status);

    REQUIRE_TRUE(status.current_x_milli_mm == 0, "initial X position should be zero");
    REQUIRE_TRUE(status.current_y_milli_mm == 0, "initial Y position should be zero");
    REQUIRE_TRUE(status.target_x_milli_mm == 0, "initial target X should be zero");
    REQUIRE_TRUE(status.target_y_milli_mm == 0, "initial target Y should be zero");
    REQUIRE_TRUE(status.feedrate_milli_mm_per_s == 0u, "initial feedrate should be zero");
    REQUIRE_TRUE(status.state == MOTION_STATE_IDLE, "initial state should be IDLE");
    return 0;
}

static int test_move_updates_position_over_time(void)
{
    motion_status_t status;
    bool accepted;

    motion_controller_init();
    accepted = motion_controller_start_move(3000, 4000, 1000u);
    REQUIRE_TRUE(accepted, "valid move should be accepted");

    motion_controller_update(1000u);
    motion_controller_get_status(&status);

    REQUIRE_TRUE(status.state == MOTION_STATE_MOVING, "state should remain MOVING before target");
    REQUIRE_TRUE(status.current_x_milli_mm == 600, "X should move along diagonal path");
    REQUIRE_TRUE(status.current_y_milli_mm == 800, "Y should move along diagonal path");

    motion_controller_update(4000u);
    motion_controller_get_status(&status);

    REQUIRE_TRUE(status.state == MOTION_STATE_DONE, "state should become DONE at target");
    REQUIRE_TRUE(status.current_x_milli_mm == 3000, "final X should equal target X");
    REQUIRE_TRUE(status.current_y_milli_mm == 4000, "final Y should equal target Y");
    REQUIRE_TRUE(status.feedrate_milli_mm_per_s == 0u, "feedrate should reset after move completes");
    return 0;
}

static int test_stop_holds_current_position(void)
{
    motion_status_t before_stop;
    motion_status_t after_stop;

    motion_controller_init();
    REQUIRE_TRUE(motion_controller_start_move(10000, 0, 1000u), "valid move should start");

    motion_controller_update(1000u);
    motion_controller_get_status(&before_stop);
    motion_controller_stop();
    motion_controller_update(5000u);
    motion_controller_get_status(&after_stop);

    REQUIRE_TRUE(after_stop.state == MOTION_STATE_STOPPED, "STOP should set STOPPED state");
    REQUIRE_TRUE(
        after_stop.current_x_milli_mm == before_stop.current_x_milli_mm,
        "position should not change after STOP"
    );
    REQUIRE_TRUE(after_stop.current_y_milli_mm == before_stop.current_y_milli_mm, "Y should remain stopped");
    return 0;
}

static int test_fault_blocks_new_moves_until_cleared(void)
{
    motion_status_t status;

    motion_controller_init();
    REQUIRE_TRUE(motion_controller_start_move(10000, 0, 1000u), "valid move should start");

    motion_controller_update(1000u);
    motion_controller_set_fault();
    motion_controller_get_status(&status);

    REQUIRE_TRUE(status.state == MOTION_STATE_FAULT, "fault should force FAULT state");
    REQUIRE_TRUE(!motion_controller_start_move(0, 0, 1000u), "new moves should be rejected during fault");

    motion_controller_clear_fault();
    motion_controller_get_status(&status);

    REQUIRE_TRUE(status.state == MOTION_STATE_IDLE, "clearing fault should return to IDLE");
    REQUIRE_TRUE(motion_controller_start_move(0, 0, 1000u), "new moves should be accepted after clear");
    return 0;
}

static int test_invalid_feedrate_is_rejected(void)
{
    motion_controller_init();

    REQUIRE_TRUE(!motion_controller_start_move(1000, 1000, 0u), "zero feedrate should be rejected");
    return 0;
}

static int test_target_bounds_checking(void)
{
    /* REQ-007: The stage travel envelope is tested at its edges so future
     * refactors cannot accidentally reject valid boundary points or allow
     * unsafe targets outside the simulated workspace.
     */
    REQUIRE_TRUE(motion_controller_is_target_in_bounds(0, 0), "minimum bounds should be valid");
    REQUIRE_TRUE(
        motion_controller_is_target_in_bounds(MOTION_MAX_X_MILLI_MM, MOTION_MAX_Y_MILLI_MM),
        "maximum bounds should be valid"
    );
    REQUIRE_TRUE(!motion_controller_is_target_in_bounds(-1, 0), "negative X should be invalid");
    REQUIRE_TRUE(!motion_controller_is_target_in_bounds(0, -1), "negative Y should be invalid");
    REQUIRE_TRUE(
        !motion_controller_is_target_in_bounds(MOTION_MAX_X_MILLI_MM + 1, 0),
        "X above maximum should be invalid"
    );
    REQUIRE_TRUE(
        !motion_controller_is_target_in_bounds(0, MOTION_MAX_Y_MILLI_MM + 1),
        "Y above maximum should be invalid"
    );
    return 0;
}

static int test_state_to_string(void)
{
    REQUIRE_TRUE(
        motion_controller_state_to_string(MOTION_STATE_IDLE)[0] == 'I',
        "IDLE state should convert to readable text"
    );
    REQUIRE_TRUE(
        motion_controller_state_to_string(MOTION_STATE_MOVING)[0] == 'M',
        "MOVING state should convert to readable text"
    );
    REQUIRE_TRUE(
        motion_controller_state_to_string((motion_state_t)99)[0] == 'U',
        "unknown state should convert to UNKNOWN"
    );
    return 0;
}

int main(void)
{
    int failures = 0;

    failures += test_init_sets_idle_origin();
    failures += test_move_updates_position_over_time();
    failures += test_stop_holds_current_position();
    failures += test_fault_blocks_new_moves_until_cleared();
    failures += test_invalid_feedrate_is_rejected();
    failures += test_target_bounds_checking();
    failures += test_state_to_string();

    if (failures == 0) {
        printf("motion controller tests passed\n");
    }

    return failures;
}
