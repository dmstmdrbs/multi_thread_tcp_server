/* file: echocli.c

   Bare-bones TCP client with commmand-line argument to specify
   port number to use to connect to server.  Server hostname is
   specified by environment variable "SERVERHOST".

   This started out with an example in W. Richard Stevens' book
   "Advanced Programming in the Unix Environment".  I have
   modified it quite a bit, including changes to make use of my
   own re-entrant version of functions in echolib.
   
   Ted Baker
   February 2015

 */

#include "config.h"
#include "echolib.h"
#include "checks.h"
#include "pthread.h"
#include <sys/time.h>
#include <math.h>
#include <immintrin.h>
#include <x86intrin.h>

// Function to swap two numbers
void swap(char *x, char *y) {
  char t = *x; *x = *y; *y = t;
}
// Function to reverse `buffer[i…j]`
char* reverse(char *buffer, int i, int j)
{
  while (i < j) {
    swap(&buffer[i++], &buffer[j--]);
  }
  return buffer;
}
 
// Iterative function to implement `itoa()` function in C
char* itoa(int value, char* buffer, int base)
{
  // invalid input
  if (base < 2 || base > 32) {
    return buffer;
  }
  // consider the absolute value of the number
  int n = abs(value);

  int i = 0;
  while (n)
  {
    int r = n % base;
    if (r >= 10) {
      buffer[i++] = 65 + (r - 10);
    }
    else {
      buffer[i++] = 48 + r;
    }
    n = n / base;
  }

  // if the number is 0
  if (i == 0) {
    buffer[i++] = '0';
  }
  // If the base is 10 and the value is negative, the resulting string
  // is preceded with a minus sign (-)
  // With any other base, value is always considered unsigned
  if (value < 0 && base == 10) {
    buffer[i++] = '-';
  }
  buffer[i++] = '\n';
  buffer[i] = '\0'; // null terminate string

  // reverse the string and return it
  return reverse(buffer, 0, i - 2);
}

void*
receive_thread_work (void* void_conn) {
  unsigned long start, end;
  char recvline[MAXLINE];
  connection_t *con = (connection_t*)void_conn;
  start = __rdtsc();
  for (int i=0; i<con->line_len; i++) { // same as for(i=0; i<N; i++)
    // Each response from server
    if (readline (con, recvline, sizeof(recvline)) <= 0){
      ERR_QUIT ("str_cli: server terminated connection prematurely");
    }
    fprintf (stdout, "[%d번째] %s",i, recvline);
    fflush (stdout);
  } // end of for
  end = __rdtsc();
  printf("time : %ld\n",end-start);
}

/* the main service loop of the client; assumes sockfd is a
  connected socket */
void
client_work (int sockfd) {
/* variables */
  // connection
  connection_t conn;
  char *p;
  char sendline[MAXLINE];

  // thread
  pthread_t rcv_thread;
  int rc;
  long status;

  char itoa_buffer[MAXLINE];
  
/* logic */
  connection_init (&conn);
  conn.sockfd = sockfd;

  while ((p = fgets (sendline, sizeof (sendline), stdin))) {
    if(shutting_down){
      printf("server shutting down\n");
      break;
    }
    conn.line_len = atoi(sendline); // input N (# of random numbers)

    // crate receive thread ; readline()
    rc = pthread_create(&rcv_thread, NULL, receive_thread_work, (void*)&conn);
  
    if (rc) {
      fprintf(stdout, "Error; return code from pthread_create() is %d\n", rc);
      fflush(stdout);
      exit (-1);
    } 

    // send ; writen()
    for(int i=0; i<conn.line_len; i++) {
      if(i%2==0) itoa(rand() % 1000000000 + 1000000000, itoa_buffer, 10);
      else itoa(1,itoa_buffer,10);
      printf("[%d번째] send :%s",i, itoa_buffer);
      usleep(15000);
      CHECK (writen (&conn, itoa_buffer, strlen(itoa_buffer)));
    }

    // join thread
    rc = pthread_join(rcv_thread, (void**)&status);
    if (rc) {
      fprintf(stdout, "Error; return code from pthread_join() is %d\n", rc);
      fflush(stdout);
      exit(-1);
    }
  } // end of while
  /* null pointer returned by fgets indicates EOF */
}

/* fetch server port number from main program argument list */
int
get_server_port (int argc, char **argv) {
  int val;
  char * endptr;
  if (argc != 2) goto fail;
  errno = 0;
  val = (int) strtol (argv [1], &endptr, 10);
  if (*endptr) goto fail;
  if ((val < 0) || (val > 0xffff)) goto fail;
#ifdef DEBUG
  fprintf (stderr, "port number = %d\n", val);
#endif
  return val;
fail:
   fprintf (stderr, "usage: echosrv [port number]\n");
   exit (-1);
}

/* set up IP address of host, using DNS lookup based on SERVERHOST
   environment variable, and port number provided in main program
   argument list. */
void
set_server_address (struct sockaddr_in *servaddr, int argc, char **argv) {
  struct hostent *hosts;
  char *server;
  const int server_port = get_server_port (argc, argv);
  if ( !(server = getenv ("SERVERHOST"))) {
    QUIT ("usage: SERVERHOST undefined.  Set it to name of server host, and export it.");
  }
  memset (servaddr, 0, sizeof(struct sockaddr_in));
  servaddr->sin_family = AF_INET;
  servaddr->sin_port = htons (server_port);
  if ( !(hosts = gethostbyname (server))) {
    ERR_QUIT ("usage: gethostbyname call failed");
  }
  servaddr->sin_addr = *(struct in_addr *) (hosts->h_addr_list[0]);
}

int
main (int argc, char **argv) {
  int sockfd;
  struct sockaddr_in servaddr;
  struct timeval start, stop;
  /* time how long we have to wait for a connection */
  CHECK (gettimeofday (&start, NULL));
  set_server_address (&servaddr, argc, argv);
  if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
    ERR_QUIT ("usage: socket call failed");
  }
  CHECK (connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr)));
  CHECK (gettimeofday (&stop, NULL));
  fprintf (stderr, "connection wait time = %ld microseconds\n",
          (stop.tv_sec - start.tv_sec)*1000000 + (stop.tv_usec - start.tv_usec));
  client_work (sockfd);
  exit (0);
}