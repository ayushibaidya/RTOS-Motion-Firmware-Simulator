#include "rtos_port.h"

#if defined(USE_FREERTOS) && (USE_FREERTOS == 1)

#include "FreeRTOS.h"
#include "event_groups.h"
#include "queue.h"
#include "semphr.h"

static bool rtos_queue_is_valid(const rtos_queue_t *queue)
{
    return
        queue != NULL &&
        queue->handle != NULL &&
        queue->item_size > 0u &&
        queue->capacity > 0u;
}

static bool rtos_size_fits_ubase_type(size_t value)
{
    return (size_t)((UBaseType_t)value) == value;
}

bool rtos_queue_init(
    rtos_queue_t *queue,
    void *storage,
    size_t item_size,
    size_t capacity
)
{
    if (
        queue == NULL ||
        storage == NULL ||
        item_size == 0u ||
        capacity == 0u ||
        !rtos_size_fits_ubase_type(item_size) ||
        !rtos_size_fits_ubase_type(capacity)
    ) {
        return false;
    }

    /* The real backend uses static FreeRTOS queues so the wrapper preserves the
     * host backend's no-heap contract for firmware-owned message buffers.
     */
    queue->handle = xQueueCreateStatic(
        (UBaseType_t)capacity,
        (UBaseType_t)item_size,
        (uint8_t *)storage,
        &queue->control_block
    );

    if (queue->handle == NULL) {
        queue->item_size = 0u;
        queue->capacity = 0u;
        return false;
    }

    queue->item_size = item_size;
    queue->capacity = capacity;
    return true;
}

bool rtos_queue_send(rtos_queue_t *queue, const void *item)
{
    if (!rtos_queue_is_valid(queue) || item == NULL || rtos_queue_is_full(queue)) {
        return false;
    }

    return xQueueSend(queue->handle, item, 0u) == pdPASS;
}

bool rtos_queue_receive(rtos_queue_t *queue, void *item)
{
    if (!rtos_queue_is_valid(queue) || item == NULL || rtos_queue_is_empty(queue)) {
        return false;
    }

    return xQueueReceive(queue->handle, item, 0u) == pdPASS;
}

bool rtos_queue_is_empty(const rtos_queue_t *queue)
{
    if (!rtos_queue_is_valid(queue)) {
        return true;
    }

    return uxQueueMessagesWaiting(queue->handle) == 0u;
}

bool rtos_queue_is_full(const rtos_queue_t *queue)
{
    if (!rtos_queue_is_valid(queue)) {
        return false;
    }

    return uxQueueMessagesWaiting(queue->handle) >= (UBaseType_t)queue->capacity;
}

size_t rtos_queue_count(const rtos_queue_t *queue)
{
    if (!rtos_queue_is_valid(queue)) {
        return 0u;
    }

    return (size_t)uxQueueMessagesWaiting(queue->handle);
}

static bool rtos_mutex_is_valid(const rtos_mutex_t *mutex)
{
    return mutex != NULL && mutex->handle != NULL;
}

bool rtos_mutex_init(rtos_mutex_t *mutex)
{
    if (mutex == NULL) {
        return false;
    }

    /* A static mutex keeps shared-state protection compatible with embedded
     * targets that avoid runtime allocation.
     */
    mutex->handle = xSemaphoreCreateMutexStatic(&mutex->control_block);
    return mutex->handle != NULL;
}

bool rtos_mutex_lock(rtos_mutex_t *mutex)
{
    if (!rtos_mutex_is_valid(mutex)) {
        return false;
    }

    return xSemaphoreTake(mutex->handle, 0u) == pdPASS;
}

bool rtos_mutex_unlock(rtos_mutex_t *mutex)
{
    if (!rtos_mutex_is_valid(mutex)) {
        return false;
    }

    return xSemaphoreGive(mutex->handle) == pdPASS;
}

static bool rtos_event_group_is_valid(const rtos_event_group_t *event_group)
{
    return event_group != NULL && event_group->handle != NULL;
}

bool rtos_event_group_init(rtos_event_group_t *event_group)
{
    if (event_group == NULL) {
        return false;
    }

    /* Event groups are represented by real FreeRTOS event bits in this backend
     * while preserving the same wrapper API used by host tests.
     */
    event_group->handle = xEventGroupCreateStatic(&event_group->control_block);

    if (event_group->handle == NULL) {
        return false;
    }

    return true;
}

rtos_event_bits_t rtos_event_group_set_bits(
    rtos_event_group_t *event_group,
    rtos_event_bits_t bits
)
{
    if (!rtos_event_group_is_valid(event_group)) {
        return 0u;
    }

    return xEventGroupSetBits(event_group->handle, (EventBits_t)bits);
}

rtos_event_bits_t rtos_event_group_clear_bits(
    rtos_event_group_t *event_group,
    rtos_event_bits_t bits
)
{
    if (!rtos_event_group_is_valid(event_group)) {
        return 0u;
    }

    (void)xEventGroupClearBits(event_group->handle, (EventBits_t)bits);
    return xEventGroupGetBits(event_group->handle);
}

rtos_event_bits_t rtos_event_group_get_bits(
    const rtos_event_group_t *event_group
)
{
    if (!rtos_event_group_is_valid(event_group)) {
        return 0u;
    }

    return xEventGroupGetBits(event_group->handle);
}

#endif
