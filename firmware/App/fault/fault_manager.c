#include "fault_manager.h"

/* Fault state is owned by this module so future FreeRTOS tasks cannot mutate
 * safety state through scattered globals.
 */
static bool fault_active; 
static fault_reason_t current_reason; 

/* REQ-007: A fresh firmware boot starts clear; persistent faults will be added
 * later only if the design gains non-volatile fault storage.
 */
void fault_manager_init(void){
    fault_active = false; 
    current_reason = FAULT_REASON_NONE; 
}

/* REQ-007: Ignore NONE so callers can safely pass default enum values without
 * accidentally latching the controller into fault mode.
 */
void fault_manager_set_fault(fault_reason_t reason){
    if(reason == FAULT_REASON_NONE){
        return; 
    }
    fault_active = true; 
    current_reason = reason; 
}

/* REQ-007: ESTOP is kept as a named wrapper so command handling can express
 * safety intent directly instead of passing a raw enum everywhere.
 */
void fault_manager_trigger_estop(void){
    fault_manager_set_fault(FAULT_REASON_ESTOP); 
}

/* REQ-007: Clearing requires an explicit call so recovery from ESTOP or limit
 * faults is deliberate rather than automatic.
 */
void fault_manager_clear(void){
    fault_active = false; 
    current_reason = FAULT_REASON_NONE; 
}

/* REQ-007: Expose a simple safety gate for motion and command modules without
 * exposing the private fault variables.
 */
bool fault_manager_is_fault_active(void){
    return fault_active; 
}

/* REQ-007: The exact reason is exposed for verification and telemetry, while
 * the stored state remains private to this module.
 */
fault_reason_t fault_manager_get_reason(void){
    return current_reason; 
}

/* REQ-006/REQ-007: Stable string values make logs and future Python tests easy
 * to parse without depending on enum integer values.
 */
const char *fault_manager_reason_to_string(fault_reason_t reason){
    switch(reason){
        case FAULT_REASON_NONE: 
            return "NONE"; 
        case FAULT_REASON_ESTOP: 
            return "ESTOP";
        case FAULT_REASON_LIMIT_EXCEEDED: 
            return "LIMIT_EXCEEDED"; 
        case FAULT_REASON_INVALID_COMMAND: 
            return "INVALID_COMMAND"; 
        default: 
            return "UNKNOWN"; 
    }
}
