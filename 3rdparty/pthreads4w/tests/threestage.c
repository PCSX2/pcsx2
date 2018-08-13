/*
 This source code is taken directly from examples in the book
 Windows System Programming, Edition 4 by Johnson (John) Hart

 Session 6, Chapter 10. ThreeStage.c

 Several required additional header and source files from the
 book examples have been included inline to simplify building.
 The only modification to the code has been to provide default
 values when run without arguments.

 Three-stage Producer Consumer system
 Other files required in this project, either directly or
 in the form of libraries (DLLs are preferable)
              QueueObj.c (inlined here)
              Messages.c (inlined here)

 Usage: ThreeStage npc goal [display]
 start up "npc" paired producer  and consumer threads.
          Display messages if "display" is non-zero
 Each producer must produce a total of
 "goal" messages, where each message is tagged
 with the consumer that should receive it
 Messages are sent to a "transmitter thread" which performs
 additional processing before sending message groups to the
 "receiver thread." Finally, the receiver thread sends
 the messages to the consumer threads.

 Transmitter: Receive messages one at a time from producers,
 create a transmission message of up to "TBLOCK_SIZE" messages
 to be sent to the Receiver. (this could be a network xfer
 Receiver: Take message blocks sent by the Transmitter
 and send the individual messages to the designated consumer
 */

/* Suppress warning re use of ctime() */
#define _CRT_SECURE_NO_WARNINGS 1

#include "test.h"
#define sleep(i) Sleep(i*1000)
#ifndef max
#define max(a,b) ((a) > (b) ? (a) : (b))
#endif

#define DATA_SIZE 256
typedef struct msg_block_tag { /* Message block */
  pthread_mutex_t mguard; /* Mutex for  the message block */
  pthread_cond_t  mconsumed; /* Event: Message consumed;          */
  /* Produce a new one or stop    */
  pthread_cond_t  mready; /* Event: Message ready         */
  /*
   * Note: the mutex and events are not used by some programs, such
   * as Program 10-3, 4, 5 (the multi-stage pipeline) as the messages
   * are part of a protected queue
   */
  volatile unsigned int source; /* Creating producer identity     */
  volatile unsigned int destination;/* Identity of receiving thread*/

  volatile unsigned int f_consumed;
  volatile unsigned int f_ready;
  volatile unsigned int f_stop;
  /* Consumed & ready state flags, stop flag      */
  volatile unsigned int sequence; /* Message block sequence number        */
  time_t timestamp;
  unsigned int checksum; /* Message contents checksum             */
  unsigned int data[DATA_SIZE]; /* Message Contents               */
} msg_block_t;

void message_fill (msg_block_t *, unsigned int, unsigned int, unsigned int);
void message_display (msg_block_t *);

#define CV_TIMEOUT 5  /* tunable parameter for the CV model */


/*
 Definitions of a synchronized, general bounded queue structure.
 Queues are implemented as arrays with indices to youngest
 and oldest messages, with wrap around.
 Each queue also contains a guard mutex and
 "not empty" and "not full" condition variables.
 Finally, there is a pointer to an array of messages of
 arbitrary type
 */

typedef struct queue_tag {      /* General purpose queue        */
  pthread_mutex_t q_guard;/* Guard the message block      */
  pthread_cond_t  q_ne;   /* Event: Queue is not empty            */
  pthread_cond_t  q_nf;   /* Event: Queue is not full                     */
  /* These two events are manual-reset for the broadcast model
   * and auto-reset for the signal model */
  volatile unsigned int q_size;   /* Queue max size size          */
  volatile unsigned int q_first;  /* Index of oldest message      */
  volatile unsigned int q_last;   /* Index of youngest msg        */
  volatile unsigned int q_destroyed;/* Q receiver has terminated  */
  void *  msg_array;      /* array of q_size messages     */
} queue_t;

/* Queue management functions */
unsigned int q_initialize (queue_t *, unsigned int, unsigned int);
unsigned int q_destroy (queue_t *);
unsigned int q_destroyed (queue_t *);
unsigned int q_empty (queue_t *);
unsigned int q_full (queue_t *);
unsigned int q_get (queue_t *, void *, unsigned int, unsigned int);
unsigned int q_put (queue_t *, void *, unsigned int, unsigned int);
unsigned int q_remove (queue_t *, void *, unsigned int);
unsigned int q_insert (queue_t *, void *, unsigned int);

