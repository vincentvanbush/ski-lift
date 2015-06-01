import re
import sys

f = open(sys.argv[1], 'r')
critical = open('critical_log', 'w')
for line in f:
	if re.search('Liczba narciarzy', line) != None:
		critical.write(line)
	if re.search('\A[0-9]+:.*weight', line) != None:
		critical.write(line)	
	if re.search('\APierwszy|\ADrugi', line) != None:
		critical.write(line)
	if re.search('entered PHASE_CRITICAL',line) != None:
		critical.write(line)
	if re.search('bcast MSG_RELEASE', line) != None:
		critical.write(line)

critical.close()

processed = open('processed_log', 'w')
critical = open('critical_log', 'r')

number_of_skiers = None
skiers_weights = {}
lifts_capacity = []
first_lift_free = 0
second_lift_free = 0
for line in critical:
	if re.search('Liczba narciarzy', line) != None:
		arr = re.split('\W+', line)
		number_of_skiers = arr[2]
		message = "Number of skiers: " + str(number_of_skiers) + "\n"
		processed.write(message)
	if re.search('\A[0-9]+:.*weight', line) != None:
		arr = re.split('\W+', line)
		skiers_weights[arr[0]] = arr[2] # zmienic na indeks 2
		message = "Skier " + str(arr[0]) + " weight: " + str(arr[2]) + "\n"
		processed.write(message)
	if re.search('\APierwszy|\ADrugi', line) != None:
		arr = re.split('\W+', line)
		lifts_capacity.append(arr[2])
		if len(lifts_capacity) == 1:
			first_lift_free = int(lifts_capacity[0])
			message = "First lift capacity: " + str(lifts_capacity[0]) + "\n"
			processed.write(message)
		else:
			second_lift_free = int(lifts_capacity[1])
			message = "Second lift capacity: " + str(lifts_capacity[1]) + "\n"
			processed.write(message)
	if re.search('entered PHASE_CRITICAL',line) != None:
		arr = re.split('\W+' ,line) 
		log_number = arr[1]
		skier_tid = arr[2]
		state = arr[8]
		chosen_lift = arr[10][-1]
		weight = int(skiers_weights[skier_tid])
		if chosen_lift == str(1):
			first_lift_free -= weight
		elif chosen_lift == str(2):
			second_lift_free -= weight
		message = "["+log_number+"]" + " " + "("+skier_tid+")" + " " + state + " weight:" + str(weight) +" lift:" + chosen_lift + "\t" + str(first_lift_free) + " " + str(second_lift_free) + "\n"
		processed.write(message)
	if re.search('bcast MSG_RELEASE', line) != None:
		arr = re.split('\W+', line)
		log_number = arr[1]
		skier_tid = arr[2]
		state = arr[8]
		chosen_lift = arr[12][-1]
		weight = int(skiers_weights[skier_tid])
		if chosen_lift == str(1):
			first_lift_free += weight
		elif chosen_lift == str(2):
			second_lift_free += weight
		message = "["+log_number+"]" + " " + "("+skier_tid+")" + " " + state + " weight:" + str(weight) +" lift:" + chosen_lift + "\t" + str(first_lift_free) + " " + str(second_lift_free) + "\n"
		processed.write(message)	


processed.close()
critical.close()