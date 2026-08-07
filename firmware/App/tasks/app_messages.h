#ifndef APP_MESSAGES_H
#define APP_MESSAGES_H

#include "command_parser.h"
#include "rtos_port.h"

#define APP_COMMAND_QUEUE_CAPACITY 8u

#define APP_EVENT_MOVING (1u << 0u)
#define APP_EVENT_FAULTED (1u << 1u)
#define APP_EVENT_STOP_REQUESTED (1u << 2u)

typedef struct {
    command_t command;
} app_command_message_t;

#endif
