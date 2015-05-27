#include <stdio.h>
#include <stdlib.h>
#include <pvm3.h>

#define SKIERS_MAX_WEIGHT 100
#define SKIERS_MIN_WEIGHT 50

#define GROUP "SystemOfADown"



#define SLAVENAME "skier"
#define SLAVENUM   10

#define NAMESIZE   64

#define MSG_MSTR 1
#define MSG_SLV  2

#define MSG_REQUEST 3
#define MSG_ACCEPT 4
#define MSG_RELEASE 5


#define PHASE_DOWNHILL 10
#define PHASE_WAIT_REQUEST 11
#define PHASE_WAIT_ACCEPTS 12
#define PHASE_CRITICAL 13
#define PHASE_UPHILL 13 // lel

#define LIFT_1 20
#define LIFT_2 21

struct msg {
	int tag;
	int sender_tid;
	int timestamp;
	int lift_number;
};

struct state_info {
	int mytid;
	int *local_clock;
	int *can_enter_lift;
	int phase;
	int *my_lift_number;
	void *waiting_req_q;
	void *skiers_weights;	// GHashTable*
	int *lift_free;
	int *pending_accepts_sum;
};

int timeval_subtract (result, x, y)
     struct timeval *result, *x, *y;
{
  /* Perform the carry for the later subtraction by updating y. */
  if (x->tv_usec < y->tv_usec) {
    int nsec = (y->tv_usec - x->tv_usec) / 1000000 + 1;
    y->tv_usec -= 1000000 * nsec;
    y->tv_sec += nsec;
  }
  if (x->tv_usec - y->tv_usec > 1000000) {
    int nsec = (x->tv_usec - y->tv_usec) / 1000000;
    y->tv_usec += 1000000 * nsec;
    y->tv_sec -= nsec;
  }

  /* Compute the time remaining to wait.
     tv_usec is certainly positive. */
  result->tv_sec = x->tv_sec - y->tv_sec;
  result->tv_usec = x->tv_usec - y->tv_usec;

  /* Return 1 if result is negative. */
  return x->tv_sec < y->tv_sec;
}
