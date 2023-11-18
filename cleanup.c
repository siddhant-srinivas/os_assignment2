#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <errno.h>
#include <string.h>
#define PERMS 0666

struct mesg_content{
	long sequence_num;
    long operation_num;
	char mesg_text[100];
}; 
typedef struct mesg_content mesg_content;
struct mesg_buffer{
    long mesg_type;
    mesg_content mesg_cont;
};


int main(){
	key_t key;
	int msgid;
	if((key=ftok("load_balancer.c",'B'))==-1){
        perror("ftok failed");
        exit(1);
    }
    if((msgid= msgget(key,PERMS))==-1){
        perror("msgget failed");
        exit(1);
    }
	while(1){
		char c;
		printf("Do you want the server to terminate? Press Y for Yes and N for No.\n");
		fflush(stdout);			//Clearing the '\n' from o/p stream
		c = getchar();
		while(getchar() != '\n');		//Clearing i/p buffer

		if((c == 'Y') || (c == 'y')){
			char cont[] = "Terminate";
			 struct mesg_buffer buf2;
                buf2.mesg_type = 10;
                buf2.mesg_cont.operation_num =10;
                buf2.mesg_cont.sequence_num = -1;
                strcpy(buf2.mesg_cont.mesg_text, cont);
                if(msgsnd(msgid,&buf2,sizeof(buf2.mesg_cont),0)==-1){
                        perror("msgsnd");
                        exit(1);
                }
			
			printf("Termination Request sent to Load Balancer. Cleanup Process terminating...\n");
			break;
		}

		else if((c == 'N') || (c == 'n')){
			printf("Nothing communicated to Load Balancer. Processes still running...\n");
		}
		else{
			printf("Please enter a valid input.\n");
		}
	}

    return 0;
}