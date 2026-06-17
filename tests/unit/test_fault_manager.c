#include "fault_manager.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

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
        int assertion_result = require_true((condition), (message), __LINE__); \
        if (assertion_result != 0) { \
            return assertion_result; \
        } \
    } while (0)

static int test_init_starts_without_fault(void)
{
    fault_manager_init();

    REQUIRE_TRUE(!fault_manager_is_fault_active(), "init should start with no active fault");
    REQUIRE_TRUE(fault_manager_get_reason() == FAULT_REASON_NONE, "init should set reason to NONE");
    return 0;
}

static int test_set_fault_latches_reason(void)
{
    fault_manager_init();
    fault_manager_set_fault(FAULT_REASON_LIMIT_EXCEEDED);

    REQUIRE_TRUE(fault_manager_is_fault_active(), "set_fault should activate fault state");
    REQUIRE_TRUE(
        fault_manager_get_reason() == FAULT_REASON_LIMIT_EXCEEDED,
        "set_fault should store requested reason"
    );
    return 0;
}

static int test_none_reason_does_not_activate_fault(void)
{
    fault_manager_init();
    fault_manager_set_fault(FAULT_REASON_NONE);

    REQUIRE_TRUE(!fault_manager_is_fault_active(), "NONE should not activate fault state");
    REQUIRE_TRUE(fault_manager_get_reason() == FAULT_REASON_NONE, "NONE should leave reason unchanged");
    return 0;
}

static int test_estop_helper_sets_estop_fault(void)
{
    fault_manager_init();
    fault_manager_trigger_estop();

    REQUIRE_TRUE(fault_manager_is_fault_active(), "ESTOP should activate fault state");
    REQUIRE_TRUE(fault_manager_get_reason() == FAULT_REASON_ESTOP, "ESTOP should set ESTOP reason");
    return 0;
}

static int test_clear_resets_fault_state(void)
{
    fault_manager_init();
    fault_manager_set_fault(FAULT_REASON_INVALID_COMMAND);
    fault_manager_clear();

    REQUIRE_TRUE(!fault_manager_is_fault_active(), "clear should deactivate fault state");
    REQUIRE_TRUE(fault_manager_get_reason() == FAULT_REASON_NONE, "clear should reset reason to NONE");
    return 0;
}

static int test_reason_to_string(void)
{
    REQUIRE_TRUE(
        strcmp(fault_manager_reason_to_string(FAULT_REASON_NONE), "NONE") == 0,
        "NONE reason string should match"
    );
    REQUIRE_TRUE(
        strcmp(fault_manager_reason_to_string(FAULT_REASON_ESTOP), "ESTOP") == 0,
        "ESTOP reason string should match"
    );
    REQUIRE_TRUE(
        strcmp(fault_manager_reason_to_string(FAULT_REASON_LIMIT_EXCEEDED), "LIMIT_EXCEEDED") == 0,
        "LIMIT_EXCEEDED reason string should match"
    );
    REQUIRE_TRUE(
        strcmp(fault_manager_reason_to_string(FAULT_REASON_INVALID_COMMAND), "INVALID_COMMAND") == 0,
        "INVALID_COMMAND reason string should match"
    );
    REQUIRE_TRUE(
        strcmp(fault_manager_reason_to_string((fault_reason_t)99), "UNKNOWN") == 0,
        "unknown reason string should match"
    );
    return 0;
}

int main(void)
{
    int failures = 0;

    failures += test_init_starts_without_fault();
    failures += test_set_fault_latches_reason();
    failures += test_none_reason_does_not_activate_fault();
    failures += test_estop_helper_sets_estop_fault();
    failures += test_clear_resets_fault_state();
    failures += test_reason_to_string();

    if (failures == 0) {
        printf("fault manager tests passed\n");
    }

    return failures;
}
