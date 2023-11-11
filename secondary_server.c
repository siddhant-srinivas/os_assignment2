#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <errno.h>
#include <string.h>

#define SHM_KEY 0x1234
#define PERMS 0644  

struct mesg_buffer{
    long mesg_type;
    char mesg_text[100];
};
void* depth_first_search(void* arg){
    
}
int main(int argc,char const *argv[]){
    //Handling server numbers
    int server_num = atoi(argv[1]);
    server_num %= 2;
    if(server_num%2 ==0) server_num +=2;

    struct mesg_buffer buf;
    key_t key;
    int len;
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
        if(msgrcv(msgid,&buf,sizeof(buf.mesg_text),0,0)==-1){
            printf("%s", buf.mesg_text);
            perror("msgrcv");
            exit(1);
        }

        
    }
}