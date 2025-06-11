#ifndef PROJECTS_CROSSROADS_PRIORITY_SYNC_H
#define PROJECTS_CROSSROADS_PRIORITY_SYNC_H

#include "threads/thread.h"
#include "threads/interrupt.h"
#include <list.h>

/* Priority-aware Semaphore */
struct priority_semaphore {
    unsigned value;
    struct list waiters;
};

void p_sema_init(struct priority_semaphore *sema, unsigned value);
void p_sema_down(struct priority_semaphore *sema);
void p_sema_up(struct priority_semaphore *sema);

/* Priority-aware Lock */
struct priority_lock {
    struct thread *holder;
    struct priority_semaphore sema;
};

void p_lock_init(struct priority_lock *lock);
void p_lock_acquire(struct priority_lock *lock);
bool p_lock_try_acquire(struct priority_lock *lock);
void p_lock_release(struct priority_lock *lock);
bool p_lock_held_by_current_thread(const struct priority_lock *lock);

/* Priority-aware Condition */
struct priority_condition {
    struct list waiters;
};

void p_cond_init(struct priority_condition *cond);
void p_cond_wait(struct priority_condition *cond, struct priority_lock *lock);
void p_cond_signal(struct priority_condition *cond, struct priority_lock *lock);
void p_cond_broadcast(struct priority_condition *cond, struct priority_lock *lock);

#endif