#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#define DELAY_COUNT 1000
#define MAX_THREADS 1024

/* Queue lengths and blocking factors. These numbers are arbitrary and  */
/* can be adjusted for performance tuning. The current values are       */
/* not well balanced.                                                   */

#define TBLOCK_SIZE 5   /* Transmitter combines this many messages at at time */
#define Q_TIMEOUT 2000 /* Transmiter and receiver timeout (ms) waiting for messages */
//#define Q_TIMEOUT INFINITE
#define MAX_RETRY 5  /* Number of q_get retries before quitting */
#define P2T_QLEN 10     /* Producer to Transmitter queue length */
#define T2R_QLEN 4      /* Transmitter to Receiver queue length */
#define R2C_QLEN 4      /* Receiver to Consumer queue length - there is one
 * such queue for each consumer */

void * producer (void *);
void * consumer (void *);
void * transmitter (void *);
void * receiver (void *);


typedef struct _THARG {
  volatile unsigned int thread_number;
  volatile unsigned int work_goal;    /* used by producers */
  volatile unsigned int work_done;    /* Used by producers and consumers */
} THARG;


/* Grouped messages sent by the transmitter to receiver         */
typedef struct T2R_MSG_TYPEag {
  volatile unsigned int num_msgs; /* Number of messages contained */
  msg_block_t messages [TBLOCK_SIZE];
} T2R_MSG_TYPE;

queue_t p2tq, t2rq, *r2cq_array;

/* ShutDown, AllProduced are global flags to shut down the system & transmitter */
static volatile unsigned int ShutDown = 0;
static volatile unsigned int AllProduced = 0;
static unsigned int DisplayMessages = 0;

int main (int argc, char * argv[])
{
  unsigned int tstatus = 0, nthread, ithread, goal, thid;
  pthread_t *producer_th, *consumer_th, transmitter_th, receiver_th;
  THARG *producer_arg, *consumer_arg;

  if (argc < 3) {
      nthread = 32;
      goal = 1000;
  } else {
      nthread = atoi(argv[1]);
      goal = atoi(argv[2]);
      if (argc >= 4)
        DisplayMessages = atoi(argv[3]);
  }

  srand ((int)time(NULL));        /* Seed the RN generator */

  if (nthread > MAX_THREADS) {
      printf ("Maximum number of producers or consumers is %d.\n", MAX_THREADS);
      return 2;
  }
  producer_th = (pthread_t *) malloc (nthread * sizeof(pthread_t));
  producer_arg = (THARG *) calloc (nthread, sizeof (THARG));
  consumer_th = (pthread_t *) malloc (nthread * sizeof(pthread_t));
  consumer_arg = (THARG *) calloc (nthread, sizeof (THARG));

  if (producer_th == NULL || producer_arg == NULL
      || consumer_th == NULL || consumer_arg == NULL)
    perror ("Cannot allocate working memory for threads.");

  q_initialize (&p2tq, sizeof(msg_block_t), P2T_QLEN);
  q_initialize (&t2rq, sizeof(T2R_MSG_TYPE), T2R_QLEN);
  /* Allocate and initialize Receiver to Consumer queue for each consumer */
  r2cq_array = (queue_t *) calloc (nthread, sizeof(queue_t));
  if (r2cq_array == NULL) perror ("Cannot allocate memory for r2c queues");

  for (ithread = 0; ithread < nthread; ithread++) {
      /* Initialize r2c queue for this consumer thread */
      q_initialize (&r2cq_array[ithread], sizeof(msg_block_t), R2C_QLEN);
      /* Fill in the thread arg */
      consumer_arg[ithread].thread_number = ithread;
      consumer_arg[ithread].work_goal = goal;
      consumer_arg[ithread].work_done = 0;

      tstatus = pthread_create (&consumer_th[ithread], NULL,
          consumer, (void *)&consumer_arg[ithread]);
      if (tstatus != 0)
        perror ("Cannot create consumer thread");

      producer_arg[ithread].thread_number = ithread;
      producer_arg[ithread].work_goal = goal;
      producer_arg[ithread].work_done = 0;
      tstatus = pthread_create (&producer_th[ithread], NULL,
          producer, (void *)&producer_arg[ithread]);
      if (tstatus != 0)
        perror ("Cannot create producer thread");
  }

  tstatus = pthread_create (&transmitter_th, NULL, transmitter, &thid);
  if (tstatus != 0)
    perror ("Cannot create tranmitter thread");
  tstatus = pthread_create (&receiver_th, NULL, receiver, &thid);
  if (tstatus != 0)
    perror ("Cannot create receiver thread");


  printf ("BOSS: All threads are running\n");
  /* Wait for the producers to complete */
  /* The implementation allows too many threads for WaitForMultipleObjects */
  /* although you could call WFMO in a loop */
  for (ithread = 0; ithread < nthread; ithread++) {
      tstatus = pthread_join (producer_th[ithread], NULL);
      if (tstatus != 0)
        perror ("Cannot wait for producer thread");
      printf ("BOSS: Producer %d produced %d work units\n",
          ithread, producer_arg[ithread].work_done);
  }
  /* Producers have completed their work. */
  printf ("BOSS: All producers have completed their work.\n");
  AllProduced = 1;

  /* Wait for the consumers to complete */
  for (ithread = 0; ithread < nthread; ithread++) {
      tstatus = pthread_join (consumer_th[ithread], NULL);
      if (tstatus != 0)
        perror ("Cannot wait for consumer thread");
      printf ("BOSS: consumer %d consumed %d work units\n",
          ithread, consumer_arg[ithread].work_done);
  }
  printf ("BOSS: All consumers have completed their work.\n");

  ShutDown = 1; /* Set a shutdown flag - All messages have been consumed */

  /* Wait for the transmitter and receiver */

  tstatus = pthread_join (transmitter_th, NULL);
  if (tstatus != 0)
    perror ("Failed waiting for transmitter");
  tstatus = pthread_join (receiver_th, NULL);
  if (tstatus != 0)
    perror ("Failed waiting for receiver");

  q_destroy (&p2tq);
  q_destroy (&t2rq);
  for (ithread = 0; ithread < nthread; ithread++)
    q_destroy (&r2cq_array[ithread]);
  free (r2cq_array);
  free (producer_th);
  free (consumer_th);
  free (producer_arg);
  free(consumer_arg);
  printf ("System has finished. Shutting down\n");
  return 0;
}

