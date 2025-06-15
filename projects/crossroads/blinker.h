#ifndef __PROJECTS_PROJECT2_BLINKER_H__
#define __PROJECTS_PROJECT2_BLINKER_H__

/** you can change the number of blinkers */
#define NUM_BLINKER 4
#define MAX_VEHICLE_COUNT 26

struct blinker_info {
    struct lock **map_locks;
    struct vehicle_info *vehicles;
};

void init_blinker(struct blinker_info* blinkers, struct lock **map_locks, struct vehicle_info * vehicle_info);
void start_blinker();

bool request_permission_to_blinker(struct vehicle_info *vi, int step);
void notify_vehicle_moved();
void notify_vehicle_finished();
void set_total_vehicle_count(int count);

extern struct priority_lock blinker_lock;
extern struct priority_condition cond_all_can_enter;

#endif /* __PROJECTS_PROJECT2_BLINKER_H__ */
