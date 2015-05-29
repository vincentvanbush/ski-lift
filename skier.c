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

void diag_msg(int mstrtid, int mytid, char *str) {
	struct timeval current_time;
	time_t logTime = time(NULL);
	char *timeStr =  asctime(localtime(&logTime));
	gettimeofday(&current_time, NULL);
	timeStr[strlen(timeStr)-6] = '\0';
	timeStr = timeStr + 11;

	char message[200];
	sprintf(message, "(%d) %s.%zu: %s", mytid, timeStr, current_time.tv_usec, str);
	logEvent(mytid, message);

	pvm_initsend(PvmDataDefault);
	pvm_pkstr(message);
	pvm_send(mstrtid, MSG_DIAG);
}

void send_accept_msg(int dest_tid, struct state_info info) {
	int *sender_weight_ptr = g_hash_table_lookup(info.skiers_weights, &dest_tid);
	int old_free = *info.lift_free;
	*info.lift_free -= *sender_weight_ptr;
	*info.local_clock += 1;

	int tag = MSG_ACCEPT;
	pvm_initsend(PvmDataDefault);
	pvm_pkint(&tag, 1, 1);
	pvm_pkint(&info.mytid, 1, 1);
	pvm_pkint(info.local_clock, 1, 1);
	pvm_send(dest_tid, tag);

	char diag[200];
	sprintf(diag, "sent MSG_ACCEPT to %d [timestamp=%d], was %d now %d",
					dest_tid, *info.local_clock, old_free, *info.lift_free);
	diag_msg(info.mstrtid, info.mytid, diag);
	//
	// sprintf(diag, "Send ACCEPT to %d", dest_tid);
	// diag_msg(mstrtid, info.mytid, diag);
}

void mcast_accept_msg(struct state_info info) {
	char diag[200];
	// diag_msg(info.mstrtid, info.mytid, "*** entering mcast_accept_msg");
	int len = g_queue_get_length(info.waiting_req_q);
	// diag_msg(info.mstrtid, info.mytid, "*** 0");
	int *tids = malloc(len * sizeof(int));
	int i = 0;
	sprintf(diag, "*** %d processes awaiting accepts", g_queue_get_length(info.waiting_req_q));
	diag_msg(info.mstrtid, info.mytid, diag);
	while (!g_queue_is_empty(info.waiting_req_q)) {
		int *tid = g_queue_pop_head(info.waiting_req_q);
		int *sender_weight_ptr = g_hash_table_lookup(info.skiers_weights, tid);
		// if (sender_weight_ptr == NULL)
		// 	diag_msg(info.mstrtid, info.mytid, "*** ACHTUNG");

		*info.lift_free -= *sender_weight_ptr;
		tids[i] = *tid;
		i++;
		sprintf(diag, "*** mcast MSG_ACCEPT to %d [weight=%d]", *tid, *sender_weight_ptr);
		diag_msg(info.mstrtid, info.mytid, diag);
	}
	*info.local_clock += 1;

	int tag = MSG_ACCEPT;
	pvm_initsend(PvmDataDefault);
	pvm_pkint(&tag, 1, 1);
	pvm_pkint(&info.mytid, 1, 1);
	pvm_pkint(info.local_clock, 1, 1);
	pvm_mcast(tids, len, tag);

	sprintf(diag, "mcast MSG_ACCEPT to %d waiting processes [timestamp=%d]", len, *info.local_clock);
	diag_msg(info.mstrtid, info.mytid, diag);
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

	char diag[200];
	sprintf(diag, "bcast MSG_REQUEST [timestamp=%d, lift_number=%s]", *info.local_clock, stringify(*info.my_lift_number));
	diag_msg(info.mstrtid, info.mytid, diag);
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

	// Unallocate space on the lift
	int *my_weight_ptr = g_hash_table_lookup(info.skiers_weights, &info.mytid);
	int my_weight = *my_weight_ptr;
	int *lift_free = (*info.my_lift_number == LIFT_1 ? info.lift1_free : info.lift2_free);
	*lift_free += my_weight;

	char diag[200];
	sprintf(diag, "bcast MSG_RELEASE [timestamp=%d, lift_number=%s]", *info.local_clock, stringify(*info.my_lift_number));
	diag_msg(info.mstrtid, info.mytid, diag);
}

