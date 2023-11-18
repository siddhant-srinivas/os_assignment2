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
#include <semaphore.h>
#include <fcntl.h>
#include <sys/stat.h>

#define SHM_KEY 0x1234
#define PERMS 0644  
#define MSG_TYPE 2 // Primary Server <-> Client

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

struct thread_arg{
    int msgid;
    struct mesg_buffer buf;
};
void* add_or_modify_graph(void *arg){
    printf("Reached here\n");
    key_t shm_key;
    sem_t *sem;
    if((sem = sem_open("write_protect",0))==SEM_FAILED){
        perror("sem open error");
        exit(1);
    }
    struct mesg_buffer buf = (*(struct thread_arg*)arg).buf;
    int msgid = (*(struct thread_arg*)arg).msgid;
    char* graph_fn = buf.mesg_cont.mesg_text;
    struct mesg_buffer buf2;
    /*for(int i = 0; i < 100; i++){

    }
    char graph_fn[100]; */
    int sem_value;
    if(sem_getvalue(sem,&sem_value)==-1){
        perror("sem_getvalue");
    }
    if(sem_value<1) printf("Critical Section currently busy. Please wait...\n");
    sem_wait(sem);

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
    printf("Shared memory key: %d, shared memory id: %d\n",shm_key,shmid);
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
    char graph_addn[] = "File successfully added";
    char graph_modif[] = "File successfully modified";
    sem_post(sem);
    buf2.mesg_type = MSG_TYPE;
    buf2.mesg_cont.sequence_num = buf.mesg_cont.sequence_num;
    buf2.mesg_cont.operation_num = buf.mesg_cont.operation_num;
    buf.mesg_cont.operation_num ==1 ? strcpy(buf2.mesg_cont.mesg_text, graph_addn) : strcpy(buf2.mesg_cont.mesg_text,graph_modif);
    if(msgsnd(msgid,&buf2,sizeof(buf2.mesg_cont),0)==-1){
            perror("msgsnd error here");
            exit(1);
    }

}

int main(int argc,char const *argv[]){
    struct mesg_buffer buf;
    sem_t *sem;
    pthread_t tid;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    key_t msg_key;
    int msgid;
    sem_unlink("write_protect");
    if((sem = sem_open("write_protect",O_CREAT | O_EXCL,PERMS, 1))==SEM_FAILED){
        perror("sem_open");
        exit(1);
    }
   
    printf("Named Semaphore Initialized\n");
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
        if(msgrcv(msgid,&buf,sizeof(buf.mesg_cont),3,0)==-1){
            printf("%s", buf.mesg_cont.mesg_text);
            perror("msgrcv");
            exit(1);
        }
        printf("Received message from: %ld\n",buf.mesg_type);
        switch(buf.mesg_cont.operation_num){
            case 1:
            case 2:
                struct thread_arg arg;
                arg.msgid = msgid;
                arg.buf = buf;
                if(pthread_create(&tid, &attr, add_or_modify_graph, (void *)&arg) != 0)
                {
                    perror("pthread create failed");
                    return 1;
                }
                void* res;
                pthread_join(tid, &res);
                if(*(int*)res == EINVAL || *(int*)res == ESRCH || *(int*)res == EDEADLK){
                    perror("join error");
                    return 1;
                }
                
                break;
                case 10:
                    printf("Primary server terminated\n");
                    exit(0);
                default:
                break;
        }  
    }  
}               
