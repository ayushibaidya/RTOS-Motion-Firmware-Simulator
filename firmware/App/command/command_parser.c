#include "command_parser.h"

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define COMMAND_PARSER_MM_SCALE 1000
#define COMMAND_PARSER_LINE_BUFFER_SIZE 96
#define COMMAND_PARSER_TOKEN_DELIMITERS " \t\r\n"

static const char COMMAND_TEXT_PING[] = "PING";
static const char COMMAND_TEXT_STATUS[] = "STATUS";
static const char COMMAND_TEXT_MOVE[] = "MOVE";
static const char COMMAND_TEXT_STOP[] = "STOP";
static const char COMMAND_TEXT_ESTOP[] = "ESTOP";
static const char COMMAND_TEXT_CLEAR_FAULT[] = "CLEAR_FAULT";
static const char COMMAND_TEXT_NONE[] = "NONE";
static const char COMMAND_TEXT_UNKNOWN[] = "UNKNOWN";
static const char COMMAND_PREFIX_X[] = "X=";
static const char COMMAND_PREFIX_Y[] = "Y=";
static const char COMMAND_PREFIX_F[] = "F=";
static const char COMMAND_PARSE_TEXT_OK[] = "OK";
static const char COMMAND_PARSE_TEXT_ERR_NULL[] = "ERR_NULL";
static const char COMMAND_PARSE_TEXT_ERR_EMPTY[] = "ERR_EMPTY";
static const char COMMAND_PARSE_TEXT_ERR_UNKNOWN_COMMAND[] = "ERR_UNKNOWN_COMMAND";
static const char COMMAND_PARSE_TEXT_ERR_MISSING_ARGUMENT[] = "ERR_MISSING_ARGUMENT";
static const char COMMAND_PARSE_TEXT_ERR_INVALID_ARGUMENT[] = "ERR_INVALID_ARGUMENT";
static const char COMMAND_PARSE_TEXT_ERR_UNKNOWN[] = "ERR_UNKNOWN";
static const size_t COMMAND_PREFIX_LENGTH = 2u;

/* REQ-004: Parser output is cleared before every parse attempt so invalid
 * input cannot accidentally reuse a previous MOVE command.
 */
static void command_parser_reset_command(command_t *command){
    if(command == NULL){
        return; 
    }

    command->type = COMMAND_TYPE_NONE; 
    command->x_milli_mm = 0; 
    command->y_milli_mm = 0; 
    command->feedrate_milli_mm_per_s = 0u; 
}

/* REQ-004: Whitespace-only input is treated as an explicit parser error so
 * blank serial lines do not look like unknown commands.
 */
static bool command_parser_is_empty_line(const char *line){
    if(line == NULL){
        return true; 
    }
    while(*line != '\0'){
        if(*line != ' ' && *line != '\t' && *line != '\n' && *line != '\r'){
            return false; 
        }
        line++; 
    }
    return true; 
}

/* REQ-003: Signed positions are converted to milli-millimeters at the parser
 * boundary so the motion module never needs to parse text or use floats.
 */
static bool command_parser_parse_i32_mm(const char *text, int32_t *out_milli_mm){
    char *end = NULL;
    long value_mm;

    if(text == NULL || out_milli_mm == NULL || *text == '\0'){
        return false; 
    }
    value_mm = strtol(text, &end, 10); 

    if(*end != '\0'){
        return false; 
    }

    if (
        value_mm > (INT32_MAX / COMMAND_PARSER_MM_SCALE) ||
        value_mm < (INT32_MIN / COMMAND_PARSER_MM_SCALE)
    ) {
        return false;
    }

    *out_milli_mm = (int32_t)(value_mm * COMMAND_PARSER_MM_SCALE);
    return true;
}

/* REQ-003: Feedrate is unsigned and nonzero because negative or zero motion
 * speed has no useful meaning for the MVP motion model.
 */
