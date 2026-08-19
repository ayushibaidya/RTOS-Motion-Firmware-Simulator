#ifndef RTOS_PORT_H
#define RTOS_PORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if defined(USE_FREERTOS) && (USE_FREERTOS == 1)
#include "FreeRTOS.h"
#include "event_groups.h"
#include "queue.h"
#include "semphr.h"

typedef EventBits_t rtos_event_bits_t;

typedef struct {
    QueueHandle_t handle;
    StaticQueue_t control_block;
    size_t item_size;
    size_t capacity;
} rtos_queue_t;

typedef struct {
    SemaphoreHandle_t handle;
    StaticSemaphore_t control_block;
} rtos_mutex_t;

typedef struct {
    EventGroupHandle_t handle;
    StaticEventGroup_t control_block;
} rtos_event_group_t;
#else
typedef uint32_t rtos_event_bits_t;

typedef struct {
    uint8_t *storage;
    size_t item_size;
    size_t capacity;
    size_t head;
    size_t tail;
    size_t count;
} rtos_queue_t;

typedef struct {
    bool locked;
} rtos_mutex_t;

typedef struct {
    rtos_event_bits_t bits;
} rtos_event_group_t;
#endif

/* This RTOS contract lets application code use queue-style message passing
 * without depending directly on one backend. Host builds use a fake backend;
 * FreeRTOS builds use real kernel objects.
 */
bool rtos_queue_init(rtos_queue_t *queue, void *storage, size_t item_size, size_t capacity);
bool rtos_queue_send(rtos_queue_t *queue, const void *item);
bool rtos_queue_receive(rtos_queue_t *queue, void *item);
bool rtos_queue_is_empty(const rtos_queue_t *queue);
bool rtos_queue_is_full(const rtos_queue_t *queue);
size_t rtos_queue_count(const rtos_queue_t *queue);

/* Mutex wrappers define the shared-state protection points that will later map
 * to real FreeRTOS mutex APIs.
 */
bool rtos_mutex_init(rtos_mutex_t *mutex);
bool rtos_mutex_lock(rtos_mutex_t *mutex);
bool rtos_mutex_unlock(rtos_mutex_t *mutex);

/* Event group wrappers model system flags such as MOVING, FAULTED, and
 * STOP_REQUESTED without coupling task code to a specific RTOS backend.
 */
bool rtos_event_group_init(rtos_event_group_t *event_group);
rtos_event_bits_t rtos_event_group_set_bits(rtos_event_group_t *event_group, rtos_event_bits_t bits);
rtos_event_bits_t rtos_event_group_clear_bits(rtos_event_group_t *event_group, rtos_event_bits_t bits);
rtos_event_bits_t rtos_event_group_get_bits(const rtos_event_group_t *event_group);

#endif 
