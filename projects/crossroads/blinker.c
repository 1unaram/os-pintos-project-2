#include "threads/thread.h"
#include "threads/synch.h"
#include "projects/crossroads/vehicle.h"
#include "projects/crossroads/map.h"
#include "projects/crossroads/blinker.h"
#include "projects/crossroads/ats.h"
#include "projects/crossroads/priority_sync.h"

extern struct position vehicle_path[4][4][12];

static struct priority_lock blinker_lock;
static struct priority_condition cond_all_can_enter;
static bool vehicle_active[MAX_VEHICLE_COUNT];
static struct vehicle_info* all_vehicles = NULL;
static int vehicle_count = 0;
static int moved_vehicle_count = 0;
static int total_vehicle_count = 0;

void set_total_vehicle_count(int count) {
    p_lock_acquire(&blinker_lock);
    total_vehicle_count = count;
    p_lock_release(&blinker_lock);
}


void notify_vehicle_moved() {
    p_lock_acquire(&blinker_lock);

    moved_vehicle_count++;

    if (moved_vehicle_count >= total_vehicle_count) {
        crossroads_step++;
        unitstep_changed();
        moved_vehicle_count = 0;
        p_cond_broadcast(&cond_all_can_enter, &blinker_lock);
    }

    p_lock_release(&blinker_lock);
}

void init_blinker(struct blinker_info* blinkers, struct lock **map_locks, struct vehicle_info * vehicle_info) {
    p_lock_init(&blinker_lock);
    p_cond_init(&cond_all_can_enter);
    all_vehicles = vehicle_info;
    vehicle_count = 0;
    while (vehicle_count < MAX_VEHICLE_COUNT) {
        vehicle_active[vehicle_count] = false;
        vehicle_count++;
    }
}

void start_blinker() {
    set_total_vehicle_count(vehicle_count);
}

enum direction_type { UTURN = 0, TURN_RIGHT = 1, STRAIGHT = 2, TURN_LEFT = 3 };

enum direction_type get_direction_type(int from, int to) {
    return (to - from + 4) % 4;
}

bool is_conflict(struct vehicle_info *vi) {

    // debug
    printf("~~~ Checking conflict for vehicle %c at step %d\n", vi->id, crossroads_step);

    int vi_from = vi->start - 'A';
    int vi_to = vi->dest - 'A';
    struct position vi_cur = vehicle_path[vi_from][vi_to][vi->step];
    struct position vi_next = vehicle_path[vi_from][vi_to][vi->step + 1];

    // 모든 차량에 대해 충돌 검사
    for (int i = 0; i < vehicle_count; i++) {
        struct vehicle_info *other = &all_vehicles[i];
        if (other == vi) continue;
        if (other->state != VEHICLE_STATUS_RUNNING) continue;

        int o_from = other->start - 'A';
        int o_to = other->dest - 'A';
        int o_step = other->step;
        struct position o_cur = vehicle_path[o_from][o_to][o_step];
        struct position o_next = vehicle_path[o_from][o_to][o_step + 1];

        // 1. 두 차량이 같은 위치로 이동하려는 경우 (head-on collision)
        if (vi_next.row == o_next.row && vi_next.col == o_next.col) {
            if (vi->type == VEHICL_TYPE_AMBULANCE && other->type != VEHICL_TYPE_AMBULANCE)
                continue; // vi는 진입, other는 대기
            return true;
        }

        // 2. 두 차량이 서로의 위치를 교차(swap)하려는 경우
        if (vi_next.row == o_cur.row && vi_next.col == o_cur.col &&
            o_next.row == vi_cur.row && o_next.col == vi_cur.col)
            return true;
    }
    return false;
}

void blinker_request_permission(struct vehicle_info *vi, int step) {
    p_lock_acquire(&blinker_lock);

    // debug
    printf("~~ Vehicle %c requesting permission at step %d\n", vi->id, step);

    while (true) {
        // 앰뷸런스 골든 타임 처리
        if (vi->type == VEHICL_TYPE_AMBULANCE) {
            int remain_time = vi->golden_time - crossroads_step;
            if (remain_time <= 0) break;
        }

        // 충돌이 없으면 진입 허가
        if (!is_conflict(vi)) break;

        // 충돌이 있으면 조건 변수에서 대기
        p_cond_wait(&cond_all_can_enter, &blinker_lock);
    }

    // 진입 허가가 나면 차량 활성화 상태를 업데이트
    // for (int i = 0; i < vehicle_count; i++) {
    //     if (&all_vehicles[i] == vi) {
    //         vehicle_active[i] = true;
    //         break;
    //     }
    // }

    p_lock_release(&blinker_lock);
}

