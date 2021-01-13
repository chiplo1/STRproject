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

// UART headers
#include <fcntl.h> // Contains file controls like O_RDWR
#include <errno.h> // Error integer and strerror() function
#include <termios.h> // Contains POSIX terminal control definitions

/* 
 * Define task structure for setting input arguments
 */
 struct taskArgsStruct {
	 RTIME taskPeriod_ns;
	 int some_other_arg;
 };

// structure for message queue 
struct {
    long int mtype;
	int freq;
	float amp;
	char form;
	int trigger;
	int transition;
} wave_data;

struct {
    long int mtype;
	int flag;
} wave_flag;

/*
static const struct rtser_config my_config = {
	.config_mask       = 0xFFFF,
	.baud_rate         = 115200,
	.parity            = RTSER_DEF_PARITY,
	.data_bits         = RTSER_DEF_BITS,
	.stop_bits         = RTSER_DEF_STOPB,
	.handshake         = RTSER_DEF_HAND,
	.fifo_depth        = RTSER_DEF_FIFO_DEPTH,
	.rx_timeout        = RTSER_DEF_TIMEOUT,
	.tx_timeout        = RTSER_DEF_TIMEOUT,
	.event_timeout     = 1000000000, /* 1 s */
	/*.timestamp_history = RTSER_RX_TIMESTAMP_HISTORY,
	.event_mask        = RTSER_EVENT_RXPEND,
};*/

/*
 * Task attributes 
 */ 
#define TASK_MODE 0  	// No flags
#define TASK_STKSZ 0 	// Default stack size

#define TASK_A_PRIO 20 	// RT priority [0..99]
#define TASK_A_PERIOD_NS 100*1000*1000 // Task period (in ns)

#define TASK_B_PRIO 20 	// priority
#define TASK_B_PERIOD_NS 200*1000*1000 // Task period (in ns)

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

int configSERIAL(int serial_port) {

  //https://blog.mbedded.ninja/programming/operating-systems/linux/linux-serial-ports-using-c-cpp/

  // Create new termios struct, we call it 'tty' for convention
  // No need for "= {0}" at the end as we'll immediately write the existing
  // config to this struct
  struct termios tty;

  // Read in existing settings, and handle any error
  // NOTE: This is important! POSIX states that the struct passed to tcsetattr()
  // must have been initialized with a call to tcgetattr() overwise behaviour
  // is undefined
  if(tcgetattr(serial_port, &tty) != 0) {
      printf("Error %i from tcgetattr: %s\n", errno, strerror(errno));
      return 1;
  }

  tty.c_cflag &= ~PARENB; // Clear parity bit, disabling parity (most common)
  //tty.c_cflag |= PARENB;  // Set parity bit, enabling parity

  tty.c_cflag &= ~CSTOPB; // Clear stop field, only one stop bit used in communication (most common)
  //tty.c_cflag |= CSTOPB;  // Set stop field, two stop bits used in communication

  tty.c_cflag &= ~CSIZE; // Clear all bits that set the data size 
  tty.c_cflag |= CS8; // 8 bits per byte (most common)

  tty.c_cflag &= ~CRTSCTS; // Disable RTS/CTS hardware flow control (most common)
  //tty.c_cflag |= CRTSCTS;  // Enable RTS/CTS hardware flow control

  tty.c_cflag |= CREAD | CLOCAL; // Turn on READ & ignore ctrl lines (CLOCAL = 1)

  tty.c_lflag &= ~ICANON;

  tty.c_lflag &= ~ECHO; // Disable echo
  tty.c_lflag &= ~ECHOE; // Disable erasure
  tty.c_lflag &= ~ECHONL; // Disable new-line echo

  tty.c_lflag &= ~ISIG; // Disable interpretation of INTR, QUIT and SUSP

  tty.c_iflag &= ~(IXON | IXOFF | IXANY); // Turn off s/w flow ctrl
  tty.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL); // Disable any special handling of received bytes

  tty.c_oflag &= ~OPOST; // Prevent special interpretation of output bytes (e.g. newline chars)
  tty.c_oflag &= ~ONLCR; // Prevent conversion of newline to carriage return/line feed
  // tty.c_oflag &= ~OXTABS; // Prevent conversion of tabs to spaces (NOT PRESENT ON LINUX)
  // tty.c_oflag &= ~ONOEOT; // Prevent removal of C-d chars (0x004) in output (NOT PRESENT ON LINUX)

  tty.c_cc[VTIME] = 10;    // Wait for up to 1s (10 deciseconds), returning as soon as any data is received.
  tty.c_cc[VMIN] = 0;

  // Set in/out baud rate to be 115200
  cfsetispeed(&tty, B115200);
  cfsetospeed(&tty, B115200);

  // Save tty settings, also checking for error
  if (tcsetattr(serial_port, TCSANOW, &tty) != 0) {
      printf("Error %i from tcsetattr: %s\n", errno, strerror(errno));
      return 1;
  }
}

void send_rtos(char c) {

    //Send wave[] to UART
    int serial_port = open("/dev/ttyUSB0", O_RDWR);
    configSERIAL(serial_port);
    write(serial_port, &c, sizeof(c));
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
    int prefFlag = 0;

    key_t key; 
    int msgid,data; 
  
    // ftok to generate unique key 
    key = ftok("progfile", 65); 
  
    // msgget creates a message queue 
    // and returns identifier 
    msgid = msgget(key, 0666 | IPC_CREAT); 

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

        // msgrcv to receive message 
        msgrcv(msgid, &wave_data, sizeof(wave_data), 1, IPC_NOWAIT); 
      
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
        
        // Receive start/stop
        data = msgrcv(msgid, &wave_flag, sizeof(wave_flag), 2, IPC_NOWAIT);
        if(data>0){
            if(wave_flag.flag) 
                printf("\nWave started being generated.\n");
            else 
                printf("\nWave stopped being generated.\n");
        }

	}

    // to destroy the message queue 
    msgctl(msgid, IPC_RMID, NULL); 

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
  
    int frequency = wave_data.freq;
    float amplitude = wave_data.amp;
    char waveform = wave_data.form;
    int trigger = wave_data.trigger;
    int transition = wave_data.transition;
	
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
		//if(to!=0) 
			//printf("Measured period (ns)= %lu\n",(long)(ta-to));
		//to=ta;

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
                        printf("Sin waveform: %f.\n",wave[i]);
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
                        printf("Tri waveform: %f.\n",wave[i]);
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
                        printf("Qua waveform: %f\n",wave[i]);
                    }
                    break;
                default:
                    printf("Bad waveform.\n");
                    break;
            }
            changed='n';   

            char c[20] = "#mensagem a enviar$";

            printf("\nSending data to UART.\n");
            for(int i = 0;i<20;i++){
                //send_rtos(c[i]);
            }
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




