#
# Common rules that define the run order of tests
#
benchtest1.bench:
benchtest2.bench:
benchtest3.bench:
benchtest4.bench:
benchtest5.bench:

affinity1.pass: errno0.pass
affinity2.pass: affinity1.pass
affinity3.pass: affinity2.pass self1.pass create3.pass
affinity4.pass: affinity3.pass
affinity5.pass: affinity4.pass
affinity6.pass: affinity5.pass
barrier1.pass: semaphore4.pass
barrier2.pass: barrier1.pass semaphore4.pass
barrier3.pass: barrier2.pass semaphore4.pass self1.pass create3.pass join4.pass
barrier4.pass: barrier3.pass semaphore4.pass self1.pass create3.pass join4.pass mutex8.pass
barrier5.pass: barrier4.pass semaphore4.pass self1.pass create3.pass join4.pass mutex8.pass
barrier6.pass: barrier5.pass semaphore4.pass self1.pass create3.pass join4.pass mutex8.pass
cancel1.pass: self1.pass create3.pass
cancel2.pass: self1.pass create3.pass join4.pass barrier6.pass
cancel3.pass: self1.pass create3.pass join4.pass context1.pass
cancel4.pass: cancel3.pass self1.pass create3.pass join4.pass
cancel5.pass: cancel3.pass self1.pass create3.pass join4.pass
cancel6a.pass: cancel3.pass self1.pass create3.pass join4.pass
cancel6d.pass: cancel3.pass self1.pass create3.pass join4.pass
cancel7.pass: self1.pass create3.pass join4.pass kill1.pass
cancel8.pass: cancel7.pass self1.pass mutex8.pass kill1.pass
cancel9.pass: cancel8.pass self1.pass create3.pass join4.pass mutex8.pass kill1.pass
cleanup0.pass: self1.pass create3.pass join4.pass mutex8.pass cancel5.pass
cleanup1.pass: cleanup0.pass
cleanup2.pass: cleanup1.pass
cleanup3.pass: cleanup2.pass
condvar1.pass: self1.pass create3.pass semaphore1.pass mutex8.pass
condvar1_1.pass: condvar1.pass
condvar1_2.pass: join2.pass
condvar2.pass: condvar1.pass
condvar2_1.pass: condvar2.pass join2.pass
condvar3.pass: create1.pass condvar2.pass
condvar3_1.pass: condvar3.pass join2.pass
condvar3_2.pass: condvar3_1.pass
condvar3_3.pass: condvar3_2.pass
condvar4.pass: create1.pass
condvar5.pass: condvar4.pass
condvar6.pass: condvar5.pass
condvar7.pass: condvar6.pass cleanup1.pass
condvar8.pass: condvar7.pass
condvar9.pass: condvar8.pass
context1.pass: cancel1.pass
count1.pass: join1.pass
create1.pass: mutex2.pass
create2.pass: create1.pass
create3.pass: create2.pass
delay1.pass: self1.pass create3.pass
delay2.pass: delay1.pass
detach1.pass: join0.pass
equal1.pass: self1.pass create1.pass
errno0.pass: sizes.pass
errno1.pass: mutex3.pass
exception1.pass: cancel4.pass
exception2.pass: exception1.pass
exception3_0.pass: exception2.pass
exception3.pass: exception3_0.pass
exit1.pass: self1.pass create3.pass
exit2.pass: create1.pass
exit3.pass: create1.pass
exit4.pass: self1.pass create3.pass 
exit5.pass: exit4.pass kill1.pass
exit6.pass: exit5.pass
eyal1.pass: self1.pass create3.pass mutex8.pass tsd1.pass
inherit1.pass: join1.pass priority1.pass
join0.pass: create1.pass
join1.pass: create1.pass
join2.pass: create1.pass
join3.pass: join2.pass
join4.pass: join3.pass
kill1.pass: self1.pass
mutex1.pass: mutex5.pass
mutex1n.pass: mutex1.pass
mutex1e.pass: mutex1.pass
mutex1r.pass: mutex1.pass
mutex2.pass: mutex1.pass
mutex2r.pass: mutex2.pass
mutex2e.pass: mutex2.pass
mutex3.pass: create1.pass
mutex3r.pass: mutex3.pass
mutex3e.pass: mutex3.pass
mutex4.pass: mutex3.pass
mutex5.pass: sizes.pass
mutex6.pass: mutex4.pass
mutex6n.pass: mutex4.pass
mutex6e.pass: mutex4.pass
mutex6r.pass: mutex4.pass
mutex6s.pass: mutex6.pass
mutex6rs.pass: mutex6r.pass
mutex6es.pass: mutex6e.pass
mutex7.pass: mutex6.pass
mutex7n.pass: mutex6n.pass
mutex7e.pass: mutex6e.pass
mutex7r.pass: mutex6r.pass
mutex8.pass: mutex7.pass
mutex8n.pass: mutex7n.pass
mutex8e.pass: mutex7e.pass
mutex8r.pass: mutex7r.pass
name_np1.pass: join4.pass barrier6.pass
name_np2.pass: name_np1.pass
once1.pass: create1.pass
once2.pass: once1.pass
once3.pass: once2.pass
once4.pass: once3.pass
priority1.pass: join1.pass
priority2.pass: priority1.pass barrier3.pass
reinit1.pass: rwlock7.pass
reuse1.pass: create3.pass
reuse2.pass: reuse1.pass
robust1.pass: mutex8r.pass
robust2.pass: mutex8r.pass
robust3.pass: robust2.pass
robust4.pass: robust3.pass
robust5.pass: robust4.pass
rwlock1.pass: condvar6.pass
rwlock2.pass: rwlock1.pass
rwlock3.pass: rwlock2.pass join2.pass
rwlock4.pass: rwlock3.pass
rwlock5.pass: rwlock4.pass
rwlock6.pass: rwlock5.pass
rwlock7.pass: rwlock6.pass
rwlock8.pass: rwlock7.pass
rwlock2_t.pass: rwlock2.pass
rwlock3_t.pass: rwlock2_t.pass
rwlock4_t.pass: rwlock3_t.pass
rwlock5_t.pass: rwlock4_t.pass
rwlock6_t.pass: rwlock5_t.pass
rwlock6_t2.pass: rwlock6_t.pass
self1.pass: sizes.pass
self2.pass: self1.pass equal1.pass create1.pass
semaphore1.pass: sizes.pass
semaphore2.pass: semaphore1.pass
semaphore3.pass: semaphore2.pass
semaphore4.pass: semaphore3.pass cancel1.pass
semaphore4t.pass: semaphore4.pass
semaphore5.pass: semaphore4.pass
sequence1.pass: reuse2.pass
sizes.pass: 
spin1.pass: self1.pass create3.pass mutex8.pass
spin2.pass: spin1.pass
spin3.pass: spin2.pass
spin4.pass: spin3.pass
stress1.pass: create3.pass mutex8.pass barrier6.pass
threestage.pass: stress1.pass
timeouts.pass: condvar9.pass
tsd1.pass: barrier5.pass join1.pass
tsd2.pass: tsd1.pass
tsd3.pass: tsd2.pass
valid1.pass: join1.pass
valid2.pass: valid1.pass
