
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "threads/thread.h"
#include "threads/synch.h"
#include "projects/crossroads/vehicle.h"
#include "projects/crossroads/map.h"
#include "projects/crossroads/ats.h"
#include "projects/crossroads/blinker.h"

// For Debugging
#define gotoxy(y,x) printf("\033[%d;%dH", (y), (x))

/* path. A:0 B:1 C:2 D:3 */
const struct position vehicle_path[4][4][12] = {
	/* from A */ {
		/* to A */
		{{4,0},{4,1},{4,2},{4,3},{4,4},{3,4},{2,4},{2,3},{2,2},{2,1},{2,0},{-1,-1},},
		/* to B */
		{{4,0},{4,1},{4,2},{5,2},{6,2},{-1,-1},},
		/* to C */
		{{4,0},{4,1},{4,2},{4,3},{4,4},{4,5},{4,6},{-1,-1},},
		/* to D */
		{{4,0},{4,1},{4,2},{4,3},{4,4},{3,4},{2,4},{1,4},{0,4},{-1,-1},}
	},
	/* from B */ {
		/* to A */
		{{6,4},{5,4},{4,4},{3,4},{2,4},{2,3},{2,2},{2,1},{2,0},{-1,-1},},
		/* to B */
		{{6,4},{5,4},{4,4},{3,4},{2,4},{2,3},{2,2},{3,2},{4,2},{5,2},{6,2},{-1,-1},},
		/* to C */
		{{6,4},{5,4},{4,4},{4,5},{4,6},{-1,-1},},
		/* to D */
		{{6,4},{5,4},{4,4},{3,4},{2,4},{1,4},{0,4},{-1,-1},}
	},
	/* from C */ {
		/* to A */
		{{2,6},{2,5},{2,4},{2,3},{2,2},{2,1},{2,0},{-1,-1},},
		/* to B */
		{{2,6},{2,5},{2,4},{2,3},{2,2},{3,2},{4,2},{5,2},{6,2},{-1,-1},},
		/* to C */
		{{2,6},{2,5},{2,4},{2,3},{2,2},{3,2},{4,2},{4,3},{4,4},{4,5},{4,6},{-1,-1},},
		/* to D */
		{{2,6},{2,5},{2,4},{1,4},{0,4},{-1,-1},}
	},
	/* from D */ {
		/* to A */
		{{0,2},{1,2},{2,2},{2,1},{2,0},{-1,-1},},
		/* to B */
		{{0,2},{1,2},{2,2},{3,2},{4,2},{5,2},{6,2},{-1,-1},},
		/* to C */
		{{0,2},{1,2},{2,2},{3,2},{4,2},{4,3},{4,4},{4,5},{4,6},{-1,-1},},
		/* to D */
		{{0,2},{1,2},{2,2},{3,2},{4,2},{4,3},{4,4},{3,4},{2,4},{1,4},{0,4},{-1,-1},}
	}
};

void parse_vehicles(struct vehicle_info *vehicle_info, char *input)
{

	// input example: "aAA:bBD:cCD:dDB:fAB5.12:gAC6.13"
	printf("Parsing vehicles from input: %s\n", input);

    int idx = 0;
	char *save_ptr;
    char *token = strtok_r(input, ":", &save_ptr);
    while (token && idx < 8) {
        char id, start, dest;
        int arrival = -1, golden_time = -1;
        int type = VEHICL_TYPE_NORMAL;

		// 앰뷸런스인지 확인 ('.' 포함)
        char *dot = strchr(token, '.');
        if (dot != NULL) {
            // 앰뷸런스: id start dest arrival_step.golden_time

            size_t len = dot - token; // '.' 이전까지 길이
            if (len >= 4) {
                id = token[0];
                start = token[1];
                dest = token[2];
                arrival = atoi(&token[3]);
                golden_time = atoi(dot + 1);
                type = VEHICL_TYPE_AMBULANCE;
            }
        } else {
            // 일반 차량: id start dest
            if (strlen(token) >= 3) {
                id = token[0];
                start = token[1];
                dest = token[2];
            }
        }

		vehicle_info[idx].id = id;
        vehicle_info[idx].start = start;
        vehicle_info[idx].dest = dest;
        vehicle_info[idx].type = type;
        vehicle_info[idx].arrival = arrival;
        vehicle_info[idx].golden_time = golden_time;
        vehicle_info[idx].state = VEHICLE_STATUS_READY;

        idx++;
        token = strtok_r(NULL, ":", &save_ptr);
	}

	// print parsed vehicles
	for (int i = 0; i < idx; i++) {
		printf("Vehicle %c: Start %c, Dest %c, Type %d, Arrival %d, Golden Time %d\n",
			vehicle_info[i].id, vehicle_info[i].start, vehicle_info[i].dest,
			vehicle_info[i].type, vehicle_info[i].arrival, vehicle_info[i].golden_time);

		printf("Position: (%d, %d)\n", vehicle_info[i].position.row, vehicle_info[i].position.col);
	}

}

