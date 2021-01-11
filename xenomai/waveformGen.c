#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <math.h>
#include <sys/ipc.h> 
#include <sys/msg.h> 

#include <sys/mman.h> // For mlockall

// Xenomai API (former Native API)
#include <alchemy/task.h>
#include <alchemy/timer.h>
#include <alchemy/sem.h>


/* 
 * Define task structure for setting input arguments
 */
 struct taskArgsStruct {
	 RTIME taskPeriod_ns;
	 int some_other_arg;
 };

// structure for message queue 
struct {
	int freq;
	float amp;
	char form;
	int trigger;
	int transition;
} wave_data;

/*
 * Task attributes 
 */ 
#define TASK_MODE 0  	// No flags
#define TASK_STKSZ 0 	// Default stack size

#define TASK_A_PRIO 20 	// RT priority [0..99]
#define TASK_A_PERIOD_NS 1000*1000*1000 // Task period (in ns)

#define TASK_B_PRIO 20 	// priority
#define TASK_B_PERIOD_NS 500*1000*1000 // Task period (in ns)

#define TASK_LOAD_NS      10*1000*1000 // Task execution time (in ns, same to all tasks)

#define SAMPLE 40

#define MAX 10 

RT_TASK task_a_desc; // Task decriptor
RT_TASK task_b_desc; // Task decriptor

/*
* Catches CTRL + C to allow a controlled termination of the application
*/
void catch_signal(int sig) {}

void wait_for_ctrl_c(void) {
	signal(SIGTERM, catch_signal); //catch_signal is called if SIGTERM received
	signal(SIGINT, catch_signal);  //catch_signal is called if SIGINT received

	// Wait for CTRL+C or sigterm
	pause();
	
	// Will terminate
	printf("Terminating ...\n");
}

char changed = 'y';

void task_read_values(void *args) {

	RT_TASK *curtask;
	RT_TASK_INFO curtaskinfo;
	struct taskArgsStruct *taskArgs;

	RTIME to=0,ta=0;
	unsigned long overruns;
	int err;
	
	int prevFreq = wave_data.freq;
	float prevAmp = wave_data.amp;
	char prevForm = wave_data.form;
	int prevTrigger = wave_data.trigger;
	int prevTransition = wave_data.transition;

	/* Get task information */
	curtask=rt_task_self();
	rt_task_inquire(curtask,&curtaskinfo);
	taskArgs=(struct taskArgsStruct *)args;
	printf("Task %s init, period:%llu\n", curtaskinfo.name, taskArgs->taskPeriod_ns);
		
	/* Set task as periodic */
	err=rt_task_set_periodic(NULL, TM_NOW, taskArgs->taskPeriod_ns);
	for(;;) {
		err=rt_task_wait_period(&overruns);
		ta=rt_timer_read();
		if(err) {
			printf("%s overrun!!!\n", curtaskinfo.name);
			break;
		}
		//printf("%s activation\n", curtaskinfo.name);
		if(to!=0) 
			//printf("Measured period (ns)= %lu\n",(long)(ta-to));
		to=ta;
        
        key_t key; 
        int msgid; 
      
        // ftok to generate unique key 
        key = ftok("progfile", 65); 
      
        // msgget creates a message queue 
        // and returns identifier 
        msgid = msgget(key, 0666 | IPC_CREAT); 
      
        // msgrcv to receive message 
        msgrcv(msgid, &wave_data, sizeof(wave_data), 0, IPC_NOWAIT); 
      
        // display the message 

        if(wave_data.freq!=prevFreq || wave_data.amp!=prevAmp || wave_data.form!=prevForm || wave_data.trigger!=prevTrigger || wave_data.transition!=prevTransition){

            printf("New input received.\n");

            printf("Frequency received is : %d \n",wave_data.freq); 
            printf("Amplitude received is : %f \n",wave_data.amp);
            printf("Waveform received is : %c \n",wave_data.form);
            printf("Trigger received is : %d \n",wave_data.trigger);
            printf("Transition received is : %d \n",wave_data.transition);
          
        	prevFreq = wave_data.freq;
	        prevAmp = wave_data.amp;
	        prevForm = wave_data.form;
	        prevTrigger = wave_data.trigger;
	        prevTransition = wave_data.transition;

            changed='y';
        }
    
        // to destroy the message queue 
        msgctl(msgid, IPC_RMID, NULL); 

	}
	return;
}

