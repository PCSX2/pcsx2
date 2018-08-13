/*
 * reinit1.c
 *
 * Same test as rwlock7.c but loop two or more times reinitialising the library
 * each time, to test reinitialisation. We use a rwlock test because rw locks
 * use CVs, mutexes and semaphores internally.
 *
 * rwlock7.c description:
 * Hammer on a bunch of rwlocks to test robustness and fairness.
 * Printed stats should be roughly even for each thread.
 */

#include "test.h"
#include <sys/timeb.h>

#ifdef __GNUC__
#include <stdlib.h>
#endif

#define THREADS         5
#define DATASIZE        7
#define ITERATIONS      1000000
#define LOOPS           3

/*
 * Keep statistics for each thread.
 */
typedef struct thread_tag {
  int         thread_num;
  pthread_t   thread_id;
  int         updates;
  int         reads;
  int         changed;
  int         seed;
} thread_t;

/*
 * Read-write lock and shared data
 */
typedef struct data_tag {
  pthread_rwlock_t    lock;
  int                 data;
  int                 updates;
} data_t;

static thread_t threads[THREADS];
static data_t data[DATASIZE];

/*
 * Thread start routine that uses read-write locks
 */
void *thread_routine (void *arg)
{
  thread_t *self = (thread_t*)arg;
  int iteration;
  int element = 0;
  int seed = self->seed;
  int interval = 1 + rand_r (&seed) % 71;

  self->changed = 0;

  assert(pthread_getunique_np(self->thread_id) == (unsigned __int64)(self->thread_num + 2));

  for (iteration = 0; iteration < ITERATIONS; iteration++)
    {
      /*
       * Each "self->interval" iterations, perform an
       * update operation (write lock instead of read
       * lock).
       */
      if ((iteration % interval) == 0)
        {
          assert(pthread_rwlock_wrlock (&data[element].lock) == 0);
          data[element].data = self->thread_num;
          data[element].updates++;
          self->updates++;
	  interval = 1 + rand_r (&seed) % 71;
          assert(pthread_rwlock_unlock (&data[element].lock) == 0);
        } else {
          /*
           * Look at the current data element to see whether
           * the current thread last updated it. Count the
           * times, to report later.
           */
          assert(pthread_rwlock_rdlock (&data[element].lock) == 0);

          self->reads++;

          if (data[element].data != self->thread_num)
            {
              self->changed++;
	      interval = 1 + self->changed % 71;
            }

          assert(pthread_rwlock_unlock (&data[element].lock) == 0);
        }

      element = (element + 1) % DATASIZE;

    }

  return NULL;
}

int
main (int argc, char *argv[])
{
  int count;
  int data_count;
  int reinit_count;
  int seed = 1;

  for (reinit_count = 0; reinit_count < LOOPS; reinit_count++)
    {
      /*
       * Initialize the shared data.
       */
      for (data_count = 0; data_count < DATASIZE; data_count++)
        {
          data[data_count].data = 0;
          data[data_count].updates = 0;

          assert(pthread_rwlock_init (&data[data_count].lock, NULL) == 0);
        }

      /*
       * Create THREADS threads to access shared data.
       */
      for (count = 0; count < THREADS; count++)
        {
          threads[count].thread_num = count;
          threads[count].updates = 0;
          threads[count].reads = 0;
          threads[count].seed = 1 + rand_r (&seed) % 71;

          assert(pthread_create (&threads[count].thread_id,
                                 NULL, thread_routine, (void*)(size_t)&threads[count]) == 0);
        }

      /*
       * Wait for all threads to complete, and collect
       * statistics.
       */
      for (count = 0; count < THREADS; count++)
        {
          assert(pthread_join (threads[count].thread_id, NULL) == 0);
        }

      pthread_win32_process_detach_np();
      pthread_win32_process_attach_np();
    }

  return 0;
}
