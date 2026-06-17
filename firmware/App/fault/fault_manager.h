#ifndef FAULT_MANAGER_H
#define FAULT_MANAGER_H

#include <stdbool.h> 

/**
 * @brief Lists the possible fault reasons tracked by the firmware.
 *
 * REQ-007: Named fault reasons make safety behavior testable and allow
 * telemetry/tests to distinguish emergency stops from bounds or command faults.
 */
typedef enum {
    FAULT_REASON_NONE = 0, 
    FAULT_REASON_ESTOP, 
    FAULT_REASON_LIMIT_EXCEEDED, 
    FAULT_REASON_INVALID_COMMAND
} fault_reason_t; 

/**
 * @brief Resets the fault manager to the default safe state.
 *
 * REQ-007: Startup must begin with no latched software fault so a previous
 * simulation run cannot block a fresh firmware session.
 */
void fault_manager_init(void); 

/**
 * @brief Activates a fault with the given reason.
 *
 * REQ-007: Faults are latched through one API so motion-control code can block
 * unsafe movement without knowing which subsystem detected the problem.
 */
void fault_manager_set_fault(fault_reason_t reason); 

/**
 * @brief Clears the active fault and returns the fault reason to NONE.
 *
 * REQ-007: Clearing is explicit so future command handling can require an
 * intentional recovery step after ESTOP or limit events.
 */
void fault_manager_clear(void); 

/**
 * @brief Returns whether the firmware is currently in a fault state.
 *
 * REQ-007: Motion and command modules need a single safety gate before
 * accepting new movement.
 */
bool fault_manager_is_fault_active(void);

/**
 * @brief Triggers an emergency-stop fault.
 *
 * REQ-007: ESTOP is represented as a dedicated helper because it is the most
 * safety-critical fault path and should remain obvious at call sites.
 */
void fault_manager_trigger_estop(void); 

/**
 * @brief Returns the currently stored fault reason.
 *
 * REQ-007: Tests and telemetry need the stored reason to verify fault behavior,
 * not just that some generic fault occurred.
 */
fault_reason_t fault_manager_get_reason(void);

/**
 * @brief Converts a fault reason into readable text for logs or telemetry.
 *
 * REQ-006/REQ-007: Text reasons keep telemetry machine-readable while still
 * being understandable during debugging.
 */
const char *fault_manager_reason_to_string(fault_reason_t reason); 

#endif 
