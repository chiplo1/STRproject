#include <stdio.h>
#include <pthread.h>
#include <string.h>

typedef struct {
	int freq;
	float amp;
	char form[16];
	char trigger;
	char condition;
} wave_data;

void* send_to_xenomai(void* arg){
    wave_data *wd = arg;
	printf(" Done!\n");
}

void* interface(void* arg){
	wave_data wd;

	while(1){
		printf("Frequency(KHz) [1-1000]: ");
		scanf("%d", &wd.freq);
		if(wd.freq < 1 || wd.freq > 1000) continue;
wave_data wd;
		printf("Amplitude(V) [0.0-3.3]: ");
		scanf("%3f", &wd.amp);
		if(wd.amp > 3.3 || wd.amp < 0.0) continue;

		printf("Waveform [sin/triangle/square]: ");
		scanf("%s", &wd.form);
		if(strcmp(wd.form, "sin") != 0 && strcmp(wd.form, "triangle") != 0 && strcmp(wd.form, "square") != 0) continue;

		printf("Trigger(falling or rising edge) [0/1]: ");
		scanf("%d", &wd.trigger);
		if(wd.trigger != 0 && wd.trigger != 1) continue;

		printf("Condition [0/1]: ");
		scanf("%d", &wd.condition);
		if(wd.condition != 0 && wd.condition != 1) continue;

		printf("Sending data to Xenomai ...");
		pthread_t tid;
		pthread_create(&tid, NULL, &send_to_xenomai, &wd);
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