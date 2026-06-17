#include "command_parser.h"
#include "fault_manager.h"
#include "motion_controller.h"
#include "telemetry.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define MAIN_TICK_MS 100u
#define MAIN_SIMULATION_STEPS 20u

static const char MAIN_RESPONSE_PONG[] = "OK PONG";
static const char MAIN_RESPONSE_STATUS[] = "OK STATUS";
static const char MAIN_RESPONSE_MOVE_QUEUED[] = "OK MOVE QUEUED";
static const char MAIN_RESPONSE_STOPPED[] = "OK STOPPED";
static const char MAIN_RESPONSE_ESTOP[] = "OK ESTOP";
static const char MAIN_RESPONSE_FAULT_CLEARED[] = "OK FAULT CLEARED";
static const char MAIN_ERROR_FAULT_ACTIVE[] = "ERR FAULT_ACTIVE";
static const char MAIN_ERROR_MOVE_REJECTED[] = "ERR MOVE_REJECTED";
static const char MAIN_ERROR_UNKNOWN_COMMAND[] = "ERR UNKNOWN_COMMAND";
static const char MAIN_DEMO_COMMAND_PING[] = "PING";
static const char MAIN_DEMO_COMMAND_MOVE[] = "MOVE X=50 Y=20 F=25";
static const char MAIN_DEMO_COMMAND_ESTOP[] = "ESTOP";
static const char MAIN_DEMO_COMMAND_CLEAR_FAULT[] = "CLEAR_FAULT";

/* The MVP uses stdout as the telemetry sink so module integration can be
 * verified before UART/QEMU output is added.
 */
static void main_telemetry_writer(const char *message){
    if(message == NULL){
        return; 
    }
    printf("%s", message); 
}

/* REQ-006: Main owns the adapter from motion/fault state into telemetry
 * snapshots so telemetry formatting stays independent from motion internals.
 */
static void main_send_motion_status(uint32_t uptime_ms){
    motion_status_t motion_status; 
    telemetry_status_t telemetry_status; 

    motion_controller_get_status(&motion_status);

    telemetry_status.uptime_ms = uptime_ms;
    telemetry_status.state = motion_controller_state_to_string(motion_status.state);
    telemetry_status.x_milli_mm = motion_status.current_x_milli_mm;
    telemetry_status.y_milli_mm = motion_status.current_y_milli_mm;
    telemetry_status.fault_active = fault_manager_is_fault_active();

    telemetry_send_status(&telemetry_status);
}

/* REQ-003/REQ-007: Main is the orchestration layer that translates validated
 * commands into motion or safety actions without putting policy in the parser.
 */
static void main_handle_command(const char *line)
{
    command_t command;
    command_parse_result_t parse_result;

    parse_result = command_parser_parse(line, &command);

    if (parse_result != COMMAND_PARSE_OK) {
        telemetry_send_line(command_parser_result_to_string(parse_result));
        return;
    }

    switch (command.type) {
    case COMMAND_TYPE_PING:
        telemetry_send_line(MAIN_RESPONSE_PONG);
        break;
    case COMMAND_TYPE_STATUS:
        telemetry_send_line(MAIN_RESPONSE_STATUS);
        break;
    case COMMAND_TYPE_MOVE:
        if (fault_manager_is_fault_active()) {
            telemetry_send_line(MAIN_ERROR_FAULT_ACTIVE);
            break;
        }

        if (motion_controller_start_move(
                command.x_milli_mm,
                command.y_milli_mm,
                command.feedrate_milli_mm_per_s
            )) {
            telemetry_send_line(MAIN_RESPONSE_MOVE_QUEUED);
        } else {
            telemetry_send_line(MAIN_ERROR_MOVE_REJECTED);
        }
        break;
    case COMMAND_TYPE_STOP:
        motion_controller_stop();
        telemetry_send_line(MAIN_RESPONSE_STOPPED);
        break;
    case COMMAND_TYPE_ESTOP:
        fault_manager_trigger_estop();
        motion_controller_set_fault();
        telemetry_send_line(MAIN_RESPONSE_ESTOP);
        break;
    case COMMAND_TYPE_CLEAR_FAULT:
        fault_manager_clear();
        motion_controller_clear_fault();
        telemetry_send_line(MAIN_RESPONSE_FAULT_CLEARED);
        break;
    case COMMAND_TYPE_NONE:
    default:
        telemetry_send_line(MAIN_ERROR_UNKNOWN_COMMAND);
        break;
    }
}

/* The finite demo loop keeps the MVP runnable under host tests/builds before
 * the project gains a real FreeRTOS scheduler.
 */
int main(void){
    uint32_t uptime_ms = 0u;
    uint32_t step;

    telemetry_init();
    telemetry_set_writer(main_telemetry_writer);
    motion_controller_init();
    fault_manager_init();

    telemetry_send_startup();

    main_handle_command(MAIN_DEMO_COMMAND_PING);
    main_handle_command(MAIN_DEMO_COMMAND_MOVE);

    for (step = 0u; step < MAIN_SIMULATION_STEPS; step++) {
        uptime_ms += MAIN_TICK_MS;
        motion_controller_update(MAIN_TICK_MS);
        main_send_motion_status(uptime_ms);
    }

    main_handle_command(MAIN_DEMO_COMMAND_ESTOP);
    main_send_motion_status(uptime_ms);
    main_handle_command(MAIN_DEMO_COMMAND_CLEAR_FAULT);
    main_send_motion_status(uptime_ms);
    main_handle_command("STATUS");

    return 0;
}