void handle_request(struct msg incoming_msg, struct state_info info) {
	// handle Lamport clocks
	char diag[200];
	update_lamport_recv(incoming_msg.timestamp, info.local_clock);
	// diag_msg(info.mstrtid, info.mytid, "-------------------------------------------");

	int *my_weight_ptr = g_hash_table_lookup(info.skiers_weights, &info.mytid);
	int my_weight = *my_weight_ptr;
	int *sender_weight_ptr = g_hash_table_lookup(info.skiers_weights, &incoming_msg.sender_tid);
	int sender_weight = *sender_weight_ptr;
	// diag_msg(info.mstrtid, info.mytid, "*********************************************");

	if (info.phase != PHASE_WAIT_ACCEPTS) {
		// Always send ACCEPT, don't care about priorities in those phases

		// sprintf(diag,"I'm just before invocing function to send accespt message");
		// diag_msg(info.mstrtid,info.mytid,diag);
		send_accept_msg(incoming_msg.sender_tid, info);
		// char *diag;
		// sprintf(diag, "Send ACCEPT to %d", incoming_msg.sender_tid);
		// diag_msg(mstrtid, info.mytid, diag);
	} else /*if (info.phase != PHASE_CRITICAL)*/ {
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
				int *tid = malloc(sizeof(int));
				memcpy(tid, &incoming_msg.sender_tid, sizeof(int));
				g_queue_push_tail(info.waiting_req_q, tid);
				int *pushed = g_queue_peek_tail(info.waiting_req_q);
				sprintf(diag, "*** pushed tid=%d", *pushed);
				diag_msg(info.mstrtid, info.mytid, diag);
			}
		}
	} /*else {
		int *tid = malloc(sizeof(int));
		memcpy(tid, &incoming_msg.sender_tid, sizeof(int));
		g_queue_push_tail(info.waiting_req_q, tid);
		int *pushed = g_queue_peek_tail(info.waiting_req_q);
		sprintf(diag, "*** pushed tid=%d", *pushed);
		diag_msg(info.mstrtid, info.mytid, diag);
	}*/
};

void handle_accept (struct msg incoming_msg, struct state_info info) {
	// diag_msg(info.mstrtid, info.mytid, "*** Entered handle_accept ***");
	char diag[200];

	update_lamport_recv(incoming_msg.timestamp, info.local_clock);

	int *my_weight_ptr = g_hash_table_lookup(info.skiers_weights, &info.mytid);
	int my_weight = *my_weight_ptr;
	int *sender_weight_ptr = g_hash_table_lookup(info.skiers_weights, &incoming_msg.sender_tid);
	int sender_weight = *sender_weight_ptr;

	// decrement the pending_accepts_sum by sender weight
	// diag_msg(info.mstrtid, info.mytid, "*** Trying to subtract from pending accepts sum ***");
	*info.pending_accepts_sum -= sender_weight;
	*info.accepts_received += 1;
	// diag_msg(info.mstrtid, info.mytid, "*** Subtracted ***");

	sprintf(diag, "*** my_lift_number=%s", stringify(*info.my_lift_number));
	diag_msg(info.mstrtid, info.mytid, diag);
	int *lift_free = (*info.my_lift_number == LIFT_1 ? info.lift1_free : info.lift2_free);

	sprintf(diag, "free space on lift %d, pending accepts sum %d, accepts received %d, my weight %d",
					*lift_free, *info.pending_accepts_sum, *info.accepts_received, my_weight);
	diag_msg(info.mstrtid, info.mytid, diag);

	if (*lift_free - *info.pending_accepts_sum >= my_weight) {
		sprintf(diag, "can enter lift now");
		*lift_free -= my_weight;
    *info.can_enter_lift = 1;
	} else if (g_hash_table_size(info.skiers_weights) - 1 == *info.accepts_received) {
		sprintf(diag, "n-1 accepts - can enter lift now");
		*lift_free -= my_weight;
		*info.can_enter_lift = 1;
	}	else {
		sprintf(diag, "not entering the lift yet");
		*info.can_enter_lift = 0;
	}
	diag_msg(info.mstrtid, info.mytid, diag);
};

