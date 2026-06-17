#ifndef COMMAND_PARSER_H
#define COMMAND_PARSER_H 

#include <stdint.h> 

/**
 * @brief Commands accepted by the MVP text protocol.
 *
 * REQ-003: A small command set keeps the host interface testable before
 * UART, QEMU, and FreeRTOS are introduced.
 */
typedef enum {
    COMMAND_TYPE_NONE = 0, 
    COMMAND_TYPE_PING, 
    COMMAND_TYPE_STATUS, 
    COMMAND_TYPE_MOVE, 
    COMMAND_TYPE_STOP, 
    COMMAND_TYPE_ESTOP,
    COMMAND_TYPE_CLEAR_FAULT
} command_type_t; 

/**
 * @brief Parser outcomes used to generate stable host-facing responses.
 *
 * REQ-004: Explicit parse failures prevent bad command text from being
 * silently interpreted as stale motion data.
 */
typedef enum {
    COMMAND_PARSE_OK = 0, 
    COMMAND_PARSE_ERR_NULL, 
    COMMAND_PARSE_ERR_EMPTY, 
    COMMAND_PARSE_ERR_UNKNOWN_COMMAND,
    COMMAND_PARSE_ERR_MISSING_ARGUMENT, 
    COMMAND_PARSE_ERR_INVALID_ARGUMENT
} command_parse_result_t; 

/**
 * @brief Structured command passed from the parser to the application layer.
 *
 * REQ-003: Positions and feedrate use milli-units so motion code can stay
 * integer-based for embedded targets.
 */
typedef struct{ 
    command_type_t type; 
    int32_t x_milli_mm; 
    int32_t y_milli_mm; 
    uint32_t feedrate_milli_mm_per_s; 
} command_t; 

/**
 * @brief Converts one input line into a safe structured command.
 *
 * REQ-003/REQ-004: This function is the boundary between raw host text and
 * validated firmware data.
 */
command_parse_result_t command_parser_parse(const char *line, command_t *command); 

/**
 * @brief Converts command enum values into stable text labels.
 *
 * REQ-006: Stable labels keep telemetry and tests independent of enum numbers.
 */
const char *command_parser_type_to_string(command_type_t type); 

/**
 * @brief Converts parse results into stable response labels.
 *
 * REQ-004: Stable error strings make host-side testing deterministic.
 */
const char *command_parser_result_to_string(command_parse_result_t result);

#endif
