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
int vehicle_count = 0;
int moved_vehicle_count = 0;


void notify_vehicle_moved() {
    p_lock_acquire(&blinker_lock);

    moved_vehicle_count++;

    //debug
    printf("$ Vehicle moved, total moved: %d / %d\n", moved_vehicle_count, vehicle_count);

    if (moved_vehicle_count >= vehicle_count) {
        crossroads_step++;
        unitstep_changed();
        moved_vehicle_count = 0;
        p_cond_broadcast(&cond_all_can_enter, &blinker_lock);
    }

    p_lock_release(&blinker_lock);
}

void init_blinker(struct blinker_info *blinkers, struct lock **map_locks, struct vehicle_info *vehicle_info) {
    p_lock_init(&blinker_lock);
    p_cond_init(&cond_all_can_enter);

    all_vehicles = vehicle_info;

    for (int i = 0; i < MAX_VEHICLE_COUNT; i++) {
        if (vehicle_info[i].id == '\0') break;
        vehicle_count++;
    }
}

void start_blinker() {

}

bool is_conflict(struct vehicle_info *vi, int step) {

    // [1] 차량의 현재 위치와 다음 위치 가져오기
    int vi_from = vi->start - 'A';
    int vi_to = vi->dest - 'A';
    struct position vi_cur = vehicle_path[vi_from][vi_to][step];
    struct position vi_next = vehicle_path[vi_from][vi_to][step + 1];

    // [2] 다른 차량들과 충돌 검사
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

        // 3. 두 차량의 경로가 겹치는 경우 (동일 경로 상에 동시에 존재)
        if (vi_next.row == o_cur.row && vi_next.col == o_cur.col) {
            return true;
        }
        if (vi_cur.row == o_next.row && vi_cur.col == o_next.col) {
            return true;
        }
    }
    return false;
}

void request_permission_to_blinker(struct vehicle_info *vi, int step) {
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
        if (!is_conflict(vi, step)) break;

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

