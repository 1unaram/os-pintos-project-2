
void priority_sema_down(struct semaphore *sema) {
    enum intr_level old_level;
    ASSERT(sema != NULL);
    ASSERT(!intr_context());

    old_level = intr_disable();
    while (sema->value == 0) {
        struct thread *curr = thread_current();
        list_insert_ordered(&sema->waiters, &curr->elem, thread_priority_more, NULL);
        thread_block();
    }
    sema->value--;
    intr_set_level(old_level);
}

void priority_sema_up(struct semaphore *sema) {
    enum intr_level old_level;
    ASSERT(sema != NULL);

    old_level = intr_disable();

    if (!list_empty(&sema->waiters)) {
        // 우선순위 높은 스레드 먼저 깸
        list_sort(&sema->waiters, thread_priority_more, NULL);
        thread_unblock(list_entry(list_pop_front(&sema->waiters), struct thread, elem));
    }

    sema->value++;
    intr_set_level(old_level);
}
