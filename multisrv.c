/* file: echosrv.c

   Bare-bones single-threaded TCP server. Listens for connections
   on "ephemeral" socket, assigned dynamically by the OS.

   This started out with an example in W. Richard Stevens' book
   "Advanced Programming in the Unix Environment".  I have
   modified it quite a bit, including changes to make use of my
   own re-entrant version of functions in echolib.

   NOTE: See comments starting with "NOTE:" for indications of
   places where code needs to be added to make this multithreaded.
   Remove those comments from your solution before turning it in,
   and replace them by comments explaining the parts you have
   changed.

   Ted Baker
   February 2015

 */

#include "config.h"
/* not needed now, but will be needed in multi-threaded version */
#include "pthread.h"
#include "echolib.h"
#include "checks.h"
#include "thread_deque.h"
#include "counter_semaphore.h"

int MAX_THREAD;
int WORKER_THREAD;
#define QUEUE_LEN 256

void* serve_connection (void* sockfd);
int check_prime(int n, int socket_id);

struct c_semaphore connection_thread_pool;
pthread_t* threads;
int rc;
long t = 0;
long status;

Task TaskQueue[QUEUE_LEN];
TaskDeque deque;

int task_count = 0;
pthread_mutex_t mutex_for_queue;
pthread_mutex_t mutex_for_task;
pthread_cond_t condQueue;
pthread_cond_t taskCond;

/* TODO : Implementation with linked-list */
void getRequest(int* arg1, char* line, int socket_id, connection_t* conn){
    ssize_t n;
    if (shutting_down){
      c_sem_client_disconnect(&connection_thread_pool);
      rc =pthread_join(threads[connection_thread_pool.count-1],(void**)&status);
      if(rc){
          fprintf(stdout, "Error; return code from pthread_join() is %d\n", rc);
          fflush(stdout);
          exit(-1);
      }
      CHECK (close (conn->sockfd));
    }
    usleep(50000);

    int i=0;
    int len=1;
    char* array[MAXLINE];
    char* p = strtok(line," ");
    while (p != '\0')
    {
        array[i++] = p;
        p = strtok (NULL, " ");
    }
    /*
    while (1)
    {
      array[i++] = p;
      p = strtok (NULL, " ");
      if(p != '\0'){
        char* tmp = malloc(sizeof(char)*MAXLINE);
        tmp = strtok(array[i-1],'\n');
        printf("in if : %s\n",tmp);
        strcpy(array[i-1],tmp);
        free(tmp);
        break;
      }
    }
    */
    len = i;
  
    for(i=0;i<len-1;i++){
      //printf("%s\n", array[i]);
      int is_prime = check_prime(atoi(array[i]),socket_id);
      char* buffer = malloc(sizeof(array[i]));
      strncpy(buffer, array[i], strlen(array[i]));
    
      if(is_prime) strcat(buffer, " is prime number\n");
      else strcat(buffer, " is not prime number\n");
      //printf("%s",buffer);

      *arg1 = 1;
      n = strlen(buffer); // strlen(line) == 2
      
      if (shutting_down){
        c_sem_client_disconnect(&connection_thread_pool);
        //printf("getRequest2\n");
        rc =pthread_join(threads[connection_thread_pool.count-1],(void**)&status);
        if(rc){
            fprintf(stdout, "Error; return code from pthread_join() is %d\n", rc);
            fflush(stdout);
            exit(-1);
        }
        //fprintf(stdout,"getRequest2 : remain sem->count : %d\n",connection_thread_pool.count);
        CHECK (close (conn->sockfd));
      }

      if (writen (conn, buffer, n) != n) {
        c_sem_client_disconnect(&connection_thread_pool);

        rc =pthread_join(threads[connection_thread_pool.count-1],(void**)&status);
        if(rc){
            fprintf(stdout, "Error; return code from pthread_join() is %d\n", rc);
            fflush(stdout);
            exit(-1);
        }

        //fprintf(stdout,"getRequest : remain sem->count : %d\n",connection_thread_pool.count);
        CHECK (close (conn->sockfd));
      }
      free(buffer);
    }
    pthread_cond_signal(&taskCond);
}

void executeTask(Task* task){
  task->taskFunction(task->arg1,task->arg2, task->socket_id, task->conn);
}

