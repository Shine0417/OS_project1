#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sched.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <signal.h>
#include <sys/wait.h>
#include <time.h>

#define PARENT_CPU 0
#define CHILD_CPU 1
#define TIME 342
#define PRINT 341
#define Nano 1000000000
typedef struct process {
	char name[32];
	int ready_time;
	int execute_time;
	pid_t pid;
}Process;
static int threshold = 0;
Process *proc;
int n;
char policy[200];
int running;
int last_change;
int t;
int last;
int cmp(const void *a, const void *b) {
	int result = ((struct process *)a)->ready_time - ((struct process *)b)->ready_time;
	if(result < 0)
		return -1;
	else if(result == 0)
		return 0;
	else
		return 1;
	
}

void unit_time(){ 
	volatile unsigned long i; 
	for(i=0;i<1000000UL;i++); 
	return;
}
void enter(){
	threshold = 1;
	return;
}

void assign_cpu(int pid, int cpu_id){
	cpu_set_t mask;
	CPU_ZERO(&mask);
	CPU_SET(cpu_id, &mask);
	
	if (sched_setaffinity(pid, sizeof(mask), &mask) < 0) {
		perror("assign cpu err");
		exit(1);
	}

	return;
}

void wake_process(int pid){
	struct sched_param parameter;
	parameter.sched_priority = 99;

	int tmp = sched_setscheduler(pid, SCHED_FIFO, &parameter);
	
	if (tmp < 0) {
		perror("wake error");
		exit(1);
	}

	return;
}

int block_process(int pid){
	struct sched_param parameter;
	parameter.sched_priority = 0;

	int tmp = sched_setscheduler(pid, SCHED_IDLE, &parameter);
	
	if (tmp < 0) {
		perror("block error");
		exit(0);
	}
	return 0;
}

int create_process(Process p){
	signal(SIGUSR1, enter);
	pid_t pid = fork();
	if(pid < 0){
		printf("fork err\n");
		exit(0);
	}
	if(pid == 0){
		//block_process(getpid());
		while(threshold == 0);
		long long int start_time = syscall(TIME);
		//printf("pid: %d start_time: %lld\n",getpid(), start_time);
		for(int i = 0; i < p.execute_time; i++){
			unit_time();
		}
		long long int end_time = syscall(TIME);
		//printf("pid: %d end_time:%lld\n", getpid(),end_time);
		syscall(PRINT, getpid(), (long)start_time/Nano, (long)start_time%Nano, (long)end_time/Nano, (long)end_time%Nano);
		exit(0);
	}
	//printf("pid :%d created_byparent\n", pid);
	assign_cpu(pid, CHILD_CPU);
	
	return pid;
}

int shortest_job_first(){
	if(running != -1){
		return running;
	}
	int next = -1;
	for (int i = 0; i < n; i++) {
		if (proc[i].pid == -1 || proc[i].execute_time == 0)
			continue;
		if(next == -1)
			next = i;
		if (proc[i].execute_time < proc[next].execute_time)
			next = i;
	}
	return next;
}

int fifo(){
	if(running != -1){
		return running;
	}
	int next = -1;
	for (int i = 0; i < n; i++) {
		if (proc[i].pid == -1 || proc[i].execute_time == 0)
			continue;
		if(next == -1)
			next = i;
		if (proc[i].ready_time < proc[next].ready_time)
			next = i;
	}
	return next;
}

int round_robin(){
	int next = -1;
	if(running == -1){
		for (int i = last; i < n; i++)
			if (proc[i].pid != -1 && proc[i].execute_time > 0){
				next = i;
				return next;
			}
		for (int i = 0; i < last; i++)
			if (proc[i].pid != -1 && proc[i].execute_time > 0){
				next = i;
				return next;
			}
	}
	else if((t - last_change)%500 == 0){
		next = (running+1)%n;
		while(proc[next].pid == -1 || proc[next].execute_time == 0){
			next = (next + 1) % n;
		}
		return next;
	}
	else{
		next = running;
	}
	return next;
}

int priority_sjf(){
	int next = -1;
	for (int i = 0; i < n; i++) {
		if(proc[i].pid == -1 || proc[i].execute_time == 0)
			continue;
		if(next == -1 || proc[i].execute_time < proc[next].execute_time)
			next = i;
	}
	return next;
}
int next_process(){
	int next = -1;
	if(strcmp(policy, "SJF") == 0)
		next = shortest_job_first();
	else if(strcmp(policy, "FIFO") == 0)
		next = fifo();
	else if(strcmp(policy, "RR") == 0)
		next = round_robin();
	else if(strcmp(policy, "PSJF") == 0)
		next = priority_sjf();
	return next;
}


void scheduler(){
	assign_cpu(getpid(), PARENT_CPU);
	wake_process(getpid());
	t = 0;
	last_change = 0;
	int count = 0;
	last = 0;
	while(1){	
		//printf("running:%s\n", proc[running].name);
		if(running != -1 && proc[running].execute_time == 0){
			if(waitpid(proc[running].pid, NULL, 0) == -1){
				printf("waitpid err");
				exit(0);
			}
			printf("%s %d\n", proc[running].name, proc[running].pid);
			fflush(stdout);
			last = running;
			running = -1;
			count++;
			if(count == n){
				exit(0);
			}
		}

		for(int i = 0; i < n; i++)
			if(proc[i].ready_time == t){
				proc[i].pid = create_process(proc[i]);
				block_process(proc[i].pid);
			}
		//printf("s\n");
		int next_id = next_process();
		//printf("debug\n");
		//long long int start_time = syscall(TIME);
		//printf("pid: %d main!!!_time: %lld\n",getpid(), start_time);
		
		if(next_id != -1){
			if(running != next_id){
				//printf("running:%s, next:%s\n", proc[running].name, proc[next_id].name);
				block_process(proc[running].pid);
				kill(proc[next_id].pid , SIGUSR1);
				wake_process(proc[next_id].pid);
				running = next_id;
				last_change = t;
			}
		}
		unit_time();
		if(running != -1){
			proc[running].execute_time--;
		}
		t++;
	}
	return;
}

int main(int argc, char* argv[]){
	scanf("%s", policy);
	scanf("%d", &n);
	proc = (struct process*)malloc(n*sizeof(struct process));

	for(int i = 0; i < n; i++){
		scanf("%s %d %d", proc[i].name, &proc[i].ready_time, &proc[i].execute_time);
		proc[i].pid = -1;
	}

	qsort(proc, n, sizeof(struct process), cmp);
	/*
	for(int i = 0; i < n; i++)
		printf("%s %d %d", proc[i].name, proc[i].ready_time, proc[i].execute_time);
	*/
	running = -1;
	scheduler();
	return 0;
}
