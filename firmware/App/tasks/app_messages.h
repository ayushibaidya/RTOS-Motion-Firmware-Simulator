#ifndef APP_MESSAGES_H
#define APP_MESSAGES_H

#include "command_parser.h"
#include "rtos_port.h"

/* Bounded message pools keep host tests aligned with embedded memory limits
 * instead of hiding overflow behind dynamic allocation.
 */
#define APP_COMMAND_QUEUE_CAPACITY 8u
#define APP_TELEMETRY_QUEUE_CAPACITY 16u
#define APP_TELEMETRY_MESSAGE_LENGTH 128u

#define APP_EVENT_MOVING (1u << 0u)
#define APP_EVENT_FAULTED (1u << 1u)
#define APP_EVENT_STOP_REQUESTED (1u << 2u)

typedef struct {
    command_t command;
} app_command_message_t;

/* Task responses use fixed-size telemetry messages so command and motion logic
 * can report outcomes without directly owning the output device.
 */
typedef struct {
    char text[APP_TELEMETRY_MESSAGE_LENGTH];
} app_telemetry_message_t;

#endif
