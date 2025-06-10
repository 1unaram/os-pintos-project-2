#include "projects/crossroads/priority_sync.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include <stdlib.h>

// 우선순위 비교 함수 정의
bool thread_cmp_priority(const struct list_elem *a,
                         const struct list_elem *b,
                         void *aux) {
    const struct thread *t_a = list_entry(a, struct thread, elem);
    const struct thread *t_b = list_entry(b, struct thread, elem);
    return t_a->priority > t_b->priority;
}

/* --- Priority Semaphore --- */
void p_sema_init(struct priority_semaphore *sema, unsigned value) {
    sema->value = value;
    list_init(&sema->waiters);
}

void p_sema_down(struct priority_semaphore *sema) {
    enum intr_level old_level = intr_disable();
    struct thread *cur = thread_current();

    while (sema->value == 0) {
        list_insert_ordered(&sema->waiters, &cur->elem,
            (list_less_func *) &thread_cmp_priority, NULL);
        thread_block();
    }
    sema->value--;
    intr_set_level(old_level);
}

void p_sema_up(struct priority_semaphore *sema) {
    enum intr_level old_level = intr_disable();

    if (!list_empty(&sema->waiters)) {
        list_sort(&sema->waiters, (list_less_func *) &thread_cmp_priority, NULL);
        struct thread *t = list_entry(list_pop_front(&sema->waiters), struct thread, elem);
        thread_unblock(t);
    }
    sema->value++;
    intr_set_level(old_level);
}

/* --- Priority Lock --- */
void p_lock_init(struct priority_lock *lock) {
    lock->holder = NULL;
    p_sema_init(&lock->sema, 1);
}

void p_lock_acquire(struct priority_lock *lock) {
    ASSERT(lock != NULL);
    ASSERT(!p_lock_held_by_current_thread(lock));

    p_sema_down(&lock->sema);
    lock->holder = thread_current();
}

bool p_lock_try_acquire(struct priority_lock *lock) {
    ASSERT(lock != NULL);
    ASSERT(!p_lock_held_by_current_thread(lock));

    enum intr_level old_level = intr_disable();
    bool success = false;
    if (lock->sema.value > 0) {
        lock->sema.value--;
        lock->holder = thread_current();
        success = true;
    }
    intr_set_level(old_level);
    return success;
}

void p_lock_release(struct priority_lock *lock) {
    ASSERT(lock != NULL);
    ASSERT(p_lock_held_by_current_thread(lock));

    lock->holder = NULL;
    p_sema_up(&lock->sema);
}

bool p_lock_held_by_current_thread(const struct priority_lock *lock) {
    return lock->holder == thread_current();
}

/* --- Priority Condition --- */
struct semaphore_elem {
    struct list_elem elem;
    struct priority_semaphore sema;
};

void p_cond_init(struct priority_condition *cond) {
    list_init(&cond->waiters);
}

void p_cond_wait(struct priority_condition *cond, struct priority_lock *lock) {
    struct semaphore_elem *waiter = malloc(sizeof(struct semaphore_elem));
    p_sema_init(&waiter->sema, 0);
    list_insert_ordered(&cond->waiters, &waiter->elem,
        (list_less_func *) &thread_cmp_priority, NULL);

    p_lock_release(lock);
    p_sema_down(&waiter->sema);
    p_lock_acquire(lock);

    free(waiter);
}

void p_cond_signal(struct priority_condition *cond, struct priority_lock *lock) {
    if (!list_empty(&cond->waiters)) {
        list_sort(&cond->waiters, (list_less_func *) &thread_cmp_priority, NULL);
        struct semaphore_elem *waiter = list_entry(list_pop_front(&cond->waiters), struct semaphore_elem, elem);
        p_sema_up(&waiter->sema);
    }
}

void p_cond_broadcast(struct priority_condition *cond, struct priority_lock *lock) {
    while (!list_empty(&cond->waiters)) {
        p_cond_signal(cond, lock);
    }
}
