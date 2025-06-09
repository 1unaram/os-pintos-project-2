#include "threads/synch.h"
#include "threads/thread.h"
#include "projects/crossroads/vehicle.h"

struct priority_lock {
    struct semaphore sema;
    struct list waiters;  // vehicle_info 리스트
};

void priority_lock_init(struct priority_lock *lock);
void priority_lock_acquire(struct priority_lock *lock, struct vehicle_info *vinfo);
void priority_lock_release(struct priority_lock *lock);
