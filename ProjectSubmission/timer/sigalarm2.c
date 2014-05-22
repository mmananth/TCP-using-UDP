/*
 * sigalarm2.c - test for timer functions (SIGALRM)
*/

#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <signal.h>

/* will give warning but disregard */
int i=0;
struct itimerval value, setvalue, ovalue;

/* handler called when timer returns */
void handler()
{

  if(i == 1)
    exit(1);

  /* Could do something here like send a packet */
  
  /* setup signal SIGALRM to call handler after time is up (5 seconds) */ 
  signal(SIGALRM, handler);
  setvalue.it_interval.tv_sec=0;
  setvalue.it_interval.tv_usec=0;
  setvalue.it_value.tv_sec=5;
  setvalue.it_value.tv_usec=0;
  
  /* initialize timer */
  setitimer(ITIMER_REAL, &setvalue, &ovalue);

  printf("handler() will be called in 5 sec!\n");
}


main(void)
{
  char ch;
  char junk;
  int k;

  /* Initiate timer */
  handler();
  
  while(1) {
  }
  
}


