#include "command_parser.h"

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

static int test_ping_command(void)
{
    command_t command;
    command_parse_result_t result = command_parser_parse("PING", &command);

    REQUIRE_TRUE(result == COMMAND_PARSE_OK, "PING should parse successfully");
    REQUIRE_TRUE(command.type == COMMAND_TYPE_PING, "PING should set command type");
    REQUIRE_TRUE(command.x_milli_mm == 0, "PING should not set X");
    REQUIRE_TRUE(command.y_milli_mm == 0, "PING should not set Y");
    REQUIRE_TRUE(command.feedrate_milli_mm_per_s == 0u, "PING should not set feedrate");
    return 0;
}

static int test_status_stop_estop_commands(void)
{
    command_t command;

    REQUIRE_TRUE(command_parser_parse("STATUS", &command) == COMMAND_PARSE_OK, "STATUS should parse");
    REQUIRE_TRUE(command.type == COMMAND_TYPE_STATUS, "STATUS should set command type");

    REQUIRE_TRUE(command_parser_parse("STOP", &command) == COMMAND_PARSE_OK, "STOP should parse");
    REQUIRE_TRUE(command.type == COMMAND_TYPE_STOP, "STOP should set command type");

    REQUIRE_TRUE(command_parser_parse("ESTOP", &command) == COMMAND_PARSE_OK, "ESTOP should parse");
    REQUIRE_TRUE(command.type == COMMAND_TYPE_ESTOP, "ESTOP should set command type");

    REQUIRE_TRUE(command_parser_parse("CLEAR_FAULT", &command) == COMMAND_PARSE_OK, "CLEAR_FAULT should parse");
    REQUIRE_TRUE(command.type == COMMAND_TYPE_CLEAR_FAULT, "CLEAR_FAULT should set command type");
    return 0;
}

static int test_valid_move_command(void)
{
    command_t command;
    command_parse_result_t result = command_parser_parse("MOVE X=50 Y=-20 F=600", &command);

    REQUIRE_TRUE(result == COMMAND_PARSE_OK, "valid MOVE should parse successfully");
    REQUIRE_TRUE(command.type == COMMAND_TYPE_MOVE, "MOVE should set command type");
    REQUIRE_TRUE(command.x_milli_mm == 50000, "MOVE should convert X mm to milli-mm");
    REQUIRE_TRUE(command.y_milli_mm == -20000, "MOVE should convert signed Y mm to milli-mm");
    REQUIRE_TRUE(command.feedrate_milli_mm_per_s == 600000u, "MOVE should convert feedrate to milli-mm/s");
    return 0;
}

static int test_move_missing_arguments(void)
{
    command_t command;

    REQUIRE_TRUE(
        command_parser_parse("MOVE X=50 Y=20", &command) == COMMAND_PARSE_ERR_MISSING_ARGUMENT,
        "MOVE without F should fail"
    );
    REQUIRE_TRUE(
        command_parser_parse("MOVE X=50 F=600", &command) == COMMAND_PARSE_ERR_MISSING_ARGUMENT,
        "MOVE without Y should fail"
    );
    REQUIRE_TRUE(
        command_parser_parse("MOVE Y=20 F=600", &command) == COMMAND_PARSE_ERR_MISSING_ARGUMENT,
        "MOVE without X should fail"
    );
    return 0;
}

static int test_move_invalid_arguments(void)
{
    command_t command;

    REQUIRE_TRUE(
        command_parser_parse("MOVE X=abc Y=20 F=600", &command) == COMMAND_PARSE_ERR_INVALID_ARGUMENT,
        "MOVE should reject invalid X"
    );
    REQUIRE_TRUE(
        command_parser_parse("MOVE X=50 Y=bad F=600", &command) == COMMAND_PARSE_ERR_INVALID_ARGUMENT,
        "MOVE should reject invalid Y"
    );
    REQUIRE_TRUE(
        command_parser_parse("MOVE X=50 Y=20 F=0", &command) == COMMAND_PARSE_ERR_INVALID_ARGUMENT,
        "MOVE should reject zero feedrate"
    );
    REQUIRE_TRUE(
        command_parser_parse("MOVE X=50 Y=20 F=-5", &command) == COMMAND_PARSE_ERR_INVALID_ARGUMENT,
        "MOVE should reject negative feedrate"
    );
    REQUIRE_TRUE(
        command_parser_parse("MOVE X=50 Y=20 F=600 Z=1", &command) == COMMAND_PARSE_ERR_INVALID_ARGUMENT,
        "MOVE should reject unknown arguments"
    );
    return 0;
}

