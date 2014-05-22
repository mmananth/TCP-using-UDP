/*
 * sigalarm.c - test for timer functions (SIGALRM)
 */

#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <signal.h>

/* will give warning but disregard */
extern int i=0;

/* Job called when timer returns */
void job2()
{
  printf("time up!\n");
  i=1;
}


main(void)
{
  struct itimerval value, setvalue, ovalue;
  char ch;
  char junk;
  
  
  printf("Setting timer for 5 seconds\n");

  /* setup signal SIGALRM to call job2 after time is up (5 seconds) */ 

  signal(SIGALRM, job2);
  setvalue.it_interval.tv_sec=0;
  setvalue.it_interval.tv_usec=0;
  setvalue.it_value.tv_sec=5;
  setvalue.it_value.tv_usec=0;
  
  /* initialize timer */
  setitimer(ITIMER_REAL, &setvalue, &ovalue);

  while(1) {                                             
    /* get current time and place in value (structure) */
    getitimer(ITIMER_REAL, &value); 
 
    /* Limit the time that the statement will print.
     * This will prevent printf from printing all the
     * way down the screen since tv_sec will be 4 for 
     * a long time.  Specifically this test will only 
     * be true if the timer is in the range 4sec+999900 micro-sec
     * through 4sec+999msec */
    if(value.it_value.tv_sec==4 && value.it_value.tv_usec > 999900){
      /* Usually prints once but not always.  This 
       * routine is not necessarily practical but only
       * for demonstration of signal timer functions. */
      printf("doing some work here\n");
    }
  
    else if(value.it_value.tv_sec==0 && i==1){
     
      printf("Press 's' to reset timer for another 5 seconds; any other key to quit; hit enter after typing the character\n");
      fflush(stdout);
      i=0;
      ch=getchar();

      /* Removes the any access line feeds or charage 
       * returns */
      if(ch=='\n' || ch=='\r')
	ch=getchar();

      if(ch=='s'){
	/* Reset timer */
	signal(SIGALRM, job2);
	
	/* Actually not necessary to reinitalize variables
	 * set to '0' since are already '0' */
	setvalue.it_interval.tv_sec=0;
	setvalue.it_interval.tv_usec=0;
	setvalue.it_value.tv_sec=5;
	setvalue.it_value.tv_usec=0;
	
	printf("Re-starting timer for 5 seconds\n");

	/* Initialize timer */
	setitimer(ITIMER_REAL, &setvalue, &ovalue);	
	
      }
      /* Exit program */
      else
	exit(1);
    }
    
  } 
}