static bool command_parser_parse_u32_mm_per_s(const char *text, uint32_t *out_milli_mm_per_s){
    char *end = NULL; 
    unsigned long value_mm_per_s; 

    if (text == NULL || out_milli_mm_per_s == NULL || *text == '\0') {
        return false;
    }

    value_mm_per_s = strtoul(text, &end, 10);

    if (*end != '\0') {
        return false;
    }

    if (value_mm_per_s == 0u) {
        return false;
    }

    if (value_mm_per_s > (UINT32_MAX / COMMAND_PARSER_MM_SCALE)) {
        return false;
    }

    *out_milli_mm_per_s = (uint32_t)(value_mm_per_s * COMMAND_PARSER_MM_SCALE);
    return true;
}

/* REQ-003/REQ-004: MOVE has required X/Y/F fields so motion requests are
 * complete before the motion controller receives them.
 */
static command_parse_result_t command_parser_parse_move(char *line, command_t *command){
    char *token;
    bool saw_x = false;
    bool saw_y = false;
    bool saw_f = false;

    if (line == NULL || command == NULL) {
        return COMMAND_PARSE_ERR_NULL;
    }

    token = strtok(line, COMMAND_PARSER_TOKEN_DELIMITERS);
    if (token == NULL || strcmp(token, COMMAND_TEXT_MOVE) != 0) {
        return COMMAND_PARSE_ERR_UNKNOWN_COMMAND;
    }

    command->type = COMMAND_TYPE_MOVE;

    while ((token = strtok(NULL, COMMAND_PARSER_TOKEN_DELIMITERS)) != NULL) {
        if (strncmp(token, COMMAND_PREFIX_X, COMMAND_PREFIX_LENGTH) == 0) {
            if (saw_x || !command_parser_parse_i32_mm(&token[COMMAND_PREFIX_LENGTH], &command->x_milli_mm)) {
                return COMMAND_PARSE_ERR_INVALID_ARGUMENT;
            }
            saw_x = true;
        } else if (strncmp(token, COMMAND_PREFIX_Y, COMMAND_PREFIX_LENGTH) == 0) {
            if (saw_y || !command_parser_parse_i32_mm(&token[COMMAND_PREFIX_LENGTH], &command->y_milli_mm)) {
                return COMMAND_PARSE_ERR_INVALID_ARGUMENT;
            }
            saw_y = true;
        } else if (strncmp(token, COMMAND_PREFIX_F, COMMAND_PREFIX_LENGTH) == 0) {
            if (saw_f || !command_parser_parse_u32_mm_per_s(&token[COMMAND_PREFIX_LENGTH], &command->feedrate_milli_mm_per_s)) {
                return COMMAND_PARSE_ERR_INVALID_ARGUMENT;
            }
            saw_f = true;
        } else {
            return COMMAND_PARSE_ERR_INVALID_ARGUMENT;
        }
    }

    if (!saw_x || !saw_y || !saw_f) {
        return COMMAND_PARSE_ERR_MISSING_ARGUMENT;
    }

    return COMMAND_PARSE_OK;
}

/* REQ-003/REQ-004: The top-level parser accepts one bounded command line so
 * future UART input can reuse the same validation path.
 */
