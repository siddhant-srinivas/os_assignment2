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
#define MSG_TYPE_PRIMARY 3
#define MSG_TYPE_SECONDARY 4

struct mesg_content{
	long sequence_num;
    long operation_num;
	char mesg_text[100];
}; 
typedef struct mesg_content mesg_content;
struct mesg_buffer{
    long mesg_type;
    // long seq_num;
    mesg_content mesg_cont;
};

int main(int argc, char* argv[]){
    struct mesg_buffer buf;
    key_t key;
    int len;
    int msgid;
    if((key=ftok("load_balancer.c",'B'))==-1){
        perror("ftok");
        exit(1);
    }
    if((msgid= msgget(key,PERMS | IPC_CREAT))==-1){
        perror("msgget");
        exit(1);
    }
    printf("Load Balancer running! Message Queue created!\n");
    while(1){
        fflush(stdout);
        printf("\n");
        if(msgrcv(msgid,&buf,sizeof(buf.mesg_cont),0,0)==-1){
            perror("msgrcv");
            exit(1);
        }
        //printf("Received Message from: %ld\n",buf.mesg_type);
        printf("Sequence Number: %ld\n",buf.mesg_cont.sequence_num);
        printf("Contents: %s\n", buf.mesg_cont.mesg_text);
        switch(buf.mesg_cont.operation_num){
            case 1:
            case 2:
                struct mesg_buffer buf2;
                buf2.mesg_type = MSG_TYPE_PRIMARY;
                buf2.mesg_cont.operation_num = buf.mesg_cont.operation_num;
                buf2.mesg_cont.sequence_num = buf.mesg_cont.sequence_num;
                strcpy(buf2.mesg_cont.mesg_text, buf.mesg_cont.mesg_text);
                if(msgsnd(msgid,&buf2,sizeof(buf2.mesg_cont),0)==-1){
                        perror("msgsnd");
                        exit(1);
                }
               break;
               
            case 3:
                if(msgsnd(msgid,&buf,sizeof(buf.mesg_cont),0)==-1){
                        perror("msgsnd");
                        exit(1);
                }
               break;
            case 10:
				buf2.mesg_type = MSG_TYPE_PRIMARY;
				buf2.mesg_cont.operation_num = 10;
				buf2.mesg_cont.sequence_num = -1;
                char cont[] = "Terminate";
				strcpy(buf2.mesg_cont.mesg_text, cont);
				if(msgsnd(msgid,&buf2,sizeof(buf2.mesg_cont),0)==-1){
					perror("msgsnd");
					exit(1);
				}
				buf2.mesg_type = MSG_TYPE_SECONDARY;
				buf2.mesg_cont.operation_num = 10;
				buf2.mesg_cont.sequence_num = -1;
				strcpy(buf2.mesg_cont.mesg_text, cont);
				if(msgsnd(msgid,&buf2,sizeof(buf2.mesg_cont),0)==-1){
					perror("msgsnd");
					exit(1);
				}
				if(msgsnd(msgid,&buf2,sizeof(buf2.mesg_cont),0)==-1){
					perror("msgsnd");
					exit(1);
				}
				sleep(5);
				if(msgctl(msgid, IPC_RMID, NULL) == -1){
					perror("Could not close message queue");
					exit(1);
				}
				printf("Load Balancer Terminated\n");
				exit(0);
				break;
            	
        }

    }

}
