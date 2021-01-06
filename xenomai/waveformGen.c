#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <math.h>

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

/*
 * Task attributes 
 */ 
#define TASK_MODE 0  	// No flags
#define TASK_STKSZ 0 	// Default stack size

#define TASK_A_PRIO 20 	// RT priority [0..99]
#define TASK_A_PERIOD_NS 200*1000*1000 // Task period (in ns)

#define TASK_B_PRIO 20 	// priority
#define TASK_B_PERIOD_NS 100*1000*1000 // Task period (in ns)

#define TASK_LOAD_NS      10*1000*1000 // Task execution time (in ns, same to all tasks)

#define SAMPLE 40

RT_TASK task_a_desc; // Task decriptor
RT_TASK task_b_desc; // Task decriptor

// Semaphore

//#define SEM_INIT 1       /* Initial semaphore count */
//#define SEM_MODE S_FIFO  /* Wait by FIFO order */

//RT_SEM sem_desc;

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

/*
* Simulates the computational load of tasks
*/ 
void simulate_load(RTIME load_ns) {
	RTIME ti, tf;
	
	ti=rt_timer_read(); // Get initial time
	tf=ti+load_ns;      // Compute end time
	while(rt_timer_read() < tf); // Busy wait

	return;
}

float amplitude = 0; // 0 to 3.3 V resolution of 0.1 V
unsigned long frequency = 1;   // 1 to 1k Hz resolution of 1 Hz
char waveform = 's'; // s(sin) t(triangular) q(quadrada)
char changed = 'y';

/*
* Task body implementation
*/
void task_read_values(void *args) {
	RT_TASK *curtask;
	RT_TASK_INFO curtaskinfo;
	struct taskArgsStruct *taskArgs;

	RTIME to=0,ta=0;
	unsigned long overruns;
	int err;
	
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
		printf("%s activation\n", curtaskinfo.name);
		if(to!=0) 
			printf("Measured period (ns)= %lu\n",(long)(ta-to));
		to=ta;
		
		/* Task "load" */
		//simulate_load(TASK_LOAD_NS);

        // Read from shared memory or w/e
        //rt_sem_p(&sem_desc,TM_INFINITE);

        amplitude = amplitude+1;
        frequency = frequency+1;
        waveform = 'q';
        changed='y';

        //rt_sem_v(&sem_desc);
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
		printf("%s activation\n", curtaskinfo.name);
		if(to!=0) 
			printf("Measured period (ns)= %lu\n",(long)(ta-to));
		to=ta;
		
		/* Task "load" */
		//simulate_load(TASK_LOAD_NS);

        //Generate waveform
        //rt_sem_p(&sem_desc,TM_INFINITE);

        printf("Amplitude %f, Frequency %lu, Waveform %c\n",amplitude,frequency,waveform);

        if(changed=='y'){
             switch(waveform){
                case 's':
                    for(int i = 0; i < SAMPLE ; i++){
                        wave[i]= amplitude * sin(i*(2*M_PI/SAMPLE));
                        //printf("Sin waveform: %f.\n",wave[i]);
                    }
                    break;
                case 't':
                    for(int i = 0; i < SAMPLE ; i++){
                        int inst = SAMPLE/4;
                        if(i<inst){
                            wave[i]= i*amplitude/inst;
                        }
                        else if(i<3*inst){
                            wave[i]= amplitude-(i-inst)*amplitude/inst;
                        }
                        else{
                            wave[i]= (i-3*inst)*amplitude/inst - amplitude;
                        }
                        printf("Tri waveform: %f.\n",wave[i]);
                    }
                    break;
                case 'q':
                    for(int i = 0; i < SAMPLE ; i++){
                        int inst = SAMPLE/2;
                        if(i<inst){
                            wave[i]= amplitude;
                        }
                        else{
                            wave[i]= -amplitude;
                        }
                        printf("Qua waveform: %f.\n",wave[i]);
                    }
                    break;
                default:
                    printf("Bad waveform.\n");
                    break;
            }     
        }

        //rt_sem_v(&sem_desc);  

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
		
    //err = rt_sem_create(&sem_desc,"MySemaphore",SEM_INIT,SEM_MODE);

	/* Start RT task */
	/* Args: task decriptor, address of function/implementation and argument*/
	taskAArgs.taskPeriod_ns = TASK_A_PERIOD_NS; 
	taskBArgs.taskPeriod_ns = TASK_B_PERIOD_NS; 
    	rt_task_start(&task_a_desc, &task_read_values, (void *)&taskAArgs);
    	rt_task_start(&task_b_desc, &task_generate_waveform, (void *)&taskBArgs);

	/* wait for termination signal */	
	wait_for_ctrl_c();
    
    //rt_sem_delete(&sem_desc);

	return 0;
		
}


