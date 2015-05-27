#include "def.h"
#include <sys/time.h>
#include <glib.h>

void update_lamport_recv(int incoming_timestamp, int *local_clock) {
	if (incoming_timestamp > *local_clock) {
		*local_clock = incoming_timestamp;
	}
	*local_clock += 1;
}

void get_initial_values (int *mstrtid, int *number_of_skiers, int **tids, GHashTable **skiers_weights, int *first_lift_capacity, int *second_lift_capacity) {
	int i;
	pvm_recv(-1, MSG_MSTR);
	pvm_upkint(mstrtid, 1, 1);
	pvm_upkint(number_of_skiers, 1, 1);
	*tids = malloc(sizeof(int) * *number_of_skiers);

	for(i=0; i<*number_of_skiers; i++) {
		pvm_upkint(&((*tids)[i]), 1, 1);
	}

	int *temp_weights = malloc(sizeof(int) * *number_of_skiers);
	for(i=0; i<*number_of_skiers; i++) {
		pvm_upkint(&temp_weights[i], 1, 1);
	}

	for(i=0; i<*number_of_skiers; i++) {
		g_hash_table_insert(*skiers_weights, &(*tids)[i], &temp_weights[i]);
	}

	pvm_upkint(first_lift_capacity, 1, 1);
	pvm_upkint(second_lift_capacity, 1, 1);
}

void send_accept_msg(int dest_tid, struct state_info info) {
	int *sender_weight_ptr = g_hash_table_lookup(info.skiers_weights, &dest_tid);
	*info.lift_free -= *sender_weight_ptr;
	*info.local_clock += 1;

	int tag = MSG_ACCEPT;
	pvm_initsend(PvmDataDefault);
	pvm_pkint(&tag, 1, 1);
	pvm_pkint(&info.mytid, 1, 1);
	pvm_pkint(info.local_clock, 1, 1);
	pvm_send(dest_tid, tag);
}

void mcast_accept_msg(struct state_info info) {
	int len = g_queue_get_length(info.waiting_req_q);
	int *tids = malloc(len * sizeof(int));
	int i = 0;
	while (!g_queue_is_empty(info.waiting_req_q)) {
		int *tid = g_queue_pop_head(info.waiting_req_q);
		tids[i] = *tid;
		i++;
	}
	*info.local_clock += 1;

	int tag = MSG_ACCEPT;
	pvm_initsend(PvmDataDefault);
	pvm_pkint(&tag, 1, 1);
	pvm_pkint(&info.mytid, 1, 1);
	pvm_pkint(info.local_clock, 1, 1);
	pvm_mcast(tids, len, tag);
}

void bcast_request_msg(struct state_info info) {
	int tag = MSG_REQUEST;
	*info.local_clock += 1;
	pvm_initsend(PvmDataDefault);
	pvm_pkint(&tag, 1, 1);
	pvm_pkint(&info.mytid, 1, 1);
	pvm_pkint(info.local_clock, 1, 1);
	pvm_pkint(info.my_lift_number, 1, 1);
	pvm_bcast(GROUP, MSG_REQUEST);
}

void bcast_release_msg(struct state_info info) {
	int tag = MSG_RELEASE;
	*info.local_clock += 1;
	pvm_initsend(PvmDataDefault);
	pvm_pkint(&tag, 1, 1);
	pvm_pkint(&info.mytid, 1, 1);
	pvm_pkint(info.local_clock, 1, 1);
	pvm_pkint(info.my_lift_number, 1, 1);
	pvm_bcast(GROUP, MSG_RELEASE);
}

