#include "init.h"

int global_tick = 0;
int time_quantum = 3;
pcb *proc[10];

int *msgq_id;

QUEUE* srunq;
QUEUE* swaitq;
QUEUE* sretireq;

FILE* fp;

int main(int argc, char* argv[]){
	int pid;

	struct sigaction old_sighandler, new_sighandler;
	struct sigaction old_sighandler2, new_sighandler2;
	struct itimerval itimer, old_timer;
	
    int ret;
	struct msgq_data msg;
	int msgq_size;

    int i, r1, r2;

    srunq    = createQueue();
    swaitq   = createQueue();
	sretireq = createQueue();

	if((fp = fopen("schedule_dump.txt", "w")) == NULL){
		fprintf(stderr, "Error");
		exit(1);
	}

	srand((unsigned int)time(NULL));

	msgq_id = (int*)malloc(sizeof(int));
    ret = msgget((key_t)1127, IPC_CREAT | 0644);

    if(ret == -1){
	    printf("msgq creation error! (p) ");
        printf("errno: %d\n", errno);

        *msgq_id = -1;
    }else{
        *msgq_id = ret;
        printf("msgq (key:0x%x, id:0x%x) created.\n", 1127, ret);
    }

    for(i = 0; i < 10; i++){
        if((pid = fork()) > 0){ // parent
			// random cpu and io time
            do{
                r1 = rand() % 15;
            }while(r1 == 0);
            r2 = rand() % 15;

            // create child proc
            proc[i] = (pcb*)malloc(sizeof(pcb));
            memset(proc[i], 0, sizeof(pcb));
			proc[i]->od = i+1;
            proc[i]->pid = pid;
            proc[i]->state = READY;
			proc[i]->cpu_burst = r1;
			proc[i]->io_burst = r2;
 
            printf("child[%d] (%d) init cpu <%d>, init io <%d>\n", proc[i]->od, proc[i]->pid, proc[i]->cpu_burst, proc[i]->io_burst);

			enqueue(srunq, proc[i]);
        }else if(pid < 0){ // fork() fail
            printf("fork failed \n");
            return 0;
        }else{ // child
            do_child();
            return 0;
        }
    } // end for

	printProc();

	// register timer signal
    memset(&new_sighandler, 0, sizeof(new_sighandler));
    new_sighandler.sa_handler = &time_tick;
    sigaction(SIGALRM, &new_sighandler, &old_sighandler);

    // setitimer
    itimer.it_interval.tv_sec = 1;
    itimer.it_interval.tv_usec = 0;
    itimer.it_value.tv_sec = 1;
    itimer.it_value.tv_usec = 0;
    global_tick = 0;
    setitimer(ITIMER_REAL, &itimer, &old_timer);

	while(1){
		if((count_end(sretireq) == 10) || (count_end(srunq) == 10)) break;

        if(msgq_id > 0){
        ret = msgrcv(*msgq_id, &msg, sizeof(msg), 0, 0);
           if(ret > 0){
                int io_pid;
                io_pid = msg.pid;
                for(i = 0; i < 10; i++){
                    // change state
                    if(proc[i]->pid == io_pid){
                        proc[i]->state = WAIT;
                        proc[i]->io_burst = msg.io_time;
                        time_quantum = 0;
                        printf("\nnow tick %d, proc %d request io for %d ticks,and sleep \n", global_tick, proc[i]->od, proc[i]->io_burst);
						break;
                    }
                }
            }else if(ret == 0){
                printf("not recieve message!\n");
            }else{
                printf("msgrcv error! (%d)\n", errno);
            }
        }
    }

	for(i = 0; i < 10; i++){
		kill(proc[i]->pid, SIGKILL);
	}

	fclose(fp);
	return;	
}

void time_tick(int signo){
    int run_pid = schedule();
	int ret;

	node *now_node = find_proc(srunq, run_pid);
	pcb  *now_proc = now_node->proc;
	node *temp;

    global_tick++;

    if(global_tick > 90){
        int i = 0;
        for(i = 0; i < 10; i++)
            kill(proc[i]->pid, SIGKILL);

        kill(getpid(), SIGKILL);
        return;
    }

	if(time_quantum > 0){
		time_quantum--;
		decreaseIO();
		if((now_proc->cpu_burst) > 0) now_proc->cpu_burst--;

		if(time_quantum > 0) printProc();

		if(now_proc->cpu_burst == 0){
			temp = dequeue(srunq);

				if(now_proc->io_burst != 0){
					enqueue(swaitq, temp->proc);
				}else{
					enqueue(sretireq, temp->proc);
				}

			if(srunq->count == 0){
					if(sretireq->count != 0) changeq(srunq, sretireq);
			}

			time_quantum = 3;
			kill(run_pid, SIGALRM);

			printProc();
			return;	
		}
	}

	if(time_quantum == 0){
		printProc();
		temp = dequeue(srunq);

			if((now_proc->cpu_burst == 0) && (now_proc->io_burst != 0)){
				enqueue(swaitq, temp->proc);
			}else{
				enqueue(sretireq, temp->proc);
			}

		if(srunq->count == 0){
            if(sretireq->count != 0) changeq(srunq, sretireq);
        }

		time_quantum = 3;
		printProc();
	}
    ret = kill (run_pid, SIGALRM);
}