void * producer (void * arg)
{
  THARG * parg;
  unsigned int ithread, tstatus = 0;
  msg_block_t msg;

  parg = (THARG *)arg;
  ithread = parg->thread_number;

  while (parg->work_done < parg->work_goal && !ShutDown) {
      /* Periodically produce work units until the goal is satisfied */
      /* messages receive a source and destination address which are */
      /* the same in this case but could, in general, be different. */
      sleep (rand()/100000000);
      message_fill (&msg, ithread, ithread, parg->work_done);

      /* put the message in the queue - Use an infinite timeout to assure
       * that the message is inserted, even if consumers are delayed */
      tstatus = q_put (&p2tq, &msg, sizeof(msg), INFINITE);
      if (0 == tstatus) {
          parg->work_done++;
      }
  }

  return 0;
}

void * consumer (void * arg)
{
  THARG * carg;
  unsigned int tstatus = 0, ithread, Retries = 0;
  msg_block_t msg;
  queue_t *pr2cq;

  carg = (THARG *) arg;
  ithread = carg->thread_number;

  carg = (THARG *)arg;
  pr2cq = &r2cq_array[ithread];

  while (carg->work_done < carg->work_goal && Retries < MAX_RETRY && !ShutDown) {
      /* Receive and display/process messages */
      /* Try to receive the requested number of messages,
       * but allow for early system shutdown */

      tstatus = q_get (pr2cq, &msg, sizeof(msg), Q_TIMEOUT);
      if (0 == tstatus) {
          if (DisplayMessages > 0) message_display (&msg);
          carg->work_done++;
          Retries = 0;
      } else {
          Retries++;
      }
  }

  return NULL;
}