void handle_request(struct msg incoming_msg, struct state_info info) {
	// handle Lamport clocks
	update_lamport_recv(incoming_msg.timestamp, info.local_clock);

	int *my_weight_ptr = g_hash_table_lookup(info.skiers_weights, &info.mytid);
	int my_weight = *my_weight_ptr;
	int *sender_weight_ptr = g_hash_table_lookup(info.skiers_weights, &incoming_msg.sender_tid);
	int sender_weight = *sender_weight_ptr;

	if (info.phase != PHASE_WAIT_ACCEPTS) {
		// Always send ACCEPT, don't care about priorities in those phases

		send_accept_msg(incoming_msg.sender_tid, info);
	} else {
		// Send ACCEPT only if:
		// a) my priority is worse than the sender's priority,
		// b) or my priority is better but we both can fit
		// c) or I want the other lift
		if (*info.my_lift_number != incoming_msg.lift_number) { // I want the other lift
			send_accept_msg(incoming_msg.sender_tid, info);
		}
		// my priority is worse
		else if ( (*info.local_clock > incoming_msg.timestamp) || (*info.local_clock == incoming_msg.timestamp && info.mytid > incoming_msg.sender_tid) ) {
			send_accept_msg(incoming_msg.sender_tid, info);
		}
		// my priority is better
		else {
			if (my_weight + sender_weight <= *info.lift_free) {
				send_accept_msg(incoming_msg.sender_tid, info);
			}
			else { // add the sender to the queue of waiting requests for the lift
				g_queue_push_tail(info.waiting_req_q, &incoming_msg.sender_tid);
			}
		}
	}
};

void handle_accept (struct msg incoming_msg, struct state_info info) {
	update_lamport_recv(incoming_msg.timestamp, info.local_clock);

	int *my_weight_ptr = g_hash_table_lookup(info.skiers_weights, &info.mytid);
	int my_weight = *my_weight_ptr;
	int *sender_weight_ptr = g_hash_table_lookup(info.skiers_weights, &incoming_msg.sender_tid);
	int sender_weight = *sender_weight_ptr;

	// decrement the pending_accepts_sum by sender weight
	*info.pending_accepts_sum -= sender_weight;

	if (*info.lift_free - *info.pending_accepts_sum >= my_weight) {
		// we can go on the lift
		*info.lift_free -= my_weight;
    *info.can_enter_lift = 1;
	}
	else *info.can_enter_lift = 0;
};

void handle_release(struct msg incoming_msg, struct state_info info) {
	update_lamport_recv(incoming_msg.timestamp, info.local_clock);

	int *my_weight_ptr = g_hash_table_lookup(info.skiers_weights, &info.mytid);
	int my_weight = *my_weight_ptr;
	int *sender_weight_ptr = g_hash_table_lookup(info.skiers_weights, &incoming_msg.sender_tid);
	int sender_weight = *sender_weight_ptr;

	// free the space on a lift
	*info.lift_free += sender_weight;

	// if in the PHASE_WAIT_REQUEST phase, try to choose the lift again
	if (*info.lift_free >= my_weight) {
		*info.my_lift_number = incoming_msg.lift_number;
	}
};

void handle_message(struct msg incoming_msg, struct state_info info) {
  switch (incoming_msg.tag) {
    case MSG_REQUEST:	handle_request(incoming_msg, info); break;
    case MSG_ACCEPT:	handle_accept (incoming_msg, info); break;
    case MSG_RELEASE:	handle_release(incoming_msg, info); break;
  }
}

void random_timeout(struct timeval *ret, int min_sec, int max_sec) {
	ret->tv_sec = min_sec + rand() % (max_sec - min_sec);
	ret->tv_usec = 0;
}

void unpack(struct msg *incoming_msg) {
	pvm_upkint(&incoming_msg->tag, 1, 1);
	pvm_upkint(&incoming_msg->sender_tid, 1, 1);
	pvm_upkint(&incoming_msg->timestamp, 1, 1);
	if (incoming_msg->tag != MSG_ACCEPT)
		pvm_upkint(&incoming_msg->lift_number, 1, 1);
}

