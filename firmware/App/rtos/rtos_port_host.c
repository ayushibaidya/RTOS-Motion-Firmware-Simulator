#include "rtos_port.h"

#include <string.h>

/* This host adapter preserves embedded-style RTOS boundaries while keeping the
 * project runnable and testable on a development machine without hardware.
 */

static bool rtos_queue_is_valid(const rtos_queue_t *queue)
{
    return
        queue != NULL &&
        queue->storage != NULL &&
        queue->item_size > 0u &&
        queue->capacity > 0u;
}

bool rtos_queue_init(
    rtos_queue_t *queue,
    void *storage,
    size_t item_size,
    size_t capacity
)
{
    /* Static caller-owned storage avoids heap allocation, matching the memory
     * discipline expected from the later embedded FreeRTOS target.
     */
    if(queue == NULL || storage == NULL || item_size == 0u || capacity == 0u) {
        return false;
    }

    queue->storage = (uint8_t *)storage;
    queue->item_size = item_size;
    queue->capacity = capacity;
    queue->head = 0u;
    queue->tail = 0u;
    queue->count = 0u;

    return true;
}

bool rtos_queue_send(rtos_queue_t *queue, const void *item)
{
    /* Bounded send behavior makes command overflow deterministic instead of
     * silently growing memory usage on the host.
     */
    uint8_t *destination; 

    if(!rtos_queue_is_valid(queue)) {
        return false;
    }

    if(item == NULL) {
        return false; 
    }

    if(rtos_queue_is_full(queue)){
        return false; 
    }

    destination = &queue->storage[queue->tail * queue->item_size];
    memcpy(destination, item, queue->item_size);
    queue->tail = (queue->tail + 1u) % queue->capacity;
    queue->count++;

    return true;
}

bool rtos_queue_receive(rtos_queue_t *queue, void *item)
{
    const uint8_t *source;

    if (!rtos_queue_is_valid(queue)) {
        return false;
    }

    if (item == NULL) {
        return false;
    }

    if (rtos_queue_is_empty(queue)) {
        return false;
    }

    source = &queue->storage[queue->head * queue->item_size];

    memcpy(item, source, queue->item_size);

    queue->head = (queue->head + 1u) % queue->capacity;
    queue->count--;

    return true;
}

bool rtos_queue_is_empty(const rtos_queue_t *queue)
{
    /* Invalid queues are treated as empty so callers can safely fail closed
     * during host-side task simulation.
     */
    if(!rtos_queue_is_valid(queue)){
        return true; 
    }

    return queue->count == 0u; 
}

bool rtos_queue_is_full(const rtos_queue_t *queue)
{
    /* A full queue is a valid queue at capacity; invalid queues are unusable,
     * not full.
     */
    if(!rtos_queue_is_valid(queue)){
        return false; 
    }

    return queue->count == queue->capacity;
}

size_t rtos_queue_count(const rtos_queue_t *queue)
{
    /* Returning zero for invalid queues keeps test and telemetry code from
     * depending on undefined queue internals.
     */
    if(!rtos_queue_is_valid(queue)){
        return 0u; 
    }

    return queue->count;
}

bool rtos_mutex_init(rtos_mutex_t *mutex)
{
    /* The host mutex starts unlocked so task-step tests can explicitly verify
     * ownership transitions.
     */
    if(mutex == NULL) {
        return false; 
    }
    
    mutex->locked = false; 

    return true;
}

bool rtos_mutex_lock(rtos_mutex_t *mutex)
{
    /* Double-lock rejection exposes incorrect shared-state ownership in tests
     * before a real scheduler can hide the problem behind timing.
     */
    if(mutex == NULL) {
        return false; 
    }

    if(mutex->locked){
        return false; 
    }

    mutex->locked = true;
    return true;
}

bool rtos_mutex_unlock(rtos_mutex_t *mutex)
{
    /* Unlocking an unlocked mutex is treated as an error because it indicates
     * a task released state it did not own.
     */
    if(mutex == NULL){
        return false; 
    }
    if(mutex->locked == false){
        return false; 
    }
    mutex->locked = false;

    return true;
}

bool rtos_event_group_init(rtos_event_group_t *event_group)
{
    /* Event flags start clear so stale MOVING or FAULTED bits cannot leak
     * across host demo runs.
     */
    if(event_group == NULL){
        return false; 
    }

    event_group->bits = 0u; 
    return true; 
}

rtos_event_bits_t rtos_event_group_set_bits(
    rtos_event_group_t *event_group,
    rtos_event_bits_t bits
)
{
    /* OR preserves existing system flags while publishing newly detected
     * conditions such as FAULTED or STOP_REQUESTED.
     */
    if(event_group == NULL){
        return 0u; 
    }

    event_group->bits |= bits;

    return event_group->bits;
}

rtos_event_bits_t rtos_event_group_clear_bits(
    rtos_event_group_t *event_group,
    rtos_event_bits_t bits
)
{
    /* Clearing only the requested bits lets one task acknowledge its flag
     * without erasing unrelated system state.
     */
    if(event_group == NULL){
        return 0u;
    }

    event_group->bits &= ~bits;
    return event_group->bits;
}

rtos_event_bits_t rtos_event_group_get_bits(
    const rtos_event_group_t *event_group
)
{
    /* A NULL event group reads as no active flags, which keeps host tests
     * deterministic for invalid inputs.
     */
    if(event_group == NULL){
        return 0u;
    }

    return event_group->bits;
}
