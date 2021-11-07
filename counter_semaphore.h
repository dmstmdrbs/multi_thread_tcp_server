
#include "pthread.h"
#include <errno.h>
#include "echolib.h"

typedef struct c_semaphore{
  int count;
  pthread_mutex_t mutex;
  pthread_cond_t cv;
} c_semaphore;

int init_c_semaphore(c_semaphore* sem, int pshared, int value){
  if(pshared) { errno = ENOSYS; return -1;}
  sem->count = value;
  pthread_mutex_init(&sem->mutex, NULL);
  pthread_cond_init(&sem->cv,NULL);
}

void c_sem_client_disconnect(c_semaphore* sem){
  pthread_mutex_unlock(&sem->mutex);
  pthread_mutex_lock(&sem->mutex);
  sem->count++;
  if(sem->count==1){
    /* to notice with cv for waiting semaphore */
    pthread_mutex_unlock(&sem->mutex);
    if(shutting_down){
      pthread_cond_broadcast(&sem->cv);
    }
    else pthread_cond_signal(&sem->cv);
  }else{
    pthread_mutex_unlock(&sem->mutex);
    if(shutting_down){
      pthread_cond_broadcast(&sem->cv);
    }
  }
}

void c_sem_client_wait_to_connect(c_semaphore* sem){
  pthread_mutex_lock(&sem->mutex);
  while(sem->count==0 && !shutting_down){
    pthread_cond_wait(&sem->cv,&sem->mutex);
  }
  if(shutting_down){
    pthread_mutex_unlock(&sem->mutex);
  }else{
    sem->count--;
    pthread_mutex_unlock(&sem->mutex);
  }
}