#include "def.h"

int main(int argc, char *argv[])
{
	/*
		argv[1] - ilość narciarzy
	*/
	time_t t;
	srand((unsigned)time(&t));

	int number_of_skiers = 0;
	if (argc >= 2) {
		number_of_skiers = atoi(argv[1]);
	} else {
		number_of_skiers = rand() % 50 + 10;
	}

	printf("Liczba narciarzy: %d\n", number_of_skiers);

	int *skiers_weights;
	skiers_weights = malloc(sizeof(int) * number_of_skiers);



	int min_skier_weight = SKIERS_MAX_WEIGHT;
	int sum_skiers_weights = 0;

	int i;

	int weights_arg_index = -1;
	for (i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "weights")) {
			weights_arg_index = i;
			break;
		}
	}

	for(i=0; i<number_of_skiers; i++) {
		if (weights_arg_index == -1)
			skiers_weights[i] = rand() % (SKIERS_MAX_WEIGHT - SKIERS_MIN_WEIGHT + 1) + SKIERS_MIN_WEIGHT;
		else
			skiers_weights[i] = atoi(argv[weights_arg_index + 1 + i]);
		sum_skiers_weights += skiers_weights[i];
		if (skiers_weights[i] < min_skier_weight) {
			min_skier_weight = skiers_weights[i];
		}
	}

	for(i=0; i<number_of_skiers; i++) {
		printf("Narciarz %d ma wage: %d\n", i, skiers_weights[i]);
	}
	printf("Min waga: %d\n", min_skier_weight);
	printf("Sum waga: %d\n", sum_skiers_weights);

	int min_perc_sum = 0.2 * sum_skiers_weights;
	int max_perc_sum = 0.4 * sum_skiers_weights;

	int first_lift_capacity;
	int second_lift_capacity;

	do {
		first_lift_capacity = rand() % (max_perc_sum - min_perc_sum) + min_perc_sum;
	} while (first_lift_capacity < min_skier_weight);

	do {
		second_lift_capacity = rand() % (max_perc_sum - min_perc_sum) + min_perc_sum;
	} while (second_lift_capacity < min_skier_weight);

	// read lifts capacities from console
	int lifts_arg_index = -1;
	for (i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "lifts")) {
			lifts_arg_index = i;
			break;
		}
	}
	if (lifts_arg_index != -1) {
		first_lift_capacity = atoi(argv[lifts_arg_index + 1]);
		second_lift_capacity = atoi(argv[lifts_arg_index + 2]);
	}

	printf("Pierwszy wyciag: %d\n", first_lift_capacity);
	printf("Drugi wyciag: %d\n", second_lift_capacity);


	int mytid;
	int tids[number_of_skiers];		/* slave task ids */
	char slave_name[NAMESIZE];
	int nproc, j, who;

	mytid = pvm_mytid();

	nproc=pvm_spawn(SLAVENAME, NULL, PvmTaskDefault, "", number_of_skiers, tids);

	for( i=0 ; i<nproc ; i++ )
	{
		pvm_initsend(PvmDataDefault);
		pvm_pkint(&mytid, 1, 1);
		pvm_pkint(&number_of_skiers, 1, 1);
		for (j=0; j<nproc; j++) {
			pvm_pkint(&tids[j], 1, 1);
		}
		for (j=0; j<nproc; j++) {
			pvm_pkint(&skiers_weights[j], 1, 1);
		}
		pvm_pkint(&first_lift_capacity, 1, 1);
		pvm_pkint(&second_lift_capacity, 1, 1);
		pvm_send(tids[i], MSG_MSTR);
	}

	int current_skier_weight;

	for( i=0 ; i<nproc ; i++ )
	{
		pvm_recv( -1, MSG_SLV );
		pvm_upkint(&who, 1, 1 );
		pvm_upkstr(slave_name );
		pvm_upkint(&current_skier_weight, 1, 1);
		printf("%d: %s weight %d\n",who, slave_name, current_skier_weight);
	}


	pvm_exit();
}
