/* 
 * power-instrument.c.
 *
 * power-instrument.c
 * - Compile your program with -finstrument-functions and link 
 *   together with this code.
 * - Logging is enabled as soon as your program calls
 *   cygprofile_enable() and disabled with cygprofile_disable().
 * - Before logging was enabled you can change the name 
 *   of a logfile by calling cygprofile_setfilename().
 */

/* Hint: -finstrument-functions, no_instrument_function */

#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <sys/time.h>

#include "cyg-profile.h"
#include "rnet_pm_api.h" //@
//#include "RNET_Interface_Host.h"

#ifndef FN_DEFAULT
#define FN_DEFAULT "powerlog.%d"
#endif

#define FN_SIZE 100
#define CONF_FILE "2_conf.rnp"

/* power parameters */
#define MAX_RESOLUTION 4000
#define MAX_MEASURES	10000
#define MAX_CHANELS	64
#define MAX_CALL_STACK	1000000
#define MAX_LEVEL	64

struct CallInfo
{
	void *this_fn;
	void *call_site;
} callInfo[MAX_MEASURES] = {0};

static int level=0;
static FILE *logfile=NULL;
static int cyg_profile_enabled=0;
static char cyg_profile_filename[FN_SIZE+1];

static handle_t daqh;
static int tags_measured, current_tag = 0;
static tag_handle_t tag_handle[MAX_MEASURES];
static tag_handle_t tag_stack[MAX_MEASURES];
static long long call_stack[MAX_CALL_STACK];
static long long callID = 0;
static int call_stack_pointer = 0;

static double power[MAX_CHANELS],energy[MAX_CHANELS],c_power[MAX_CHANELS],c_energy[MAX_CHANELS];
static struct timeval tv1,tv2;
static float tv_diff;
static int component = -1,comp_start,comp_end,num_components,comp;
////////////////////////

#ifdef __cplusplus
extern "C" {
#endif

/* Static functions. */
static FILE *openlogfile (const char *filename)
	__attribute__ ((no_instrument_function));
static void closelogfile (void)
	__attribute__ ((no_instrument_function));

/* Note that these are linked internally by the compiler. 
 * Don't call them directly! */
void __cyg_profile_func_enter (void *this_fn, void *call_site)
	__attribute__ ((no_instrument_function));
void __cyg_profile_func_exit (void *this_fn, void *call_site)
	__attribute__ ((no_instrument_function));
	
void __constructor( void )
	__attribute__ ((constructor, no_instrument_function));

void __destructor( void )
	__attribute__ ((destructor, no_instrument_function));

#ifdef __cplusplus
};
#endif

void __constructor( void )
{
	cygprofile_enable();
	int numdevs = rnet_pm_init(); //@
	if(numdevs <=0){
		printf("No active devices found!\n");
		exit(0);
	}
	else printf("Found %d devices, connecting to #0\n",numdevs);

	daqh = power_init(0,CONF_FILE); //@

	printf("Power initialization done, handle %lx...\n",daqh);
	
	num_components = 31;//@

	power_start_capture(daqh); //@
	power_start_task(daqh,0); //@
	power_start_task(daqh,1); //@

	sleep(1);
  	gettimeofday(&tv1,NULL);
}


void __destructor( void )
{
	sleep(1);
	
	printf("****** Stopping Task *********)\n");
	power_stop_task(daqh,1);
	power_stop_task(daqh,0);	
	power_stop_capture(daqh);
	
    gettimeofday(&tv2,NULL);
    tv_diff = (tv2.tv_sec - tv1.tv_sec) + (float)(tv2.tv_usec - tv1.tv_usec)/1000000;
	
	sleep(2);
	
	//printf("Rank %d starting post processing ...\n",rank);
    /*for(i=0;i<TRIES;i++){

		pow = power_get_channel_data(daqh,0,1,tags[i],OP_AVG,NULL,NULL);	
		printf("Rank %d, Power of channel 1, measurement %d: %f\n",rank,i,pow);

		//energy = power_get_channel_data(daqh,0,3,tags[i],OP_AVG,NULL,NULL);
        	//printf("Rank %d, Energy of channel 3, measurement %d: %f\n",rank,i,energy);
     }*/
	
	/*Read back the results - the average of all power samples for all channels during each measurement period*/
	
	printf("Measurement took %f sec.\n",tv_diff);
	
	int i=0;
	for(i = 0;i < tags_measured; i++){
		power[i] = 0; //power_get_aggregate_power(daq_handle,tag_handle[i]);
		energy[i] = 0; //power_get_aggregate_energy(daq_handle,tag_handle[i]);
		
		//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
		fprintf(logfile,"function %p call_from %p TOTAL * %f W %f J.\n", callInfo[i].this_fn, callInfo[i].call_site,power[i],energy[i]);
		//printf("function %p call_from %p TOTAL * %f W %f J.\n", callInfo[i].this_fn, callInfo[i].call_site,power[i],energy[i]);
		//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
		

		int comp_index[]={1, 3, 5, 7, 9, 11,12,13,14,15,16,17,18,19, 22, 24,25, 27,28,29,30,31};
		//Single component results
		/*if(component >=0){
			comp_start = component;
			comp_end = component;
		}else{
			comp_start = 1;
			comp_end = num_components;
		}*/
		for(comp = 0; comp <22; comp++){ //22 is the sife of comp_index
			c_power[i] = power_get_channel_data(daqh,0,comp_index[comp],tag_handle[i],OP_AVG,NULL,NULL);	//power_get_component_power(daq_handle,comp,tag_handle[i]);
			c_energy[i] = power_get_channel_data(daqh,1,comp_index[comp],tag_handle[i],OP_SUM,NULL,NULL);  //power_get_component_energy(daq_handle,comp,tag_handle[i]);
			fprintf(logfile,"function %p call_from %p component: %d %f W %f J.\n", callInfo[i].this_fn, callInfo[i].call_site,comp_index[comp],c_power[i],c_energy[i]);
			//printf("function %p call_from %p component: %d %f W %f J.\n", callInfo[i].this_fn, callInfo[i].call_site,comp,c_power[i],c_energy[i]);
			//printf("Component %d power measurement %d is %f Watts, energy is %f Joules.\n",comp,i,c_power[i],c_energy[i]);
		}
	  }	
 	 
	power_finalize(daqh);
	rnet_pm_finalize();	
}	