int schedule(void){
	// find proc from runq, according to the rr policy
	node* temp;
	temp = srunq->front;

	// reset time quantum
	if(time_quantum == 0) time_quantum = 3;

    return temp->proc->pid;
}

void do_child(void){
	int ret, i, msgq_size;
	struct sigaction old_sighandler, new_sighandler;
	struct msgq_data msg;

	memset(&new_sighandler, 0, sizeof(new_sighandler));
	new_sighandler.sa_handler = &child_process;
	sigaction(SIGALRM, &new_sighandler, &old_sighandler);

	node *now_node = find_proc(srunq, getpid());
	if(now_node == NULL) now_node = find_proc(sretireq, getpid());
	pcb *now_proc = now_node->proc;


    // open msgq
    msgq_id = (int *)malloc(sizeof(int));
	ret = *msgq_id; 
	if(ret == -1){
        printf("msgq creation error! (p) ");
        printf("errno: %d\n", errno);
    }

	while(1){
		if(now_proc->cpu_burst == 0){
			msg.mtype = getppid();
			msg.pid = getpid();
			msg.io_time = rand() % 10;
			msgq_size = sizeof(msg) - sizeof(msg.mtype);

			ret = msgsnd(*msgq_id, &msg, msgq_size, 0);
		
			if(ret == -1){
				printf("msgsnd() fail\n");
				return;
			}
		}else{
			printf("pass!");
		}
    }
}

void child_process(int signo){
	printf("hoit");

	node *now_node;
	pcb  *now_proc;

	now_node = find_proc(srunq, getpid());
	if(now_node != NULL){
		now_proc = now_node->proc;
		now_proc->cpu_burst--;
	}

	now_node = find_proc(swaitq, getpid());
    if (now_node != NULL) {
       do_io();
    }

	return;
}

void do_io(void){
	struct msgq_data msg;
	int ret;
	key_t key;

	if (*msgq_id == -1) {
		ret = msgget(MY_MSGQ_KEY, IPC_CREAT | 0644);
		if (ret == -1) {
			printf("msgq creation error !  (c) ");
			printf("errno: %d\n", errno);
		}
		*msgq_id = ret;
	}

	msg.mtype = getpid();
	msg.io_time = 1;
	msgsnd(*msgq_id, &msg, sizeof(msg), 0);
}

void decreaseIO(){
	node *swait;
	node *temp;

	for(swait = swaitq->front; swait != NULL; swait = swait->next){
		swait->proc->io_burst--;

		if(swait->proc->io_burst == 0){
			temp = swait;

			if(temp->next != NULL){
				if(temp->prev != NULL){
					temp->prev->next = temp->next;
				}else{
					swaitq->front = temp->next;
					temp->next->prev = NULL;
				}
			}

			if(swaitq->count == 1){
				swaitq->front = NULL;
				swaitq->rear = NULL;
				swaitq->count = 0;

				enqueue(sretireq, temp->proc);
				return;
			}else{
				if(temp->prev != NULL){
					temp->prev->next = temp->next;
				}else{
					swaitq->front = temp->next;
					temp->next->prev = NULL;
				}
			}
			enqueue(sretireq, temp->proc);
			swaitq->count--;
		}
	}

	return;
}


void printProc (){
	node* temp;

	printf("\nAt tick %d, ", global_tick);
	printf("current: %d, ", srunq->front->proc->od);
	printf("remain quantum: %d,\n", time_quantum);
	printf("\trunq: ");
	printQueue(srunq);
	printf("\n\tretireq: ");
	printQueue(sretireq);
	printf("\n\twaitq: ");
	printQueue(swaitq);

	fprintf(fp, "\nAt tick %d, ", global_tick);
    fprintf(fp, "current: %d, ", srunq->front->proc->od);
    fprintf(fp, "remain quantum: %d,\n", time_quantum);
    fprintf(fp, "\trunq: ");

	fprintf(fp, "<");
    if(srunq->front != NULL){
       for(temp = srunq->front; temp->next != NULL; temp = temp->next){
            fprintf(fp, "%d, ", temp->proc->od);
        }
        fprintf(fp, "%d", temp->proc->od);
    }
    fprintf(fp, ">");

    fprintf(fp, "\n\tretireq: ");
	
	fprintf(fp, "<");
    if(sretireq->front != NULL){
       for(temp = sretireq->front; temp->next != NULL; temp = temp->next){
            fprintf(fp, "%d, ", temp->proc->od);
        }
        fprintf(fp, "%d", temp->proc->od);
    }
    fprintf(fp, ">");

    fprintf(fp, "\n\twaitq: ");

	fprintf(fp, "<");
    if(swaitq->front != NULL){
       for(temp = swaitq->front; temp->next != NULL; temp = temp->next){
            fprintf(fp, "%d, ", temp->proc->od);
        }
        fprintf(fp, "%d", temp->proc->od);
    }
    fprintf(fp, ">");

}

int count_end(QUEUE* queue){
	node *snode;
    node *temp;
	int end_count = 0;

    for(snode = queue->front; snode != NULL; snode = snode->next)
	{
		if((snode->proc->cpu_burst == 0) && (snode->proc->io_burst == 0))
			end_count++;
	}

	return end_count;
}
