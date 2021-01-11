#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ipc.h> 
#include <sys/msg.h> 
#include <sys/types.h>

#include <unistd.h>

#define MAX 10 

#define PIPE_MINOR 0

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

void* start_stop(void* arg){
	//printf("Start/Stop!");

	int flag = 1;

	char input;

	key_t key; 
    int msgid; 
  
    // ftok to generate unique key 
    key = ftok("progfile", 65); 
  
    // msgget creates a message queue 
    // and returns identifier 
    msgid = msgget(key, 0666 | IPC_CREAT);

    printf("\n\nPress ENTER to start/stop the signal generation or insert y to create a new wave.\n");

    scanf("%c", &input);

	while(1){  

        if(flag==1){
            printf("-> The wave started being generated.\n");
        }
        if(flag==0){
            printf("-> The wave stopped being generated.\n");
        }

        scanf("%c", &input);

        if(input=='y') break;
        
        flag=!(flag);

        wave_flag.flag=flag;
        wave_flag.mtype=2;

        msgsnd(msgid, &wave_flag, sizeof(wave_flag), 0);
        
	}


}

void* send_to_xenomai(void* arg){

	printf(" Done!\n");

    key_t key; 
    int msgid; 
  
    // ftok to generate unique key 
    key = ftok("progfile", 65); 
  
    // msgget creates a message queue 
    // and returns identifier 
    msgid = msgget(key, 0666 | IPC_CREAT); 
    
    printf("Sending wave: freq=%d, amp=%3f, form=%c, trigger=%d, transition=%d\n",wave_data.freq,wave_data.amp,wave_data.form,wave_data.trigger,wave_data.transition);

    // msgsnd to send message 
    msgsnd(msgid, &wave_data, sizeof(wave_data), 0); 
  
    // display the message 
    printf("Data sent.\n");

    if(wave_data.trigger==0){
	    printf("Starting a Start/Stop switch...");
	    pthread_t tid;
	    pthread_create(&tid, NULL, &start_stop, NULL);
	    pthread_join(tid, NULL);
    }


}

void* interface(void* arg){

	while(1){
        printf("\n\n----------INSERT WAVE PARAMETERS----------\n");
		printf("Frequency(KHz) [1-1000]: ");
		scanf("%d", &wave_data.freq);
		if(wave_data.freq < 1 || wave_data.freq > 1000) continue;

		printf("Amplitude(V) [0.0-3.3]: ");
		scanf("%3f", &wave_data.amp);
		if(wave_data.amp > 3.3 || wave_data.amp < 0.0) continue;

		printf("Waveform [sin(s)/triangle(t)/square(q)]: ");
		scanf("%s", &wave_data.form);
		if(wave_data.form != 's' && wave_data.form != 't' && wave_data.form != 'q') continue;

		printf("Trigger condition activated [0/1]: ");
		scanf("%d", &wave_data.trigger);
		if(wave_data.trigger != 0 && wave_data.trigger != 1) continue;

        if(wave_data.trigger==1){
		    printf("Trigger negative or positive edge trigger [0/1]: ");
		    scanf("%d", &wave_data.transition);
		    if(wave_data.transition != 0 && wave_data.transition != 1) continue;
        }
        else wave_data.transition=0;

        wave_data.mtype=1;

		printf("Sending data to Xenomai ...");
		pthread_t tid;
		pthread_create(&tid, NULL, &send_to_xenomai, &wave_data);
		pthread_join(tid, NULL);
	}

}

int main(){

	printf("Starting user interface ...");
	pthread_t tid;
	pthread_create(&tid, NULL, &interface, NULL);
	pthread_join(tid, NULL);
	

	return 0;
}