void submitTask(Task task){
  pthread_mutex_lock(&mutex_for_queue);

  //printf("in submitTask : after mutex lock\n");
  add_rear(&deque, task);

  pthread_mutex_unlock(&mutex_for_queue);

  pthread_cond_signal(&condQueue);
}

void* startThread(void* args){
    while(!shutting_down){
        Task task;
        pthread_mutex_lock(&mutex_for_queue);
        // wait if there is no task in queue
        while(!shutting_down&&isEmpty(&deque)){
          //printf("in startThread : waiting\n");
          pthread_cond_wait(&condQueue, &mutex_for_queue);
        }

        if(!shutting_down){
          //printf("task poped from deque\n");
          task = pop_front(&deque);
          pthread_mutex_unlock(&mutex_for_queue);
          //printf("unlock mutex for deque\n");
          executeTask(&task);
        }
        else{
          pthread_mutex_unlock(&mutex_for_queue);
          break;
        }
    }
    //printf("end of startThread\n");
}

void server_handoff (int sockfd, c_semaphore* sem, pthread_t* threads) {
  /* check connection */
  //fprintf(stdout,"server_handout : start\n");
  c_sem_client_wait_to_connect(sem);

  //fprintf(stdout,"server_handout : connect client \n");
  /* create threads */
  int rc;
  fprintf(stdout,"server_handoff : remain sem->count : %d\n",sem->count);
  fflush(stdout);

  rc = pthread_create(&threads[sem->count - 1], NULL, serve_connection, (void*)sockfd);
  if (rc) {
    fprintf(stdout, "Error; return code from pthread_create() is %d\n", rc);
    fflush(stdout);
    exit (-1);
  } 
  //fprintf(stdout,"server_handoff : after create thread, end\n");
}

int check_prime(int n, int socket_id){
  if(n<=1) return 0;
  for(int i=2; i<=n/2;i++){
    if(n % i == 0) return 0;
  }
  return 1;
}

/* the main per-connection service loop of the server; assumes
   sockfd is a connected socket */
void*
serve_connection (void* void_sockfd) {
  fprintf(stdout,"serve_connection\n");
  int sockfd = (int)void_sockfd;
  ssize_t  n, result;

  char line[MAXLINE];
  connection_t conn;
  connection_init (&conn);
  conn.sockfd = sockfd;
  while (! shutting_down) {
    /* submit Task into TaskQueue */
    if ((n = readline (&conn, line, MAXLINE)) == 0) goto quit;
    printf("%s\n",line);

    
    /* connection closed by other end */
    if (shutting_down) goto quit;
    if (n < 0) {
      perror ("readline failed");
      goto quit;
    }
    
    int done = 0;
    Task t = {
      .taskFunction = &getRequest,
      .conn = &conn,
      .arg1 = &done,
      .arg2 = line,
      .socket_id = sockfd,
    };
    submitTask(t);
    
    pthread_mutex_lock(&mutex_for_task);

    while(done==0){
      if (shutting_down) goto quit;
      pthread_cond_wait(&taskCond, &mutex_for_task);
      if (shutting_down) {

        goto quit;
      }
    }
    pthread_mutex_unlock(&mutex_for_task);
  }
quit:
  c_sem_client_disconnect(&connection_thread_pool);
  fprintf(stdout,"quit : remain sem->count : %d\n",connection_thread_pool.count);
  CHECK (close (conn.sockfd));
}

/* set up socket to use in listening for connections */
void
open_listening_socket (int *listenfd) {
  struct sockaddr_in servaddr;
  const int server_port = 0; /* use ephemeral port number */
  socklen_t namelen;
  memset (&servaddr, 0, sizeof(struct sockaddr_in));
  servaddr.sin_family = AF_INET;
  /* htons translates host byte order to network byte order; ntohs
     translates network byte order to host byte order */
  servaddr.sin_addr.s_addr = htonl (INADDR_ANY);
  servaddr.sin_port = htons (server_port);
  /* create the socket */
  CHECK (*listenfd = socket(AF_INET, SOCK_STREAM, 0))
  /* bind it to the ephemeral port number */
  CHECK (bind (*listenfd, (struct sockaddr *) &servaddr, sizeof (servaddr)));
  /* extract the ephemeral port number, and put it out */
  namelen = sizeof (servaddr);
  CHECK (getsockname (*listenfd, (struct sockaddr *) &servaddr, &namelen));
  fprintf (stderr, "server using port %d\n", ntohs(servaddr.sin_port));
}