void * transmitter (void * arg)
{

  /* Obtain multiple producer messages, combining into a single   */
  /* compound message for the receiver */

  unsigned int tstatus = 0, im, Retries = 0;
  T2R_MSG_TYPE t2r_msg = {0};
  msg_block_t p2t_msg;

  while (!ShutDown && !AllProduced) {
      t2r_msg.num_msgs = 0;
      /* pack the messages for transmission to the receiver */
      im = 0;
      while (im < TBLOCK_SIZE && !ShutDown && Retries < MAX_RETRY && !AllProduced) {
          tstatus = q_get (&p2tq, &p2t_msg, sizeof(p2t_msg), Q_TIMEOUT);
          if (0 == tstatus) {
              memcpy (&t2r_msg.messages[im], &p2t_msg, sizeof(p2t_msg));
              t2r_msg.num_msgs++;
              im++;
              Retries = 0;
          } else { /* Timed out.  */
              Retries++;
          }
      }
      tstatus = q_put (&t2rq, &t2r_msg, sizeof(t2r_msg), INFINITE);
      if (tstatus != 0) return NULL;
  }
  return NULL;
}


void * receiver (void * arg)
{
  /* Obtain compound messages from the transmitter and unblock them       */
  /* and transmit to the designated consumer.                             */

  unsigned int tstatus = 0, im, ic, Retries = 0;
  T2R_MSG_TYPE t2r_msg;
  msg_block_t r2c_msg;

  while (!ShutDown && Retries < MAX_RETRY) {
      tstatus = q_get (&t2rq, &t2r_msg, sizeof(t2r_msg), Q_TIMEOUT);
      if (tstatus != 0) { /* Timeout - Have the producers shut down? */
          Retries++;
          continue;
      }
      Retries = 0;
      /* Distribute the packaged messages to the proper consumer */
      im = 0;
      while (im < t2r_msg.num_msgs) {
          memcpy (&r2c_msg, &t2r_msg.messages[im], sizeof(r2c_msg));
          ic = r2c_msg.destination; /* Destination consumer */
          tstatus = q_put (&r2cq_array[ic], &r2c_msg, sizeof(r2c_msg), INFINITE);
          if (0 == tstatus) im++;
      }
  }
  return NULL;
}

#if (!defined INFINITE)
#define INFINITE 0xFFFFFFFF
#endif

/*
 Finite bounded queue management functions
 q_get, q_put timeouts (max_wait) are in ms - convert to sec, rounding up
 */
unsigned int q_get (queue_t *q, void * msg, unsigned int msize, unsigned int MaxWait)
{
  int tstatus = 0, got_msg = 0, time_inc = (MaxWait + 999) /1000;
  struct timespec timeout;
  timeout.tv_nsec = 0;

  if (q_destroyed(q)) return 1;
  pthread_mutex_lock (&q->q_guard);
  while (q_empty (q) && 0 == tstatus) {
      if (MaxWait != INFINITE) {
          timeout.tv_sec = time(NULL) + time_inc;
          tstatus = pthread_cond_timedwait (&q->q_ne, &q->q_guard, &timeout);
      } else {
          tstatus = pthread_cond_wait (&q->q_ne, &q->q_guard);
      }
  }
  /* remove the message, if any, from the queue */
  if (0 == tstatus && !q_empty (q)) {
      q_remove (q, msg, msize);
      got_msg = 1;
      /* Signal that the queue is not full as we've removed a message */
      pthread_cond_broadcast (&q->q_nf);
  }
  pthread_mutex_unlock (&q->q_guard);
  return (0 == tstatus && got_msg == 1 ? 0 : max(1, tstatus));   /* 0 indicates success */
}

unsigned int q_put (queue_t *q, void * msg, unsigned int msize, unsigned int MaxWait)
{
  int tstatus = 0, put_msg = 0, time_inc = (MaxWait + 999) /1000;
  struct timespec timeout;
  timeout.tv_nsec = 0;

  if (q_destroyed(q)) return 1;
  pthread_mutex_lock (&q->q_guard);
  while (q_full (q) && 0 == tstatus) {
      if (MaxWait != INFINITE) {
          timeout.tv_sec = time(NULL) + time_inc;
          tstatus = pthread_cond_timedwait (&q->q_nf, &q->q_guard, &timeout);
      } else {
          tstatus = pthread_cond_wait (&q->q_nf, &q->q_guard);
      }
  }
  /* Insert the message into the queue if there's room */
  if (0 == tstatus && !q_full (q)) {
      q_insert (q, msg, msize);
      put_msg = 1;
      /* Signal that the queue is not empty as we've inserted a message */
      pthread_cond_broadcast (&q->q_ne);
  }
  pthread_mutex_unlock (&q->q_guard);
  return (0 == tstatus && put_msg == 1 ? 0 : max(1, tstatus));   /* 0 indictates success */
}