static int test_simple_commands_reject_extra_arguments(void)
{
    command_t command;

    REQUIRE_TRUE(
        command_parser_parse("PING NOW", &command) == COMMAND_PARSE_ERR_INVALID_ARGUMENT,
        "PING should reject extra arguments"
    );
    REQUIRE_TRUE(
        command_parser_parse("STATUS PLEASE", &command) == COMMAND_PARSE_ERR_INVALID_ARGUMENT,
        "STATUS should reject extra arguments"
    );
    REQUIRE_TRUE(
        command_parser_parse("CLEAR_FAULT NOW", &command) == COMMAND_PARSE_ERR_INVALID_ARGUMENT,
        "CLEAR_FAULT should reject extra arguments"
    );
    return 0;
}

static int test_empty_unknown_and_null_inputs(void)
{
    command_t command;

    REQUIRE_TRUE(command_parser_parse("", &command) == COMMAND_PARSE_ERR_EMPTY, "empty string should fail");
    REQUIRE_TRUE(command_parser_parse("   \t\r\n", &command) == COMMAND_PARSE_ERR_EMPTY, "whitespace should fail");
    REQUIRE_TRUE(command_parser_parse("BANANA", &command) == COMMAND_PARSE_ERR_UNKNOWN_COMMAND, "unknown command should fail");
    REQUIRE_TRUE(command_parser_parse(NULL, &command) == COMMAND_PARSE_ERR_NULL, "NULL line should fail");
    REQUIRE_TRUE(command_parser_parse("PING", NULL) == COMMAND_PARSE_ERR_NULL, "NULL command output should fail");
    return 0;
}

static int test_failed_parse_resets_command_output(void)
{
    command_t command;

    REQUIRE_TRUE(command_parser_parse("MOVE X=50 Y=20 F=600", &command) == COMMAND_PARSE_OK, "valid MOVE should parse");
    REQUIRE_TRUE(command_parser_parse("BANANA", &command) == COMMAND_PARSE_ERR_UNKNOWN_COMMAND, "invalid command should fail");
    REQUIRE_TRUE(command.type == COMMAND_TYPE_NONE, "failed parse should reset command type");
    REQUIRE_TRUE(command.x_milli_mm == 0, "failed parse should reset X");
    REQUIRE_TRUE(command.y_milli_mm == 0, "failed parse should reset Y");
    REQUIRE_TRUE(command.feedrate_milli_mm_per_s == 0u, "failed parse should reset feedrate");
    return 0;
}

static int test_string_conversions(void)
{
    REQUIRE_TRUE(strcmp(command_parser_type_to_string(COMMAND_TYPE_MOVE), "MOVE") == 0, "MOVE string should match");
    REQUIRE_TRUE(
        strcmp(command_parser_type_to_string(COMMAND_TYPE_CLEAR_FAULT), "CLEAR_FAULT") == 0,
        "CLEAR_FAULT string should match"
    );
    REQUIRE_TRUE(
        strcmp(command_parser_type_to_string((command_type_t)99), "UNKNOWN") == 0,
        "unknown command type should stringify"
    );
    REQUIRE_TRUE(
        strcmp(command_parser_result_to_string(COMMAND_PARSE_ERR_INVALID_ARGUMENT), "ERR_INVALID_ARGUMENT") == 0,
        "invalid argument result string should match"
    );
    REQUIRE_TRUE(
        strcmp(command_parser_result_to_string((command_parse_result_t)99), "ERR_UNKNOWN") == 0,
        "unknown parse result should stringify"
    );
    return 0;
}

int main(void)
{
    int failures = 0;

    failures += test_ping_command();
    failures += test_status_stop_estop_commands();
    failures += test_valid_move_command();
    failures += test_move_missing_arguments();
    failures += test_move_invalid_arguments();
    failures += test_simple_commands_reject_extra_arguments();
    failures += test_empty_unknown_and_null_inputs();
    failures += test_failed_parse_resets_command_output();
    failures += test_string_conversions();

    if (failures == 0) {
        printf("command parser tests passed\n");
    }

    return failures;
}
