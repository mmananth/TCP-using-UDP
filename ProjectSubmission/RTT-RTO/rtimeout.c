//===================================================== file = rtimeout.c ====
//= program to compare with three TCP "retransmit time-out"(RTO) algorithms  =
//============================================================================
//=  Notes:                                                                  =
//=    1) The 3 algorithms used for comparison are:                          =
//=        [a] Average RTT algorithm                                         =
//=        [b] Old TCP RTO algorithm                                         =
//=        [c] Jacobson's algorithm (officially adopted for use of TCP )     =
//=    2) The value of parameters used in each algorithm are hard-coded      =
//=       in this program by using "#define" and can be easily modified      =
//=       to observe the function performed by these parameters.             =
//=    3) The initial value of SRTT and the constant used in algorithm [a]   =
//=        may also be changed to other reasonable value.                    =
//=--------------------------------------------------------------------------=
//=  Example input: rtt1.dat (a list of rtt time values )                    =
//=                                                                          =
//=       1.0                                                                =
//=       2.0                                                                =
//=       3.0                                                                =
//=       ...                                                                =
//=--------------------------------------------------------------------------=
//=  Example execution and output:                                           =
//=     rto  rtt1.dat  rto1.out                                              =
//=                                                                          =
//=  List content of "rto1.out"                                              =
//=                                                                          =
//=   #SEGMENT	 RTTi     RTO_AVE  RTO_TCP_OLD 	 RTO_TCP_NEW(V.J)	     =
//=      1	 1.00	   4.00	      2.88	     1.94                    =
//=      2	 2.00	   5.00	      3.02	     2.45                    =
//=     ...      ...        ...        ...            ...                    =
//=--------------------------------------------------------------------------=
//=  Build: gcc -o rtimeout rtimeout.c                                       =
//=--------------------------------------------------------------------------=
//=  Execute: rtimeout inputfile outputfile                                  =
//=          (rtt data should be in the input file)                          =
//=--------------------------------------------------------------------------=
//=  History: ZY/KJC (04/16/99) - Genesis                                    =
//============================================================================

//----- Include files --------------------------------------------------------
#include <stdio.h>           // Needed for printf() etc.
#include <math.h>            // Needed for fabs()

//----- Defines --------------------------------------------------------------
#define ALFA        0.875    // Constant factor for SRTT(K+1) in OLD TCP RTO
#define BETA        2        // The old TCP RTO constant factor "beta"
#define PARA_F      4        // The new TCP RTO constant factor "f"
#define PARA_G      0.125    // "g" value in Jacobson's algorithm
#define PARA_H      0.25     // "h" value in Jacobson's algorithm
#define CONST       1        // Constant in average RTT algorithm
#define SRTT_0      1.5      // Initial value of smoothed RTT
#define SDEV_0      0        // Initial value of smoothed deviation
//============================================================================
//=  Main program                                                            =
//============================================================================
void main(int argc, char *argv[]  )
{

  FILE             *fp_in ;                // Pointer for input file
  FILE             *fp_out;                // Pointer for output file

  int              counter=0;              // segment counter
  float            sum=0.0 ;               // Accumulator for rtt
  float            rto_ave;                // rto of ave rtt algorithm
  float            srtt_last, srtt_now;    // srtt(k)& srtt(k+1) - TCP_OLD
  float            rto_old_now;            // rto(k+1)           - TCP_OLD
  float            rto_jaco_now;           // rto(k+1)           - Jacobson
  float            srtt3_last, srtt3_now;  // srtt(k), srtt(k+1) - Jacobson
  float            serr_last, serr_now;    // serr(k), serr(k+1) - Jacobson
  float            sdev_last, sdev_now;    // sdev(k), sdev(k+1) - Jacobson
  float            rtt;                    // rtt value after atof()
  char             rttc[20];               // rtt string read from input file

  // Command line error detection
  if (argc!=3)
    {
      printf(" Usage: rtimeout  Inputfile  Outputfile .");
      exit(-1);
     }

  // Open input data file to read
  if (( fp_in=fopen(argv[1], "r"))==NULL)
     {
      printf("Error in opening data file : %s \n !", argv[1] );
      exit(-1);
      }

  // Open/create output file to write
  if (( fp_out=fopen(argv[2], "w+"))==NULL)
     {
      printf("Error in opening output file : %s \n !", argv[2] );
      exit(-1);
      }

fprintf(fp_out,"\t\t==== Result RTO of the three algorithms ====\t\n\n" );
fprintf(fp_out,"     #SEGMENT\t  RTTi\t\t RTO_AVE \t RTO_TCP_OLD \tRTO_TCP_Jaco\t\n" );

   while( !feof(fp_in))
     {
       fscanf(fp_in,"%s", rttc);

       rtt=atof(rttc);
       counter++;
       sum+=rtt;


   // Calculate RTO using average rtt algorithm
       rto_ave=BETA*(CONST+(sum/counter));

   // Calculate RTO using the old TCP formular based on SRTT
       if(counter==1){
       	     srtt_now = ALFA*SRTT_0+(1-ALFA)*rtt;
          rto_old_now = BETA*srtt_now;

          }
        else{
          srtt_now=ALFA*srtt_last+(1-ALFA)*rtt;
          rto_old_now=BETA*srtt_now;
          }
        srtt_last=srtt_now;

    // Calculate RTO using the Jacobson algorithm
       if(counter==1){
               srtt3_now = (1-PARA_G)*SRTT_0+PARA_G*rtt;
               serr_now = rtt - SRTT_0;
               sdev_now = (1-PARA_H)*SDEV_0+PARA_H*fabs(serr_now);
           rto_jaco_now = srtt3_now+PARA_F*sdev_now;
         }
       else{
               srtt3_now = (1-PARA_G)*srtt3_last+PARA_G*rtt;
               serr_now = rtt - srtt3_last;
               sdev_now = (1-PARA_H)*sdev_last+PARA_H*fabs(serr_now);
           rto_jaco_now = srtt3_now+PARA_F*sdev_now;
           }
               srtt3_last = srtt3_now;
                serr_last = serr_now;
                sdev_last = sdev_now;


    // Output the results to the file specified by the 3rd argument
    // in command line.

      fprintf(fp_out,"%9d",counter);
      fprintf(fp_out,"\t%7.2f",rtt);
      fprintf(fp_out,"\t\t%9.2f",rto_ave);
      fprintf(fp_out,"\t%9.2f",rto_old_now);
      fprintf(fp_out,"\t%9.2f\n",rto_jaco_now);

   }

    // Close all the files
      fclose(fp_in);
      fclose(fp_out);

 }
