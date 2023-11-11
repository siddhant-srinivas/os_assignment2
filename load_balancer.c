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

    while(1){
        if(msgrcv(msgid,&buf,sizeof(buf.mesg_text),0,0)==-1){
            perror("msgrcv");
            exit(1);
        }
        printf("%s", buf.mesg_text);
        switch(buf.mesg_type){
            case 1:
            case 2:
                if(msgsnd(msgid,&buf,strlen(buf.mesg_text)+1,0)==-1){
                        perror("msgsnd");
                        exit(1);
                }
               break;
        }

    }

}
