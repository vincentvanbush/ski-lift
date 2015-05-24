#include "def.h"

void get_initial_values(int*, int*, int**, int**, int*, int*);

main()
{
	int mytid;
	char slave_name[NAMESIZE];
	int mstrtid;
	int myind;
	int number_of_skiers;
	int *tids;
	int *skiers_weights;
	int first_lift_capacity;
	int second_lift_capacity;
	int i;

	mytid = pvm_mytid();
	gethostname(slave_name, NAMESIZE);

	get_initial_values(&mstrtid, &number_of_skiers, &tids, &skiers_weights, &first_lift_capacity, &second_lift_capacity);	
	i=0;
	while (tids[i] != mytid) {
		i++;
	}
	
	pvm_initsend(PvmDataDefault);
	pvm_pkint(&mytid, 1, 1);
	pvm_pkstr(slave_name);
	pvm_pkint(&skiers_weights[i], 1, 1);
	pvm_send(mstrtid, MSG_SLV);

	pvm_exit();

}

void get_initial_values (int *mstrtid, int *number_of_skiers, int **tids, int **skiers_weights, int *first_lift_capacity, int *second_lift_capacity) {
	int i;
	pvm_recv(-1, MSG_MSTR);
	pvm_upkint(mstrtid, 1, 1);
	pvm_upkint(number_of_skiers, 1, 1);
	*tids = malloc(sizeof(int) * *number_of_skiers);
	*skiers_weights = malloc(sizeof(int) * *number_of_skiers);
	for(i=0; i<*number_of_skiers; i++) {
		pvm_upkint(&(*tids)[i], 1, 1);
	}
	for(i=0; i<*number_of_skiers; i++) {
		pvm_upkint(&(*skiers_weights)[i], 1, 1);
	}
	pvm_upkint(first_lift_capacity, 1, 1);
	pvm_upkint(second_lift_capacity, 1, 1);
}