void task_generate_waveform(void *args) {
	RT_TASK *curtask;
	RT_TASK_INFO curtaskinfo;
	struct taskArgsStruct *taskArgs;

	RTIME to=0,ta=0;
	unsigned long overruns;
	int err;
  
    int frequency = wave_data.freq;
    float amplitude = wave_data.amp;
    char waveform = wave_data.form;
    int trigger = wave_data.trigger;
    int transition = wave_data.transition;

    float wave[SAMPLE];
	
	/* Get task information */
	curtask=rt_task_self();
	rt_task_inquire(curtask,&curtaskinfo);
	taskArgs=(struct taskArgsStruct *)args;
	printf("Task %s init, period:%llu\n", curtaskinfo.name, taskArgs->taskPeriod_ns);
		
	/* Set task as periodic */
	err=rt_task_set_periodic(NULL, TM_NOW, taskArgs->taskPeriod_ns);
	for(;;) {
		err=rt_task_wait_period(&overruns);
		ta=rt_timer_read();
		if(err) {
			printf("%s overrun!!!\n", curtaskinfo.name);
			break;
		}
		//printf("%s activation\n", curtaskinfo.name);
		if(to!=0) 
			//printf("Measured period (ns)= %lu\n",(long)(ta-to));
		to=ta;



        if(changed=='y'){

            frequency = wave_data.freq;
	        amplitude = wave_data.amp;
	        waveform = wave_data.form;
	        trigger = wave_data.trigger;
	        transition = wave_data.transition;

        }

        if(changed=='y'){
             printf("Amplitude %f, Frequency %d, Waveform %c\n",amplitude,frequency,waveform);
             switch(waveform){
                case 's':
                    for(int i = 0; i < SAMPLE ; i++){
                        wave[i]= amplitude/2 + amplitude/2 * sin(i*(2*M_PI/SAMPLE));
                        //printf("Sin waveform: %f.\n",wave[i]);
                    }
                    break;
                case 't':
                    for(int i = 0; i < SAMPLE ; i++){
                        if(i<SAMPLE/2){
                            wave[i]= i*amplitude/(SAMPLE/2);
                        }
                        else{
                            wave[i]= amplitude-(i-SAMPLE/2)*amplitude/(SAMPLE/2);
                        }
                        //printf("Tri waveform: %f.\n",wave[i]);
                    }
                    break;
                case 'q':
                    for(int i = 0; i < SAMPLE ; i++){
                        if(i<SAMPLE/2){
                            wave[i]= amplitude;
                        }
                        else{
                            wave[i]= 0;
                        }
                        //printf("Qua waveform: %f\n",wave[i]);
                    }
                    break;
                default:
                    printf("Bad waveform.\n");
                    break;
            }
            changed='n';     
        }
	}
	return;
}


/*
* Main function
*/ 
int main(int argc, char *argv[]) {
	int err; 
	struct taskArgsStruct taskAArgs,taskBArgs;
	
	/* Lock memory to prevent paging */
	mlockall(MCL_CURRENT|MCL_FUTURE); 

	/* Create RT task */
	/* Args: descriptor, name, stack size, prioritry [0..99] and mode (flags for CPU, FPU, joinable ...) */
	err=rt_task_create(&task_a_desc, "Task a", TASK_STKSZ, TASK_A_PRIO, TASK_MODE);
	if(err) {
		printf("Error creating task a (error code = %d)\n",err);
		return err;
	} else 
		printf("Task a created successfully\n");
	
	err=rt_task_create(&task_b_desc, "Task b", TASK_STKSZ, TASK_B_PRIO, TASK_MODE);
	if(err) {
		printf("Error creating task b (error code = %d)\n",err);
		return err;

	} else 
		printf("Task b created successfully\n");

	/* Start RT task */
	/* Args: task decriptor, address of function/implementation and argument*/
	taskAArgs.taskPeriod_ns = TASK_A_PERIOD_NS; 
	taskBArgs.taskPeriod_ns = TASK_B_PERIOD_NS; 

	rt_task_start(&task_a_desc, &task_read_values, (void *)&taskAArgs);
	rt_task_start(&task_b_desc, &task_generate_waveform, (void *)&taskBArgs);

	/* wait for termination signal */	
	wait_for_ctrl_c();
    
    rt_task_delete(&task_a_desc);
    rt_task_delete(&task_b_desc);

	return 0;
		
}