unsigned int q_initialize (queue_t *q, unsigned int msize, unsigned int nmsgs)
{
  /* Initialize queue, including its mutex and events */
  /* Allocate storage for all messages. */

  q->q_first = q->q_last = 0;
  q->q_size = nmsgs;
  q->q_destroyed = 0;

  pthread_mutex_init (&q->q_guard, NULL);
  pthread_cond_init (&q->q_ne, NULL);
  pthread_cond_init (&q->q_nf, NULL);

  if ((q->msg_array = calloc (nmsgs, msize)) == NULL) return 1;
  return 0; /* No error */
}

unsigned int q_destroy (queue_t *q)
{
  if (q_destroyed(q)) return 1;
  /* Free all the resources created by q_initialize */
  pthread_mutex_lock (&q->q_guard);
  q->q_destroyed = 1;
  free (q->msg_array);
  pthread_cond_destroy (&q->q_ne);
  pthread_cond_destroy (&q->q_nf);
  pthread_mutex_unlock (&q->q_guard);
  pthread_mutex_destroy (&q->q_guard);

  return 0;
}

unsigned int q_destroyed (queue_t *q)
{
  return (q->q_destroyed);
}

unsigned int q_empty (queue_t *q)
{
  return (q->q_first == q->q_last);
}

unsigned int q_full (queue_t *q)
{
  return ((q->q_first - q->q_last) == 1 ||
      (q->q_last == q->q_size-1 && q->q_first == 0));
}


unsigned int q_remove (queue_t *q, void * msg, unsigned int msize)
{
  char *pm;

  pm = (char *)q->msg_array;
  /* Remove oldest ("first") message */
  memcpy (msg, pm + (q->q_first * msize), msize);
  // Invalidate the message
  q->q_first = ((q->q_first + 1) % q->q_size);
  return 0; /* no error */
}

unsigned int q_insert (queue_t *q, void * msg, unsigned int msize)
{
  char *pm;

  pm = (char *)q->msg_array;
  /* Add a new youngest ("last") message */
  if (q_full(q)) return 1; /* Error - Q is full */
  memcpy (pm + (q->q_last * msize), msg, msize);
  q->q_last = ((q->q_last + 1) % q->q_size);

  return 0;
}

unsigned int compute_checksum (void * msg, unsigned int length)
{
  /* Computer an xor checksum on the entire message of "length"
   * integers */
  unsigned int i, cs = 0, *pint;

  pint = (unsigned int *) msg;
  for (i = 0; i < length; i++) {
      cs = (cs ^ *pint);
      pint++;
  }
  return cs;
}

void  message_fill (msg_block_t *mblock, unsigned int src, unsigned int dest, unsigned int seqno)
{
  /* Fill the message buffer, and include checksum and timestamp  */
  /* This function is called from the producer thread while it    */
  /* owns the message block mutex                                 */

  unsigned int i;

  mblock->checksum = 0;
  for (i = 0; i < DATA_SIZE; i++) {
      mblock->data[i] = rand();
  }
  mblock->source = src;
  mblock->destination = dest;
  mblock->sequence = seqno;
  mblock->timestamp = time(NULL);
  mblock->checksum = compute_checksum (mblock, sizeof(msg_block_t)/sizeof(unsigned int));
  /*      printf ("Generated message: %d %d %d %d %x %x\n",
                src, dest, seqno, mblock->timestamp,
                mblock->data[0], mblock->data[DATA_SIZE-1]);  */
  return;
}

void  message_display (msg_block_t *mblock)
{
  /* Display message buffer and timestamp, validate checksum      */
  /* This function is called from the consumer thread while it    */
  /* owns the message block mutex                                 */
  unsigned int tcheck = 0;

  tcheck = compute_checksum (mblock, sizeof(msg_block_t)/sizeof(unsigned int));
  printf ("\nMessage number %d generated at: %s",
      mblock->sequence, ctime (&(mblock->timestamp)));
  printf ("Source and destination: %d %d\n",
      mblock->source, mblock->destination);
  printf ("First and last entries: %x %x\n",
      mblock->data[0], mblock->data[DATA_SIZE-1]);
  if (tcheck == 0 /*mblock->checksum was 0 when CS first computed */)
    printf ("GOOD ->Checksum was validated.\n");
  else
    printf ("BAD  ->Checksum failed. message was corrupted\n");

  return;

}