static int is_position_outside(struct position pos)
{
	return (pos.row == -1 || pos.col == -1);
}

/* return 0:termination, 1:success, -1:fail */
static int try_move(int start, int dest, int step, struct vehicle_info *vi)
{
	struct position pos_cur, pos_next;

	pos_cur = vi->position;
	pos_next = vehicle_path[start][dest][step];

	// debug
	// printf("~~ Vehicle %c trying to move from (%d, %d) to (%d, %d)\n",
		// vi->id, pos_cur.row, pos_cur.col, pos_next.row, pos_next.col);

	// When vehicle arrives at the destination -> terminate
	if (vi->state == VEHICLE_STATUS_RUNNING) {
		if (is_position_outside(pos_next)) {
			vi->position.row = vi->position.col = -1;
			p_lock_release(&vi->map_locks[pos_cur.row][pos_cur.col]);
			return 0;
		}
	}

	// Lock next position
	p_lock_acquire(&vi->map_locks[pos_next.row][pos_next.col]);
	if (vi->state == VEHICLE_STATUS_READY) {
		vi->state = VEHICLE_STATUS_RUNNING;
	} else {
		p_lock_release(&vi->map_locks[pos_cur.row][pos_cur.col]);
	}

	// Update vehicle position
	vi->position = pos_next;

	return 1;
}

void init_on_mainthread(int thread_cnt){

}

// Vehicle main loop
void vehicle_loop(void *_vi)
{
	int res;
	int start, dest, step;
	struct vehicle_info *vi = _vi;

	start = vi->start - 'A';
	dest = vi->dest - 'A';
	step = 0;

	vi->position.row = vi->position.col = -1;
	vi->state = VEHICLE_STATUS_READY;

	while (1) {

		// [1] 앰뷸런스 도착 전까지 대기
		if (vi->type == VEHICL_TYPE_AMBULANCE && crossroads_step < vi->arrival) {
				// debug
				// printf("~ Vehicle %c is waiting for its arrival step: %d\n", vi->id, vi->arrival);

				notify_vehicle_moved();
				thread_yield(); // Wait for the next step
				continue;
		}

		// debug
		// printf("\n~ Vehicle %c is at step %d, start %c, dest %c / State: %d | pos : (%d, %d)\n", vi->id, step, vi->start, vi->dest, vi->state, vi->position.row, vi->position.col);

		// [2] 진입 허가 요청
		bool can_enter = false;
		if (vi->state == VEHICLE_STATUS_READY || vi->state == VEHICLE_STATUS_RUNNING) {
			can_enter = request_permission_to_blinker(vi, step);
			if (can_enter) {
				// printf("~~ Vehicle %c granted permission to enter at step %d\n", vi->id, step);
			} else {
				// printf("~~ Vehicle %c denied permission to enter at step %d\n", vi->id, step);
			}
		}

		int res = -1;
		if (can_enter) {
			res = try_move(start, dest, step, vi);
			// printf("~~ Vehicle %c try_move result: %d\n", vi->id, res);

			if (res == 1) {
				step++;
				vi->step++;
			} else if (res == 0) {
				notify_vehicle_moved();
				notify_vehicle_finished();
				break;
			}
		}

		// 이동 알림
		notify_vehicle_moved();

		if (can_enter && res == 1) {
			// printf("~~~ Complete: Vehicle %c moved to (%d, %d) at step %d\n", vi->id, vi->position.row, vi->position.col, step);
		}


	}

	/* status transition must happen before sema_up */
	// 종료 상태로 전환
	vi->state = VEHICLE_STATUS_FINISHED;
}
