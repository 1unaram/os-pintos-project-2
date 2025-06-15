#include "threads/thread.h"
#include "threads/synch.h"
#include "projects/crossroads/vehicle.h"
#include "projects/crossroads/map.h"
#include "projects/crossroads/blinker.h"
#include "projects/crossroads/ats.h"
#include "projects/crossroads/priority_sync.h"

extern struct position vehicle_path[4][4][12];

struct priority_lock blinker_lock;
struct priority_condition cond_all_can_enter;
struct vehicle_info* all_vehicles = NULL;
int cur_vehicle_count = 0;
int all_vehicle_count = 0;
int moved_vehicle_count = 0;

// For Debugging
#define gotoxy(y,x) printf("\033[%d;%dH", (y), (x))

// blinker.c
void notify_vehicle_finished() {
    p_lock_acquire(&blinker_lock);
    cur_vehicle_count--;
    p_lock_release(&blinker_lock);
}

void notify_vehicle_moved() {
    p_lock_acquire(&blinker_lock);

    moved_vehicle_count++;

    //debug
    // printf("$ Vehicle moved, total moved: %d / %d\n", moved_vehicle_count, cur_vehicle_count);

    p_cond_wait(&cond_all_can_enter, &blinker_lock);

    p_lock_release(&blinker_lock);
}

void init_blinker(struct blinker_info *blinkers, struct lock **map_locks, struct vehicle_info *vehicle_info) {
    p_lock_init(&blinker_lock);
    p_cond_init(&cond_all_can_enter);

    all_vehicles = vehicle_info;

    for (int i = 0; i < MAX_VEHICLE_COUNT; i++) {
        if (vehicle_info[i].id == '\0') break;
        cur_vehicle_count++;
    }
    all_vehicle_count = cur_vehicle_count;
}
void blinker_thread(void *aux UNUSED) {
    while (true) {
        p_lock_acquire(&blinker_lock);
        // 모든 차량이 moved/alive 상태인지 확인
        if (moved_vehicle_count >= cur_vehicle_count) {
            crossroads_step++;
            unitstep_changed();
            moved_vehicle_count = 0;
            p_cond_broadcast(&cond_all_can_enter, &blinker_lock);
        }
        p_lock_release(&blinker_lock);
        thread_yield(); // 또는 timer_msleep(1);
    }
}

void start_blinker() {
    thread_create("blinker", PRI_DEFAULT, blinker_thread, NULL);
}

bool is_conflict(struct vehicle_info *vi, int step) {


    // [1] 차량의 현재 위치와 다음 위치 가져오기
    int vi_from = vi->start - 'A';
    int vi_to = vi->dest - 'A';
    struct position vi_cur = vehicle_path[vi_from][vi_to][step];
    struct position vi_next = vehicle_path[vi_from][vi_to][step + 1];


    // 도착지 검사
    if ((vi_next.row == -1 && vi_next.col == -1) ||
        (vi_cur.row == -1 && vi_cur.col == -1)) {
        // 차량이 목적지에 도착했을 때
        return false;
    }

    // [2] 다른 차량들과 충돌 검사
    for (int i = 0; i < all_vehicle_count; i++) {
        struct vehicle_info *other = &all_vehicles[i];

        if (other == vi) continue;
        if (other->state != VEHICLE_STATUS_RUNNING) continue;

        int o_from = other->start - 'A';
        int o_to = other->dest - 'A';
        int o_step = other->step;
        struct position o_cur = vehicle_path[o_from][o_to][o_step];
        struct position o_next = vehicle_path[o_from][o_to][o_step + 1];

        // 1. 두 차량이 같은 위치로 이동하려는 경우
        if (vi_next.row == o_next.row && vi_next.col == o_next.col) {
            if (vi->type == VEHICL_TYPE_AMBULANCE && other->type != VEHICL_TYPE_AMBULANCE) continue; // vi는 진입, other는 대기

            return true;
        }

        // 2. 두 차량이 서로의 위치를 교차(swap)하려는 경우
        if (vi_next.row == o_cur.row && vi_next.col == o_cur.col &&
            o_next.row == vi_cur.row && o_next.col == vi_cur.col) return true;
    }
    return false;
}

bool request_permission_to_blinker(struct vehicle_info *vi, int step) {
    p_lock_acquire(&blinker_lock);

    // 앰뷸런스 골든타임 처리
    if (vi->type == VEHICL_TYPE_AMBULANCE) {
        int remain_time = vi->golden_time - crossroads_step;
        if (remain_time <= 0) {
            p_lock_release(&blinker_lock);
            return true;
        }
    }

    // 충돌 검사
    bool can_enter = !is_conflict(vi, step);

    p_lock_release(&blinker_lock);
    return can_enter;
}