/* handler for SIGINT, the signal conventionally generated by the
   control-C key at a Unix console, to allow us to shut down
   gently rather than having the entire process killed abruptly. */ 
void
siginthandler (int sig, siginfo_t *info, void *ignored) {
  shutting_down = 1;
}

void
install_siginthandler () {
  struct sigaction act;
  /* get current action for SIGINT */
  CHECK (sigaction (SIGINT, NULL, &act));
  /* add our handler */
  act.sa_sigaction = siginthandler;
  /* update action for SIGINT */
  CHECK (sigaction (SIGINT, &act, NULL));
}

int
main (int argc, char **argv) {
  if(argc!=3){
    printf("argc : %d\t%s\n",argc,argv[2]);
    printf("Usage\n\t./multisrv -n <Number of Total Threads>\n");
    return 1;
  }
  if(strcmp(argv[1],"-n")){
    printf("argc : %d\t%s\n",argc,argv[2]);
    printf("Usage\n\t./multisrv -n <Number of Total Threads>\n");
    return 1;
  }

  int total_threads = atoi(argv[2]);
  if(total_threads <=1){
    printf("threads number is too small, plz enter number over 2.\n");
    return 1;
  }
  if(total_threads == 2){
    MAX_THREAD = 1;
    WORKER_THREAD = 1;  
  }else{
    MAX_THREAD = total_threads/3;
    WORKER_THREAD = total_threads - MAX_THREAD;
  }
  printf("number of acceptors : %d\tnumber of workers in pool : %d\n", MAX_THREAD, WORKER_THREAD);

  int connfd, listenfd;
  socklen_t clilen;
  struct sockaddr_in cliaddr;

  /* init */
  install_siginthandler();
  init_deque(&deque);

  threads = malloc(sizeof(pthread_t)*MAX_THREAD);
  pthread_t* threads_pool = malloc(sizeof(pthread_t)*WORKER_THREAD);
  pthread_cond_init(&condQueue,NULL);
  pthread_cond_init(&taskCond,NULL);
  pthread_mutex_init(&mutex_for_queue,NULL);
  pthread_mutex_init(&mutex_for_task,NULL);


  /* create thread in pool */
  for(int i=0;i<WORKER_THREAD;i++){
    if(pthread_create(&threads_pool[i], NULL, &startThread, NULL) !=0 ){
      perror("Failed to create t he thread");
    }
  }

  /* thread pool for connection with client */

  init_c_semaphore(&connection_thread_pool, NULL, MAX_THREAD);

  /////////////////////////////////////

  open_listening_socket (&listenfd);
  CHECK (listen (listenfd, 4));
  /* allow up to 4 queued connection requests before refusing */
  while (! shutting_down) {
    errno = 0;
    clilen = sizeof (cliaddr); /* length of address can vary, by protocol */
    if ((connfd = accept (listenfd, (struct sockaddr *) &cliaddr, &clilen)) < 0) {
      if (errno != EINTR) ERR_QUIT ("accept"); 
      /* otherwise try again, unless we are shutting down */
    } else {
      server_handoff (connfd, &connection_thread_pool, &threads); /* process the connection */
    }
  }

  pthread_cond_broadcast(&connection_thread_pool.cv);
  pthread_cond_broadcast(&taskCond);
  pthread_cond_broadcast(&condQueue);

  /* join threads */
  for(t=0;t<WORKER_THREAD;t++){
    rc =pthread_join(threads_pool[t],(void**)&status);
    if(rc){
       fprintf(stdout, "Error; return code from pthread_join() is %d\n", rc);
       fflush(stdout);
	     exit(-1);
    }
  }

  pthread_mutex_destroy(&mutex_for_queue);
  pthread_mutex_destroy(&mutex_for_task);
  pthread_cond_destroy(&condQueue);
  pthread_cond_destroy(&taskCond);

  free(threads);
  free(threads_pool);
  CHECK (close (listenfd));
  return 0;
}