command_parse_result_t command_parser_parse(const char *line, command_t *command){
    char command_line[COMMAND_PARSER_LINE_BUFFER_SIZE];
    char move_line[COMMAND_PARSER_LINE_BUFFER_SIZE];
    char *token;
    command_parse_result_t result;
    size_t line_length;

    if (command == NULL) {
        return COMMAND_PARSE_ERR_NULL;
    }

    command_parser_reset_command(command);

    if (line == NULL) {
        return COMMAND_PARSE_ERR_NULL;
    }

    if (command_parser_is_empty_line(line)) {
        return COMMAND_PARSE_ERR_EMPTY;
    }

    line_length = strlen(line);
    if (line_length >= COMMAND_PARSER_LINE_BUFFER_SIZE) {
        return COMMAND_PARSE_ERR_INVALID_ARGUMENT;
    }

    memcpy(command_line, line, line_length + 1u);
    memcpy(move_line, line, line_length + 1u);

    token = strtok(command_line, COMMAND_PARSER_TOKEN_DELIMITERS);
    if (token == NULL) {
        return COMMAND_PARSE_ERR_EMPTY;
    }

    if (strcmp(token, COMMAND_TEXT_PING) == 0) {
        if (strtok(NULL, COMMAND_PARSER_TOKEN_DELIMITERS) != NULL) {
            return COMMAND_PARSE_ERR_INVALID_ARGUMENT;
        }
        command->type = COMMAND_TYPE_PING;
        return COMMAND_PARSE_OK;
    }

    if (strcmp(token, COMMAND_TEXT_STATUS) == 0) {
        if (strtok(NULL, COMMAND_PARSER_TOKEN_DELIMITERS) != NULL) {
            return COMMAND_PARSE_ERR_INVALID_ARGUMENT;
        }
        command->type = COMMAND_TYPE_STATUS;
        return COMMAND_PARSE_OK;
    }

    if (strcmp(token, COMMAND_TEXT_STOP) == 0) {
        if (strtok(NULL, COMMAND_PARSER_TOKEN_DELIMITERS) != NULL) {
            return COMMAND_PARSE_ERR_INVALID_ARGUMENT;
        }
        command->type = COMMAND_TYPE_STOP;
        return COMMAND_PARSE_OK;
    }

    if (strcmp(token, COMMAND_TEXT_ESTOP) == 0) {
        if (strtok(NULL, COMMAND_PARSER_TOKEN_DELIMITERS) != NULL) {
            return COMMAND_PARSE_ERR_INVALID_ARGUMENT;
        }
        command->type = COMMAND_TYPE_ESTOP;
        return COMMAND_PARSE_OK;
    }

    if (strcmp(token, COMMAND_TEXT_CLEAR_FAULT) == 0) {
        if (strtok(NULL, COMMAND_PARSER_TOKEN_DELIMITERS) != NULL) {
            return COMMAND_PARSE_ERR_INVALID_ARGUMENT;
        }
        command->type = COMMAND_TYPE_CLEAR_FAULT;
        return COMMAND_PARSE_OK;
    }

    if (strcmp(token, COMMAND_TEXT_MOVE) == 0) {
        result = command_parser_parse_move(move_line, command);
        if (result != COMMAND_PARSE_OK) {
            command_parser_reset_command(command);
        }
        return result;
    }

    return COMMAND_PARSE_ERR_UNKNOWN_COMMAND;
}

/* REQ-006: String conversion keeps logs/tests readable without exposing enum
 * integer values as part of the external protocol.
 */
const char *command_parser_type_to_string(command_type_t type){
    switch (type) {
    case COMMAND_TYPE_NONE:
        return COMMAND_TEXT_NONE;
    case COMMAND_TYPE_PING:
        return COMMAND_TEXT_PING;
    case COMMAND_TYPE_STATUS:
        return COMMAND_TEXT_STATUS;
    case COMMAND_TYPE_MOVE:
        return COMMAND_TEXT_MOVE;
    case COMMAND_TYPE_STOP:
        return COMMAND_TEXT_STOP;
    case COMMAND_TYPE_ESTOP:
        return COMMAND_TEXT_ESTOP;
    case COMMAND_TYPE_CLEAR_FAULT:
        return COMMAND_TEXT_CLEAR_FAULT;
    default:
        return COMMAND_TEXT_UNKNOWN;
    }
}

/* REQ-004: Stable parse-result text lets the application layer emit consistent
 * ERR responses without duplicating parser error mapping logic.
 */
const char *command_parser_result_to_string(command_parse_result_t result){
    switch (result) {
    case COMMAND_PARSE_OK:
        return COMMAND_PARSE_TEXT_OK;
    case COMMAND_PARSE_ERR_NULL:
        return COMMAND_PARSE_TEXT_ERR_NULL;
    case COMMAND_PARSE_ERR_EMPTY:
        return COMMAND_PARSE_TEXT_ERR_EMPTY;
    case COMMAND_PARSE_ERR_UNKNOWN_COMMAND:
        return COMMAND_PARSE_TEXT_ERR_UNKNOWN_COMMAND;
    case COMMAND_PARSE_ERR_MISSING_ARGUMENT:
        return COMMAND_PARSE_TEXT_ERR_MISSING_ARGUMENT;
    case COMMAND_PARSE_ERR_INVALID_ARGUMENT:
        return COMMAND_PARSE_TEXT_ERR_INVALID_ARGUMENT;
    default:
        return COMMAND_PARSE_TEXT_ERR_UNKNOWN;
    }
}
