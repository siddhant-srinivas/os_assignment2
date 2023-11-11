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

struct mesg_buffer{
    long mesg_type;
    char mesg_text[100];
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
			struct mesg_buffer buf;
			buf.mesg_type = 10;
			strcpy(buf.mesg_text, "Termination request");

			if(msgsnd(msgid, &buf, strlen(buf.mesg_text) + 1, 0) == -1){
				perror("msgsnd failed");
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