void
__cyg_profile_func_enter (void *this_fn, void *call_site)
{
	//printf("enter function\n");
	if( (level < MAX_LEVEL) && cyg_profile_enabled )
	{
		call_stack[call_stack_pointer++] = ( (callID < 2000000000)? (callID++) : callID );
		
		if(tags_measured < MAX_MEASURES)
     	{
			callInfo[tags_measured].this_fn = this_fn;
         	callInfo[tags_measured].call_site = call_site;         	
         	
			tag_stack[current_tag] = power_start_measure(daqh,0); //resolution, the second argument is currently ignored
         	
         	tag_handle[tags_measured] = tag_stack[current_tag];         	
         	//printf("start measuring for tag# %d \n", tag_stack[current_tag]);         	         	         	
         	current_tag++;
         	tags_measured++;
		}
	}
	
	level++;
}

void
__cyg_profile_func_exit (void *this_fn, void *call_site)
{
	//printf("exit function\n");
	level--;
	
	if( (level < MAX_LEVEL) && cyg_profile_enabled )
	{			
		call_stack_pointer--; 
		
		if( call_stack[call_stack_pointer] < MAX_MEASURES ) //@
     	{
     		current_tag--;        	
  			power_end_measure(daqh,tag_stack[current_tag]);
  			
  			//printf("stop measuring for tag# %d \n", tag_stack[current_tag]);  			  			
		}
		
	}
}

void
cygprofile_enable (void)
{
	if (!cyg_profile_filename[0])
		cygprofile_setfilename (FN_DEFAULT);
	if (!openlogfile (cyg_profile_filename))
		return;
	cyg_profile_enabled = 1;
}

void
cygprofile_disable (void)
{
	cyg_profile_enabled = 0;
}

int
cygprofile_isenabled (void)
{ return cyg_profile_enabled; }

int 
cygprofile_setfilename (const char *filename)
{
	const char *ptr;

	if (cygprofile_isenabled ())
		return -1;

	if (strlen (filename) > FN_SIZE)
		return -2;

	ptr = strstr (filename, "%d");
	if (ptr)
	{
		size_t len;
		len = ptr - filename;
		snprintf (cyg_profile_filename, len+1, "%s", filename);
		snprintf (&cyg_profile_filename[len], FN_SIZE - len, 
			"%d", getpid ());
		len = strlen (cyg_profile_filename);
		snprintf (&cyg_profile_filename[len], FN_SIZE - len,
			"%s", ptr + 2);
	}
	else
		snprintf (cyg_profile_filename, FN_SIZE, "%s", filename);

	if (logfile)
		closelogfile ();

	return 0;
}

char *
cygprofile_getfilename (void)
{
	if (!cyg_profile_filename[0])
		cygprofile_setfilename (FN_DEFAULT);
	return cyg_profile_filename;
}

static FILE *
openlogfile (const char *filename)
{
	static int complained = 0;
	FILE *file;
	
	if (complained)
		return NULL;
	
	if (logfile)
		return logfile;

	file = fopen(filename, "w");
	if (!file)
	{
		fprintf (stderr, "WARNING: Can't open logfile '%s': %s\n", 
			filename, strerror (errno));
		complained = 1;
		return NULL;
	}
	
	setlinebuf (file);
	logfile = file;

	return file;
}

static void
closelogfile (void)
{
	if (logfile)
		fclose (logfile);
}