void handle_release(struct msg incoming_msg, struct state_info info) {
	update_lamport_recv(incoming_msg.timestamp, info.local_clock);

	int *my_weight_ptr = g_hash_table_lookup(info.skiers_weights, &info.mytid);
	int my_weight = *my_weight_ptr;
	int *sender_weight_ptr = g_hash_table_lookup(info.skiers_weights, &incoming_msg.sender_tid);
	int sender_weight = *sender_weight_ptr;

	// free the space on a lift
	*info.lift_free += sender_weight;
	char diag[200];
	sprintf(diag, "released by %d: sender_weight=%d, lift_free=%d",
					incoming_msg.sender_tid, sender_weight, *info.lift_free);

	// if in the PHASE_WAIT_REQUEST phase, try to choose the lift again
	if (info.phase == PHASE_WAIT_REQUEST && *info.lift_free >= my_weight) {
		*info.my_lift_number = incoming_msg.lift_number;
	}
};

void handle_message(struct msg incoming_msg, struct state_info info) {
	char diag[200];
	sprintf(diag,"receive %s from %d [phase=%s, timestamp=%d, lift_number=%s]",
					stringify(incoming_msg.tag), incoming_msg.sender_tid, stringify(info.phase),
					incoming_msg.timestamp, stringify(incoming_msg.tag == MSG_ACCEPT ? -1 : incoming_msg.lift_number));
	diag_msg(info.mstrtid, info.mytid, diag);

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
	info->lift1_free = first_lift_free;
	info->lift2_free = second_lift_free;
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
	char diag[200];

	struct state_info info;

	mytid = pvm_mytid();
	srand(time(NULL) + mytid);
	gethostname(slave_name, NAMESIZE);
	pvm_joingroup(GROUP);

	get_initial_values(&mstrtid, &number_of_skiers, &tids, &skiers_weights, &first_lift_capacity, &second_lift_capacity);
	info.mstrtid = mstrtid;
	first_lift_free = first_lift_capacity;
	second_lift_free = second_lift_capacity;
	int *my_weight_ptr = g_hash_table_lookup(skiers_weights, &mytid);
	int my_weight = *my_weight_ptr;

	int all_skiers_weight = 0;
	for(i=0; i<number_of_skiers; i++) {
		int *lookup = g_hash_table_lookup(skiers_weights, &tids[i]);
		all_skiers_weight += *lookup;
	}
	all_skiers_weight -= my_weight;
	sprintf(diag, "sum of all other skiers' weights = %d", all_skiers_weight);
	diag_msg(mstrtid, mytid, diag);

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

	diag_msg(mstrtid, mytid, "entering main loop");
	while (1) {
  	int can_enter_lift = 0;
  	int chosen_lift = -1;
		int accepts_received = 0;
		info.accepts_received = &accepts_received;

		// random waiting - down to the hill
    // diag_msg(mstrtid, mytid, "entering PHASE_DOWNHILL");
		phase = PHASE_DOWNHILL;
		// char *msg_to_file = "First message";
		// char *phase_to_file = "PHASE_DOWNHILL";
		// logEvent(msg_to_file, phase_to_file, mytid);

		struct timeval timeout;
		random_timeout(&timeout, 3, 10);

		struct timeval start_time;
		gettimeofday(&start_time, NULL);

		struct timeval elapsed;
		elapsed.tv_sec = 0;
		elapsed.tv_usec = 0;

		sprintf(diag, "entered %s, time=%zu.%zu", stringify(phase), timeout.tv_sec, timeout.tv_usec);
		diag_msg(mstrtid, mytid, diag);
		while (1) {


			int bufid = pvm_trecv(-1, -1, &timeout);

			if (bufid) {
				// diag_msg(mstrtid, mytid, "Got message in PHASE_DOWNHILL");
				struct msg incoming_msg;
				unpack(&incoming_msg);

				int *sender_weight = g_hash_table_lookup(skiers_weights, &incoming_msg.sender_tid);

				// handle the message accordingly
        		prepare_info(&info, mytid, &local_clock, phase, &chosen_lift, &can_enter_lift,
                     &first_lift_free, &second_lift_free, incoming_msg,
										 waiting_req_q1, waiting_req_q2);
        		info.skiers_weights = skiers_weights;
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
		diag_msg(mstrtid, mytid, "");
		phase = PHASE_WAIT_REQUEST;

		// choose the lift (1 or 2)
		// determine if we can fit on the lift (based on our knowledge from the accepts we had sent)

		sprintf(diag, "entered PHASE_WAIT_REQUEST, weight=%d, LIFT_1=%d, LIFT_2=%d",my_weight,first_lift_free,second_lift_free);
		diag_msg(mstrtid, mytid, diag);

		if (my_weight > first_lift_free && my_weight > second_lift_free) {
			// no lift for us
			// wait for RELEASE messages until we can fit in the lift
			// meanwhile: handle all incoming messages, responding accordingly (ALWAYS respond with accepts)
			while (1) {
				sprintf(diag, "no space in lifts, weight=%d, LIFT_1=%d, LIFT_2=%d",my_weight,first_lift_free,second_lift_free);
				diag_msg(mstrtid, mytid, diag);

				int bufid = pvm_recv(-1, -1);
				struct msg incoming_msg;
				unpack(&incoming_msg);

				prepare_info(&info, mytid, &local_clock, phase, &chosen_lift, &can_enter_lift,
                     &first_lift_free, &second_lift_free, incoming_msg,
										 waiting_req_q1, waiting_req_q2);
				info.skiers_weights = skiers_weights;
        handle_message(incoming_msg, info);

				// now check if we should break the loop and go to the next phase!
				if (chosen_lift == LIFT_1 || chosen_lift == LIFT_2) {
					sprintf(diag, "weight=%d, LIFT_1=%d, LIFT_2=%d, choosing %s",
									my_weight, first_lift_free, second_lift_free, stringify(chosen_lift));
					diag_msg(mstrtid, mytid, diag);

					// broadcast a request with our chosen lift and go to (*)
					bcast_request_msg(info);
					break;
				}
			}

		}
		else { // if there is a fitting lift

			if (my_weight <= first_lift_free && my_weight > second_lift_free) { // can only go to first lift
				// diag_msg(mstrtid, mytid, "*** can only fit on LIFT_1");
				chosen_lift = LIFT_1;
			}

			else if (my_weight > first_lift_free && my_weight <= second_lift_free) { // can only go to second lift
				// diag_msg(mstrtid, mytid, "*** can only fit on LIFT_2");
				chosen_lift = LIFT_2;
			}

			else {	// can go to either lift - randomize
				// diag_msg(mstrtid, mytid, "*** choosing lift randomly");
				chosen_lift = (rand() % 2 == 1) ? LIFT_1 : LIFT_2;
			}

			///
			/// for debugging necessary values
			info.my_lift_number = &chosen_lift;
			info.mytid = mytid;
			info.local_clock = &local_clock;
			info.skiers_weights = skiers_weights;

			// broadcast a request with our chosen lift and go to (*)
			bcast_request_msg(info);
			// sprintf(diag,"So I am here just before broadcasting REQUEST and I choose lift %d and in info %d",chosen_lift, *info.my_lift_number);
			sprintf(diag, "weight=%d, LIFT_1=%d, LIFT_2=%d, choosing %s",
							my_weight, first_lift_free, second_lift_free, stringify(chosen_lift));
			diag_msg(mstrtid, mytid, diag);
		}

		sprintf(diag, "*** chosen_lift=%s", stringify(chosen_lift));

		// waiting for accepts (*)
    // wait for enough accepts or just as much as required to be sure that we can get in
    // meanwhile: handle all incoming messages, responding accordingly (not send back accept only to those with worse priority that can't fit together with us)
		// diag_msg(mstrtid, mytid, "entering PHASE_WAIT_ACCEPTS");
		phase = PHASE_WAIT_ACCEPTS;
		int pending = all_skiers_weight;
		info.pending_accepts_sum = &pending;

		sprintf(diag, "entered PHASE_WAIT_ACCEPTS, lift=%s", stringify(chosen_lift));
		diag_msg(mstrtid, mytid, diag);

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

		sprintf(diag, "entered PHASE_CRITICAL, lift=%s, time=%zu.%zu",
						stringify(chosen_lift), timeout.tv_sec, timeout.tv_usec);
		diag_msg(mstrtid, mytid, diag);

		while (1) {
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
		// diag_msg(mstrtid, mytid, "Broadcast RELEASE");
		info.mytid = mytid;
		info.local_clock = &local_clock;
		info.my_lift_number = &chosen_lift;
		bcast_release_msg(info);

		// send all the requests stored in the queue (mcast automatically clears it)
		mcast_accept_msg(info);
	}

	pvm_exit();
}
