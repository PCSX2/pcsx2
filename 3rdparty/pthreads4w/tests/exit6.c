/*
 * exit6.c
 *
 *  Created on: 14/05/2013
 *      Author: ross
 */

#include "test.h"
#ifndef _UWIN
#include <process.h>
#endif

#include <pthread.h>
//#include <stdlib.h>

static pthread_key_t key;
static int where;

static unsigned __stdcall
start_routine(void * arg)
{
  int *val = (int *) malloc(4);

  where = 2;
  //printf("start_routine: native thread\n");

  *val = 48;
  pthread_setspecific(key, val);
  return 0;
}

static void
key_dtor(void *arg)
{
  //printf("key_dtor: %d\n", *(int*)arg);
  if (where == 2)
    printf("Library has thread exit POSIX cleanup for native threads.\n");
  else
    printf("Library has process exit POSIX cleanup for native threads.\n");
  free(arg);
}

int main(int argc, char **argv)
{
  HANDLE hthread;

  where = 1;
  pthread_key_create(&key, key_dtor);
  hthread = (HANDLE)_beginthreadex(NULL, 0, start_routine, NULL, 0, NULL);
  WaitForSingleObject(hthread, INFINITE);
  CloseHandle(hthread);
  where = 3;
  pthread_key_delete(key);

  //printf("main: exiting\n");
  return 0;
}
