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
#include <pthread.h>

#define SHM_KEY 0x1234
#define PERMS 0644  

struct mesg_buffer{
    long mesg_type;
    char mesg_text[100];
};

void* add_or_modify_graph(void *arg){
    
    key_t shm_key;
    char *graph_fn = (char *)arg;
    /*for(int i = 0; i < 100; i++){

    }
    char graph_fn[100]; */

    if((shm_key=ftok("client.c",'D'))== -1){
        perror("ftok failed");
        exit(1);
    }
    int shmid;
    int BUF_SIZE = sizeof(int) * 900;
    shmid = shmget(shm_key,BUF_SIZE,PERMS|IPC_CREAT);
    if(shmid == -1){
        perror("SHM error");
        return 1;
    }
    int *shmptr = (int *)shmat(shmid, NULL, 0);
    if(shmptr == (int*)-1){
        perror("SHMPTR ERROR");
        return 1;
    }

    int nodes = *shmptr;
    int *ptr = shmptr + 1;  // Start of matrix values

    int **adj_matrix;
    for(int i = 0; i < nodes; i++){
        for(int j = 0; j < nodes; j++){
            adj_matrix[i][j] = *ptr;
            ptr++;
        }
    }
    FILE* file = fopen(graph_fn, "w");
    if (file == NULL) {
        perror("Error opening file");
        return 1;
    }

    fprintf(file, "%d\n", nodes);
    for (int i = 0; i < nodes; ++i) {
        for (int j = 0; j < nodes; ++j) {
            fprintf(file, "%d ", adj_matrix[i][j]);
        }
        fprintf(file, "\n");
    }

    fclose(file);

    if (shmdt(shmptr) == -1) {
        perror("shmdt");
        exit(EXIT_FAILURE);
    }
}

int main(int argc,char const *argv[]){

    struct mesg_buffer buf;
    struct mesg_buffer buf2;
    key_t msg_key;
    int msgid;
    if((msg_key=ftok("load_balancer.c",'B'))==-1){
        perror("ftok");
        exit(1);
    }
    if((msgid= msgget(msg_key,PERMS | IPC_CREAT))==-1){
        perror("msgget");
        exit(1);
    }
    while(1){
        if(msgrcv(msgid,&buf,sizeof(buf.mesg_text),0,0)==-1){
            printf("%s", buf.mesg_text);
            perror("msgrcv");
            exit(1);
        }
        switch(buf.mesg_type){
            case 1:
                pthread_t tid;
                pthread_attr_t attr;
                pthread_attr_init(&attr);
                if(pthread_create(&tid, &attr, add_or_modify_graph, (void *)buf.mesg_text) != 0)
                {
                    perror("pthread create failed");
                    return 1;
                }
                char graph_addn[] = "File successfully added";
                strcpy(buf2.mesg_text, graph_addn);
                if(msgsnd(msgid,&buf2,strlen(buf2.mesg_text)+1,0)==-1){
                        perror("msgsnd");
                        exit(1);
                }
                if(pthread_join(tid, NULL) != 0){
                    perror("join error");
                    return 1;
                }
                break;
            case 2:
                pthread_t tid;
                pthread_attr_t attr;
                pthread_attr_init(&attr);
                if(pthread_create(&tid, &attr, add_or_modify_graph, (void *)buf.mesg_text) != 0)
                {
                    perror("pthread create failed");
                    return 1;
                }
                char graph_modif[] = "File successfully modified";
                strcpy(buf2.mesg_text, graph_modif);
                if(msgsnd(msgid,&buf2,strlen(buf2.mesg_text)+1,0)==-1){
                        perror("msgsnd");
                        exit(1);
                }
                if(pthread_join(tid, NULL) != 0){
                    perror("join error");
                    return 1;
                }
                break;
        }  
    }  
}               
