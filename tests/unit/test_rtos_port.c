#include "rtos_port.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define TEST_QUEUE_CAPACITY 3u
#define TEST_EVENT_MOVING (1u << 0u)
#define TEST_EVENT_FAULTED (1u << 1u)
#define TEST_EVENT_STOP_REQUESTED (1u << 2u)

typedef struct {
    int id;
    char label[8];
} test_message_t;

/* A tiny local assertion helper keeps these tests dependency-free, matching
 * the project's embedded-friendly test style.
 */
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

static int test_queue_init_validation(void)
{
    rtos_queue_t queue;
    int storage[TEST_QUEUE_CAPACITY];

    /* Queue setup is the boundary where caller-owned static memory becomes an
     * RTOS-style message buffer, so invalid configurations must fail early.
     */
    REQUIRE_TRUE(
        rtos_queue_init(&queue, storage, sizeof(storage[0]), TEST_QUEUE_CAPACITY),
        "valid queue init should succeed"
    );
    REQUIRE_TRUE(rtos_queue_is_empty(&queue), "new queue should start empty");
    REQUIRE_TRUE(rtos_queue_count(&queue) == 0u, "new queue count should be zero");

    REQUIRE_TRUE(
        !rtos_queue_init(NULL, storage, sizeof(storage[0]), TEST_QUEUE_CAPACITY),
        "NULL queue should fail init"
    );
    REQUIRE_TRUE(
        !rtos_queue_init(&queue, NULL, sizeof(storage[0]), TEST_QUEUE_CAPACITY),
        "NULL storage should fail init"
    );
    REQUIRE_TRUE(
        !rtos_queue_init(&queue, storage, 0u, TEST_QUEUE_CAPACITY),
        "zero item size should fail init"
    );
    REQUIRE_TRUE(
        !rtos_queue_init(&queue, storage, sizeof(storage[0]), 0u),
        "zero capacity should fail init"
    );
    return 0;
}

static int test_queue_send_receive_updates_count(void)
{
    rtos_queue_t queue;
    int storage[TEST_QUEUE_CAPACITY];
    int sent = 42;
    int received = 0;

    /* CommandTask and MotionTask will depend on send/receive preserving both
     * payload data and queue depth.
     */
    REQUIRE_TRUE(
        rtos_queue_init(&queue, storage, sizeof(storage[0]), TEST_QUEUE_CAPACITY),
        "queue init should succeed"
    );
    REQUIRE_TRUE(rtos_queue_send(&queue, &sent), "send should accept valid item");
    REQUIRE_TRUE(!rtos_queue_is_empty(&queue), "queue should not be empty after send");
    REQUIRE_TRUE(rtos_queue_count(&queue) == 1u, "queue count should increase after send");

    REQUIRE_TRUE(rtos_queue_receive(&queue, &received), "receive should return queued item");
    REQUIRE_TRUE(received == sent, "received item should match sent item");
    REQUIRE_TRUE(rtos_queue_is_empty(&queue), "queue should be empty after receive");
    REQUIRE_TRUE(rtos_queue_count(&queue) == 0u, "queue count should decrease after receive");
    return 0;
}

static int test_queue_rejects_invalid_send_receive(void)
{
    rtos_queue_t queue;
    int storage[1];
    int item = 7;
    int received = 0;

    /* Embedded queues are bounded; rejecting invalid access and overflow makes
     * task communication deterministic instead of silently corrupting state.
     */
    REQUIRE_TRUE(rtos_queue_init(&queue, storage, sizeof(storage[0]), 1u), "queue init should succeed");

    REQUIRE_TRUE(!rtos_queue_send(NULL, &item), "NULL queue should fail send");
    REQUIRE_TRUE(!rtos_queue_send(&queue, NULL), "NULL item should fail send");
    REQUIRE_TRUE(!rtos_queue_receive(NULL, &received), "NULL queue should fail receive");
    REQUIRE_TRUE(!rtos_queue_receive(&queue, NULL), "NULL output should fail receive");
    REQUIRE_TRUE(!rtos_queue_receive(&queue, &received), "empty queue should fail receive");

    REQUIRE_TRUE(rtos_queue_send(&queue, &item), "first send should fill capacity-one queue");
    REQUIRE_TRUE(rtos_queue_is_full(&queue), "queue should report full at capacity");
    REQUIRE_TRUE(!rtos_queue_send(&queue, &item), "full queue should reject extra send");
    return 0;
}

static int test_queue_preserves_fifo_after_wrap(void)
{
    rtos_queue_t queue;
    int storage[2];
    int first = 1;
    int second = 2;
    int third = 3;
    int received = 0;

    /* The circular buffer must keep FIFO order after wraparound because task
     * messages must execute in the same order they were received.
     */
    REQUIRE_TRUE(rtos_queue_init(&queue, storage, sizeof(storage[0]), 2u), "queue init should succeed");
    REQUIRE_TRUE(rtos_queue_send(&queue, &first), "first send should succeed");
    REQUIRE_TRUE(rtos_queue_send(&queue, &second), "second send should succeed");
    REQUIRE_TRUE(rtos_queue_receive(&queue, &received), "receive should free one slot");
    REQUIRE_TRUE(received == first, "first received value should preserve FIFO order");
    REQUIRE_TRUE(rtos_queue_send(&queue, &third), "send should wrap into freed slot");

    REQUIRE_TRUE(rtos_queue_receive(&queue, &received), "second receive should succeed");
    REQUIRE_TRUE(received == second, "second received value should preserve FIFO order");
    REQUIRE_TRUE(rtos_queue_receive(&queue, &received), "third receive should succeed");
    REQUIRE_TRUE(received == third, "wrapped value should be received last");
    return 0;
}

