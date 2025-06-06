
#include <stdio.h>
#include <string.h>

#include "threads/thread.h"
#include "threads/synch.h"
#include "projects/crossroads/vehicle.h"
#include "projects/crossroads/map.h"
#include "projects/crossroads/ats.h"

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
	}

	timer_msleep(10000); // Simulate some delay for parsing
}

static int is_position_outside(struct position pos)
{
	return (pos.row == -1 || pos.col == -1);
}

/* return 0:termination, 1:success, -1:fail */
static int try_move(int start, int dest, int step, struct vehicle_info *vi)
{
	struct position pos_cur, pos_next;

	pos_next = vehicle_path[start][dest][step];
	pos_cur = vi->position;

	if (vi->state == VEHICLE_STATUS_RUNNING) {
		/* check termination */
		if (is_position_outside(pos_next)) {
			/* actual move */
			vi->position.row = vi->position.col = -1;
			/* release previous */
			lock_release(&vi->map_locks[pos_cur.row][pos_cur.col]);
			return 0;
		}
	}

	/* lock next position */
	lock_acquire(&vi->map_locks[pos_next.row][pos_next.col]);
	if (vi->state == VEHICLE_STATUS_READY) {
		/* start this vehicle */
		vi->state = VEHICLE_STATUS_RUNNING;
	} else {
		/* release current position */
		lock_release(&vi->map_locks[pos_cur.row][pos_cur.col]);
	}
	/* update position */
	vi->position = pos_next;

	return 1;
}

void init_on_mainthread(int thread_cnt){
	/* Called once before spawning threads */
}

void vehicle_loop(void *_vi)
{
	int res;
	int start, dest, step;

	struct vehicle_info *vi = _vi;

	start = vi->start - 'A';
	dest = vi->dest - 'A';

	vi->position.row = vi->position.col = -1;
	vi->state = VEHICLE_STATUS_READY;

	step = 0;
	while (1) {
		/* vehicle main code */
		res = try_move(start, dest, step, vi);
		if (res == 1) {
			step++;
		}

		/* termination condition. */
		if (res == 0) {
			break;
		}

		/* unitstep change! */
		unitstep_changed();
	}

	/* status transition must happen before sema_up */
	vi->state = VEHICLE_STATUS_FINISHED;
}
