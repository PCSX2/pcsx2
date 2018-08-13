#
# Common elements to all makefiles
#

ALL_KNOWN_TESTS = \
	affinity1 affinity2 affinity3 affinity4 affinity5 affinity6 \
	barrier1 barrier2 barrier3 barrier4 barrier5 barrier6 \
	cancel1 cancel2 cancel3 cancel4 cancel5 cancel6a cancel6d \
	cancel7 cancel8 cancel9 \
	cleanup0 cleanup1 cleanup2 cleanup3 \
	condvar1 condvar1_1 condvar1_2 condvar2 condvar2_1 \
	condvar3 condvar3_1 condvar3_2 condvar3_3 \
	condvar4 condvar5 condvar6 \
	condvar7 condvar8 condvar9 \
	timeouts \
	count1 \
	context1 \
	create1 create2 create3 \
	delay1 delay2 \
	detach1 \
	equal1 \
	errno1 errno0 \
	exception1 exception2 exception3_0 exception3 \
	exit1 exit2 exit3 exit4 exit5 exit6 \
	eyal1 \
	join0 join1 join2 join3 join4 \
	kill1 \
	mutex1 mutex1n mutex1e mutex1r \
	mutex2 mutex2r mutex2e mutex3 mutex3r mutex3e \
	mutex4 mutex5 mutex6 mutex6n mutex6e mutex6r \
	mutex6s mutex6es mutex6rs \
	mutex7 mutex7n mutex7e mutex7r \
	mutex8 mutex8n mutex8e mutex8r \
	name_np1 name_np2 \
	once1 once2 once3 once4 \
	priority1 priority2 inherit1 \
	reinit1 \
	reuse1 reuse2 \
	robust1 robust2 robust3 robust4 robust5 \
	rwlock1 rwlock2 rwlock3 rwlock4 \
	rwlock2_t rwlock3_t rwlock4_t rwlock5_t rwlock6_t rwlock6_t2 \
	rwlock5 rwlock6 rwlock7 rwlock8 \
	self1 self2 \
	semaphore1 semaphore2 semaphore3 \
	semaphore4 semaphore4t semaphore5 \
	sequence1 \
	sizes \
	spin1 spin2 spin3 spin4 \
	stress1 threestage \
	tsd1 tsd2 tsd3 \
	valid1 valid2

TESTS = $(ALL_KNOWN_TESTS)

BENCHTESTS = \
	benchtest1 benchtest2 benchtest3 benchtest4 benchtest5

# Output useful info if no target given. I.e. the first target that "make" sees is used in this case.
default_target: help
	