static int test_queue_copies_struct_payload(void)
{
    rtos_queue_t queue;
    test_message_t storage[1];
    test_message_t sent = { .id = 12, .label = "MOVE" };
    test_message_t received = { .id = 0, .label = "" };

    /* RTOS queues copy message bytes; this test prevents accidentally storing
     * caller pointers that could change before another task receives them.
     */
    REQUIRE_TRUE(rtos_queue_init(&queue, storage, sizeof(storage[0]), 1u), "queue init should succeed");
    REQUIRE_TRUE(rtos_queue_send(&queue, &sent), "struct payload should enqueue");

    sent.id = 99;
    strcpy(sent.label, "STOP");

    REQUIRE_TRUE(rtos_queue_receive(&queue, &received), "struct payload should dequeue");
    REQUIRE_TRUE(received.id == 12, "queue should copy item data instead of storing caller pointer");
    REQUIRE_TRUE(strcmp(received.label, "MOVE") == 0, "queue should preserve copied struct text");
    return 0;
}

static int test_mutex_lock_unlock_behavior(void)
{
    rtos_mutex_t mutex;

    /* The host mutex is intentionally strict so future task-step tests catch
     * double-lock and double-unlock mistakes around shared motion state.
     */
    REQUIRE_TRUE(!rtos_mutex_init(NULL), "NULL mutex should fail init");
    REQUIRE_TRUE(rtos_mutex_init(&mutex), "valid mutex init should succeed");
    REQUIRE_TRUE(rtos_mutex_lock(&mutex), "first lock should succeed");
    REQUIRE_TRUE(!rtos_mutex_lock(&mutex), "double lock should fail");
    REQUIRE_TRUE(rtos_mutex_unlock(&mutex), "unlock after lock should succeed");
    REQUIRE_TRUE(!rtos_mutex_unlock(&mutex), "double unlock should fail");
    REQUIRE_TRUE(rtos_mutex_lock(&mutex), "lock should succeed again after unlock");
    REQUIRE_TRUE(rtos_mutex_unlock(&mutex), "final unlock should succeed");
    REQUIRE_TRUE(!rtos_mutex_lock(NULL), "NULL mutex should fail lock");
    REQUIRE_TRUE(!rtos_mutex_unlock(NULL), "NULL mutex should fail unlock");
    return 0;
}

static int test_event_group_set_clear_get_behavior(void)
{
    rtos_event_group_t event_group;

    /* Event bits model shared task flags; clearing one flag must not erase
     * unrelated system conditions such as FAULTED or STOP_REQUESTED.
     */
    REQUIRE_TRUE(!rtos_event_group_init(NULL), "NULL event group should fail init");
    REQUIRE_TRUE(rtos_event_group_init(&event_group), "valid event group init should succeed");
    REQUIRE_TRUE(rtos_event_group_get_bits(&event_group) == 0u, "event group should start with no flags");

    REQUIRE_TRUE(
        rtos_event_group_set_bits(&event_group, TEST_EVENT_MOVING) == TEST_EVENT_MOVING,
        "setting MOVING should turn on MOVING bit"
    );
    REQUIRE_TRUE(
        rtos_event_group_set_bits(&event_group, TEST_EVENT_FAULTED) == (TEST_EVENT_MOVING | TEST_EVENT_FAULTED),
        "setting FAULTED should preserve MOVING bit"
    );
    REQUIRE_TRUE(
        rtos_event_group_clear_bits(&event_group, TEST_EVENT_MOVING) == TEST_EVENT_FAULTED,
        "clearing MOVING should leave FAULTED bit set"
    );
    REQUIRE_TRUE(
        rtos_event_group_set_bits(&event_group, TEST_EVENT_STOP_REQUESTED) ==
            (TEST_EVENT_FAULTED | TEST_EVENT_STOP_REQUESTED),
        "setting STOP_REQUESTED should preserve unrelated bits"
    );
    REQUIRE_TRUE(
        rtos_event_group_get_bits(&event_group) == (TEST_EVENT_FAULTED | TEST_EVENT_STOP_REQUESTED),
        "get bits should return current event flags"
    );
    REQUIRE_TRUE(rtos_event_group_set_bits(NULL, TEST_EVENT_MOVING) == 0u, "NULL set bits should return zero");
    REQUIRE_TRUE(rtos_event_group_clear_bits(NULL, TEST_EVENT_MOVING) == 0u, "NULL clear bits should return zero");
    REQUIRE_TRUE(rtos_event_group_get_bits(NULL) == 0u, "NULL get bits should return zero");
    return 0;
}

int main(void)
{
    int failures = 0;

    failures += test_queue_init_validation();
    failures += test_queue_send_receive_updates_count();
    failures += test_queue_rejects_invalid_send_receive();
    failures += test_queue_preserves_fifo_after_wrap();
    failures += test_queue_copies_struct_payload();
    failures += test_mutex_lock_unlock_behavior();
    failures += test_event_group_set_clear_get_behavior();

    if (failures == 0) {
        printf("rtos port tests passed\n");
    }

    return failures;
}
