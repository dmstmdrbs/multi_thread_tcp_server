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

#define MAX_THREAD 4

typedef struct c_semaphore{
  int count;
  pthread_mutex_t mutex;
  pthread_cond_t cv;
} c_semaphore;

struct c_semaphore connection_thread_pool;

int init_c_semaphore(c_semaphore* sem, int pshared, int value){
  if(pshared) { errno = ENOSYS; return -1;}
  sem->count = value;
  pthread_mutex_init(&sem->mutex, NULL);
  pthread_cond_init(&sem->cv,NULL);
}

void c_sem_client_disconnect(c_semaphore* sem){
  pthread_mutex_lock(&sem->mutex);
  sem->count++;
  if(sem->count==1){
    /* to notice with cv for waiting semaphore */
    pthread_mutex_unlock(&sem->mutex);
    pthread_cond_signal(&sem->cv);
  }else{
    pthread_mutex_unlock(&sem->mutex);
  }
}

void c_sem_client_wait_to_connect(c_semaphore* sem){
  pthread_mutex_lock(&sem->mutex);
  while(sem->count==0){
    /* unlcok mutex and waits signal in sleep state */
    pthread_cond_wait(&sem->cv,&sem->mutex);
  }
  sem->count--;
  pthread_mutex_unlock(&sem->mutex);
}

void*
serve_connection (void* sockfd);

void* test(void* sockfd){
  fprintf(stdout, "test function : %d\n",(int)sockfd);
}
void
server_handoff (int sockfd, c_semaphore* sem, pthread_t* threads) {
  c_sem_client_wait_to_connect(sem);
  //serve_connection (sockfd);
  /* create threads */
  int rc;
  fprintf(stdout,"remain sem->count : %d\n",sem->count);
  fflush(stdout);
  rc = pthread_create(&threads[sem->count - 1], NULL, serve_connection, (void*)sockfd);
  
  if (rc) {
    fprintf(stdout, "Error; return code from pthread_create() is %d\n", rc);
    fflush(stdout);
    exit (-1);
  } 
/* NOTE: You will need to completely rewrite this function, so
   that it hands off the connection to one of your server threads,
   moving the call to serve_connection() into the server thread
   body.  To do that, you will need to declare a lot of new stuff,
   including thread, mutex, and condition variable structures,
   functions, etc.  You can insert the new bode before and after
   this function, but you do not need to modify serve_connection()
   or anything else between it and the note in the main program
   body.  However, you are free to change anything in this file if
   you feel it is necessary for your design. */
  
}

int check_prime(int n){
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
    if ((n = readline (&conn, line, MAXLINE)) == 0) goto quit;
    /* connection closed by other end */
    if (shutting_down) goto quit;
    if (n < 0) {
      perror ("readline failed");
      goto quit;
    }

  ///////////////////////////////////////////
  /* check if a given number (line) is prime */
    int is_prime = check_prime(atoi(line));

    if(is_prime) strcpy(line, "1\n");
    else strcpy(line, "0\n");
    n = 2; // strlen(line) == 2
  ///////////////////////////////////////////

    result = writen (&conn, line, n);
    if (shutting_down) goto quit;
    if (result != n) {
      perror ("writen failed");
      goto quit;
    }
  }
quit:
  c_sem_client_disconnect(&connection_thread_pool);
  fprintf(stdout,"remain sem->count : %d\n",connection_thread_pool.count);
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
  int connfd, listenfd;
  socklen_t clilen;
  struct sockaddr_in cliaddr;

  /////////////////////////////////////
  /* NOTE: To make this multi-threaded, You may need insert
     additional initialization code here, but you will not need to
     modify anything below here, though you are permitted to
     change anything in this file if you feel it is necessary for
     your design */

  /* init new local variable */
  /* TODO : Init new data structure for threads list */
  pthread_t threads[MAX_THREAD];
  int rc;
  long t = 0;
  long status;
  /* thread pool for connection with client */

  init_c_semaphore(&connection_thread_pool, NULL, MAX_THREAD);
  /////////////////////////////////////

  install_siginthandler();
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
      
      /* TODO : Select thread from threads */
      server_handoff (connfd, &connection_thread_pool, &threads); /* process the connection */
      /////////////////////////////////////
      /////////////////////////////////////
    }
  }

  /////////////////////////////////////
  /* join threads */
  for(t=0; t<MAX_THREAD; t++){
  	rc = pthread_join(threads[t], (void**)&status);

    if (rc) {
      fprintf(stdout, "Error; return code from pthread_join() is %d\n", rc);
      fflush(stdout);
	    exit(-1);
    }
  }
  /////////////////////////////////////


  CHECK (close (listenfd));
  return 0;
}