void prepare_info(struct state_info *info, int mytid, int *local_clock, int phase,
                  int *chosen_lift, int *can_enter_lift, int *first_lift_free,
                  int *second_lift_free, struct msg incoming_msg, GQueue *waiting_req_q1, GQueue *waiting_req_q2) {
  info->mytid = mytid;
  info->local_clock = local_clock;
  info->phase = phase;
  info->my_lift_number = chosen_lift;
  info->can_enter_lift = can_enter_lift;
  if (incoming_msg.lift_number == LIFT_1) {
    info->lift_free = first_lift_free;
		info->waiting_req_q = waiting_req_q1;
  }
  else {
    info->lift_free = second_lift_free;
		info->waiting_req_q = waiting_req_q2;
  }
}

main()
{
	GHashTable* skiers_weights = g_hash_table_new(g_str_hash, g_str_equal);
	GQueue *waiting_req_q1 = g_queue_new();
	GQueue *waiting_req_q2 = g_queue_new();

	int first_lift_free, second_lift_free;

	int mytid, mstrtid, myind, number_of_skiers, *tids, first_lift_capacity,
			second_lift_capacity, i, phase, local_clock;
	time_t t;
	char slave_name[NAMESIZE];
	srand((unsigned)time(&t));

	struct state_info info;

	mytid = pvm_mytid();
	gethostname(slave_name, NAMESIZE);
	pvm_joingroup(GROUP);

	get_initial_values(&mstrtid, &number_of_skiers, &tids, &skiers_weights, &first_lift_capacity, &second_lift_capacity);

	first_lift_free = first_lift_capacity;
	second_lift_free = second_lift_capacity;
	int *my_weight_ptr = g_hash_table_lookup(skiers_weights, &mytid);
	int my_weight = *my_weight_ptr;

	int all_skiers_weight;
	for(i=0; i<number_of_skiers; i++) {
		int *lookup = g_hash_table_lookup(skiers_weights, &tids[i]);
		all_skiers_weight += *lookup;
	}
	all_skiers_weight -= my_weight;

	i=0;
	while (tids[i] != mytid) {
		i++;
	}

	pvm_initsend(PvmDataDefault);
	pvm_pkint(&mytid, 1, 1);
	pvm_pkstr(slave_name);

	int *wat_to_send = g_hash_table_lookup(skiers_weights, &mytid);
	pvm_pkint(wat_to_send, 1, 1);
	pvm_send(mstrtid, MSG_SLV);


	// bariera

	local_clock = 0;
	pvm_barrier(GROUP, number_of_skiers);


	int time_to_wait;
	// main loop

	while (1) {
    int can_enter_lift = 0;
    int chosen_lift = -1;

		// random waiting - down to the hill
		phase = PHASE_DOWNHILL;

		struct timeval timeout;
		random_timeout(&timeout, 3, 10);

		struct timeval start_time;
		gettimeofday(&start_time, NULL);

		struct timeval elapsed;
		elapsed.tv_sec = 0;
		elapsed.tv_usec = 0;

		while (1) {
			int bufid = pvm_trecv(-1, -1, &timeout);

			if (bufid) {
				struct msg incoming_msg;
				unpack(&incoming_msg);

				int *sender_weight = g_hash_table_lookup(skiers_weights, &incoming_msg.sender_tid);

				// handle the message accordingly
        prepare_info(&info, mytid, &local_clock, phase, &chosen_lift, &can_enter_lift,
                     &first_lift_free, &second_lift_free, incoming_msg,
										 waiting_req_q1, waiting_req_q2);
        handle_message(incoming_msg, info);
			}


			// break the loop if time elapsed is more than the timeout, else wait for another message
			struct timeval current_time;
			gettimeofday(&current_time, NULL);

			timeval_subtract(&elapsed, &current_time, &start_time);
			if (elapsed.tv_sec * 1000000 + elapsed.tv_usec >= timeout.tv_sec * 1000000 + timeout.tv_usec)
				break;
			else
				timeval_subtract(&timeout, &timeout, &elapsed);
		}

		// want to go up
		phase = PHASE_WAIT_REQUEST;

		// choose the lift (1 or 2)
		// determine if we can fit on the lift (based on our knowledge from the accepts we had sent)

		if (my_weight > first_lift_free && my_weight > second_lift_free) {
			// no lift for us
			// wait for RELEASE messages until we can fit in the lift
			// meanwhile: handle all incoming messages, responding accordingly (ALWAYS respond with accepts)
			while (1) {
				int bufid = pvm_recv(-1, -1);
				struct msg incoming_msg;
				unpack(&incoming_msg);

				prepare_info(&info, mytid, &local_clock, phase, &chosen_lift, &can_enter_lift,
                     &first_lift_free, &second_lift_free, incoming_msg,
										 waiting_req_q1, waiting_req_q2);
        handle_message(incoming_msg, info);

				// now check if we should break the loop and go to the next phase!
				if (chosen_lift == LIFT_1 || chosen_lift == LIFT_2) {
					// broadcast a request with our chosen lift and go to (*)
					bcast_request_msg(info);
					break;
				}
			}

		}
		else { // if there is a fitting lift
			if (my_weight >= first_lift_free && my_weight < second_lift_free) // can only go to first lift
				chosen_lift = LIFT_1;
			else if (my_weight < first_lift_free && my_weight >= second_lift_free) // can only go to second lift
				chosen_lift = LIFT_2;
			else // can go to either lift - randomize
				chosen_lift = (rand() % 2 == 1) ? LIFT_1 : LIFT_2;

			// broadcast a request with our chosen lift and go to (*)
			bcast_request_msg(info);
		}

		// waiting for accepts (*)
    // wait for enough accepts or just as much as required to be sure that we can get in
    // meanwhile: handle all incoming messages, responding accordingly (not send back accept only to those with worse priority that can't fit together with us)
		phase = PHASE_WAIT_ACCEPTS;
		int pending = all_skiers_weight;
		info.pending_accepts_sum = &pending;
		while (1) {
			int bufid = pvm_recv(-1, -1);
			struct msg incoming_msg;
			unpack(&incoming_msg);

			prepare_info(&info, mytid, &local_clock, phase, &chosen_lift, &can_enter_lift,
									&first_lift_free, &second_lift_free, incoming_msg,
									waiting_req_q1, waiting_req_q2);
      handle_message(incoming_msg, info);

			// check if there are enough amount of acceptes - can break the loop and go to the critical section
			if (can_enter_lift) {
				break;
			}
		}

		// random waiting up to the hill - critical section
		phase = PHASE_CRITICAL;

		random_timeout(&timeout, 3, 10);

		gettimeofday(&start_time, NULL);

		elapsed.tv_sec = 0;
		elapsed.tv_usec = 0;

		while (1) {
			// handle all incoming messages, responding accordingly (ALWAYS respond with accepts)
			int bufid = pvm_trecv(-1, -1, &timeout);

			if (bufid) {
				// unpack the received message
				struct msg incoming_msg;
				// int msgtag = -1, sender_tid = -1, lift_number = -1, timestamp = -1;
				unpack(&incoming_msg);

				int *sender_weight = g_hash_table_lookup(skiers_weights, &incoming_msg.sender_tid);

				// handle the message accordingly
				prepare_info(&info, mytid, &local_clock, phase, &chosen_lift, &can_enter_lift,
                     &first_lift_free, &second_lift_free, incoming_msg,
										 waiting_req_q1, waiting_req_q2);
        handle_message(incoming_msg, info);
			}

			// break the loop if time elapsed is more than the timeout, else wait for another message
			struct timeval current_time;
			gettimeofday(&current_time, NULL);

			timeval_subtract(&elapsed, &current_time, &start_time);
			if (elapsed.tv_sec * 1000000 + elapsed.tv_usec >= timeout.tv_sec * 1000000 + timeout.tv_usec)
				break;
			else
				timeval_subtract(&timeout, &timeout, &elapsed);
		}

		// release - broadcast the RELEASE message to all others
		info.mytid = mytid;
		info.local_clock = &local_clock;
		info.my_lift_number = &chosen_lift;
		bcast_release_msg(info);

		// send all the requests stored in the queue (mcast automatically clears it)
		mcast_accept_msg(info);
	}

	pvm_exit();
}
