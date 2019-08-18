#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <execinfo.h>
#include <pthread.h>
#include <stdbool.h>

/* Global Var for building mutex tables */
pthread_mutex_t* thread_lock_table [10][100] = {0,};
int count_lock_num_per_thread[10] = {0,};
int edge_table [100][100] = {0,};

pthread_mutex_t* lock_list[100];
int lock_count = 0;
static __thread bool lock_exist;

/* Global Var for storing thread ids */
unsigned long tid_list[10];
int thread_count = 0 ;
static __thread bool thread_exist;

static __thread bool lock_exist_in_thread;

/* to keep track of the visited locks */
bool lock_visited[100][100] = {0,};


/* Function that returns index for the thread id */
int
thread_index_return(pthread_t tid){
  int index;
  for(int i = 0; i < thread_count; i++){
    if(tid_list[i] == tid){
      index = i;
    }
  }

  return index;
}

/* Function that returns index for the thread id */
int
lock_index_return(pthread_mutex_t* mutex){
  int index;
  for(int i = 0; i < lock_count; i++){
    if(lock_list[i] == mutex){
      index = i;
    }
  }

  return index;
}

/* To check whether there is a cycle in the program */
void
cycle_check(int row, int col){
    if(!lock_visited[row][col]){
        lock_visited[row][col] = true;
        int j;
        for(j=0; j<100; j++){
            if( (col != j) && (edge_table[col][j] != 0)) cycle_check(col,j);
        }
    }
    else{
        fprintf(stderr, "\n*****DeadLock Spotted*****\n");
    }
}

void
reinitialize_visited(){
  for(int i = 0; i < 100; i++){
    for(int j = 0; j < 100; j++){
      lock_visited[i][j] = false;
    }
  }
}

/* Hooking pthread_mutex_lock */
int
pthread_mutex_lock(pthread_mutex_t *mutex)
{
  int (*pthread_mutex_lockp)(pthread_mutex_t *mutex) ;
	char * error ;

	pthread_mutex_lockp = dlsym(RTLD_NEXT, "pthread_mutex_lock") ;
	if ((error = dlerror()) != 0x0)
		exit(1) ;

  // Thread id
  pthread_t tid = pthread_self();

  /* Store distinct thread ids in tid_list */
  thread_exist = false;
  if(thread_count == 0){
    tid_list[thread_count] = tid;
    thread_count++;
  }else{
    for(int i = 0; i < thread_count; i++){
      if(tid_list[i] == tid){
        thread_exist = true;
      }
    }

    if(!thread_exist)
      tid_list[thread_count++] = tid;
  }

  /* Store distinct lock in lock_list */
  lock_exist = false;
  if(lock_count == 0){
    lock_list[lock_count] = mutex;
    lock_count++;
  }else{
    for(int i = 0; i < lock_count; i++){
      if(lock_list[i] == mutex){
        lock_exist = true;
      }
    }

    if(!lock_exist)
      lock_list[lock_count++] = mutex;
  }

  /* Store Thread_Lock Info in an array */
  int row = thread_index_return(tid);
  int col = count_lock_num_per_thread[row];
  lock_exist_in_thread = false;

  for(int i = 0; i < count_lock_num_per_thread[row]; i++){
    if(thread_lock_table[row][i] == mutex){
      lock_exist_in_thread = true;
    }
  }

  if(!lock_exist_in_thread){
    thread_lock_table[row][col] = mutex;
    count_lock_num_per_thread[row]++;
  }

  int edge_row, edge_col;

  if((!lock_exist_in_thread) && count_lock_num_per_thread[row] > 0){ // A thread has already been holding one or more locks -> time to make an edge
        for(int i = 0; i < col; i++){
            if(thread_lock_table[row][i] != NULL){
                edge_row = lock_index_return(thread_lock_table[row][i]);
                edge_col = lock_index_return(thread_lock_table[row][col]);
                edge_table[edge_row][edge_col] = 1; // make an edge
                cycle_check(edge_row, edge_col);
                reinitialize_visited();
            }
        }
    }


  for(int i = 0; i < lock_count; i++){
    for(int j = 0; j < lock_count; j++){
      printf("%d ", edge_table[i][j]);
    }
    printf("\n");
  }
  return pthread_mutex_lockp(mutex);
}

/* Hooking pthread_mutex_unlock */
int
pthread_mutex_unlock(pthread_mutex_t *mutex)
{
  int (*pthread_mutex_unlockp)(pthread_mutex_t *mutex) ;
	char * error ;

	pthread_mutex_unlockp = dlsym(RTLD_NEXT, "pthread_mutex_unlock") ;
	if ((error = dlerror()) != 0x0)
		exit(1) ;

  pthread_t tid = pthread_self();

  /* Store Thread_Lock Info in an array */
  int row = thread_index_return(tid);
  int col = count_lock_num_per_thread[row];

  for(int i = 0; i < count_lock_num_per_thread[row]; i++){
    if(thread_lock_table[row][i] == mutex)
      thread_lock_table[row][i] = NULL;
  }
  int edge_row, edge_col;


  if(col > 0){
    for(int i = 0; i < col; i++){
      edge_row = lock_index_return(mutex);
      edge_table[edge_row][i] = 0; // erase an edge
      edge_table[i][edge_row] = 0;
    }
  }
  return pthread_mutex_unlockp(mutex);
}
