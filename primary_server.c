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
#define IDENTIFIER 2

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
        return NULL;
    }
    int *shmptr = (int *)shmat(shmid, NULL, 0);
    if(shmptr == (int*)-1){
        perror("SHMPTR ERROR");
        return NULL;
    }
    int nodes = *shmptr;
    int *ptr = shmptr;
    printf("Nodes: %d\n",nodes);
    shmptr++;
      // Start of matrix values
    int adj_matrix[nodes][nodes];
    for(int i = 0; i < nodes; i++){
        for(int j = 0; j < nodes; j++){
            adj_matrix[i][j] = *shmptr;
            printf("%d ",adj_matrix[i][j]);
            shmptr++;
        }
        printf("\n");
    }
    shmptr = ptr;
    FILE* file = fopen(graph_fn, "w");
    if (file == NULL) {
        perror("Error opening file");
        return NULL;
    }

    fprintf(file, "%d\n", nodes);
    for (int i = 0; i < nodes; i++) {
        for (int j = 0; j < nodes; j++) {
            fprintf(file, "%d ", adj_matrix[i][j]);
        }
        fprintf(file, "\n");
    }

    fclose(file);
    printf("Operation successful!\n");
    if (shmdt(shmptr) == -1) {
        perror("shmdt");
        exit(EXIT_FAILURE);
    }
}

int main(int argc,char const *argv[]){
    struct mesg_buffer buf;
    struct mesg_buffer buf2;
    pthread_t tid;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    key_t msg_key;
    int msgid;
    void* res;
    if((msg_key=ftok("load_balancer.c",'B'))==-1){
        perror("ftok");
        exit(1);
    }
    if((msgid= msgget(msg_key,PERMS | IPC_CREAT))==-1){
        perror("msgget");
        exit(1);
    }
    printf("Primary Server Running! Established connection to Message Queue!\n");
    while(1){
        printf("\n");
        if(msgrcv(msgid,&buf,sizeof(buf.mesg_cont),0,0)==-1){
            printf("%s", buf.mesg_cont.mesg_text);
            perror("msgrcv");
            exit(1);
        }
        printf("Received message from: %ld\n",buf.mesg_type);
        switch(buf.mesg_cont.operation_num){
            case 1:
                if(pthread_create(&tid, &attr, add_or_modify_graph, (void *)buf.mesg_cont.mesg_text) != 0)
                {
                    perror("pthread create failed");
                    return 1;
                }
           
                pthread_join(tid, &res);
                if(*(int*)res != 0){
                    perror("join error");
                    return 1;
                }
                char graph_addn[] = "File successfully added";
                buf2.mesg_type = buf.mesg_type;
                buf2.mesg_cont.sequence_num = buf.mesg_cont.sequence_num;
                buf2.mesg_cont.operation_num = buf.mesg_cont.operation_num;
                strcpy(buf2.mesg_cont.mesg_text, graph_addn);
                if(msgsnd(msgid,&buf2,sizeof(buf2.mesg_cont),0)==-1){
                        perror("msgsnd error here");
                        exit(1);
                }
                
               
               
                break;
            case 2:
                if(pthread_create(&tid, &attr, add_or_modify_graph, (void *)buf.mesg_cont.mesg_text) != 0)
                {
                    perror("pthread create failed");
                    return 1;
                }
                pthread_join(tid, &res);
                 if(*(int*)res != 0){
                    perror("join error");
                    return 1;
                }
                char graph_modif[] = "File successfully modified";
                buf2.mesg_type = buf.mesg_type;
                buf2.mesg_cont.operation_num = buf.mesg_cont.operation_num;
                buf2.mesg_cont.sequence_num = buf.mesg_cont.sequence_num;
                strcpy(buf2.mesg_cont.mesg_text, graph_modif);
                if(msgsnd(msgid,&buf2,sizeof(buf2.mesg_cont),0)==-1){
                        perror("msgsnd");
                        exit(1);
                }
                break;
        }  
    }  
}               
