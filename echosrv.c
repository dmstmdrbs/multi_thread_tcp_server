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
#include "echolib.h"
#include "checks.h"

/* 
  by seunggyun
  date : 2021/11/03
*/
int check_prime(int n, int length){
  int isPrime = 1;
  if(n==1){
    return 0;
  }
  for(int i=2; i<=n/2;i++){
    if(n%i==0){
      isPrime=0;
      break;
    }
  }
  if(isPrime==1) {
    return 1;
  }
  else{
   return 0;
  } 
}

void
serve_connection (int sockfd);

void
server_handoff (int sockfd) {
  serve_connection (sockfd);

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

/* the main per-connection service loop of the server; assumes
   sockfd is a connected socket */
void
serve_connection (int sockfd) {
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
    /* check the prime num */
    int isPrime = check_prime(atoi(line), strlen(line));

    if(isPrime==0){
      fprintf (stdout, "%d is not prime number\n",atoi(line));
      strcpy(line,"0\n"); 
    }else if(isPrime==1){ 
      fprintf (stdout, "%d is prime number\n",atoi(line)); 
      strcpy(line,"1\n");
    }
    n = strlen(line);
    result = writen (&conn, line, strlen(line));
    if (shutting_down) goto quit;
    if (result != n) {
      perror ("writen failed");
      goto quit;
    }
  }
quit:
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

int main (int argc, char **argv) {
  int connfd, listenfd;
  socklen_t clilen;
  struct sockaddr_in cliaddr;

  /* NOTE: To make this multi-threaded, You may need insert
     additional initialization code here, but you will not need to
     modify anything below here, though you are permitted to
     change anything in this file if you feel it is necessary for
     your design */

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
     server_handoff (connfd); /* process the connection */
    }
  }
  CHECK (close (listenfd));
  return 0;
}


