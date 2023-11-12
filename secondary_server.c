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
    long seq_num;
    char mesg_text[100];
};

void* dfsStartRoutine(void *arg)
{    
    key_t shm_key;
    char *graph_fn = (char *)arg;
    
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
    int starting_vertex = *shmptr;
    
    FILE* file = fopen(graph_fn, "r");
    if (file == NULL) {
        perror("Error opening file");
        return 1;
    }

    int n;
    fscanf(file, "%d", &n);

    int graph[n][n];
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            fscanf(file, "%d", &graph[i][j]);
        }
    }

    fclose(file);

    printf("DFS Order: ");
    dfs(n, graph, starting_vertex);
    
    
    if (shmdt(shmptr) == -1) {
        perror("shmdt");
        exit(EXIT_FAILURE);
    }
}


int main(int argc,char const *argv[]){
    /*//Handling server numbers
    int server_num = atoi(argv[1]);
    server_num %= 2;
    if(server_num%2 ==0) server_num +=2;*/

    struct mesg_buffer buf;
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

    while(1)
    {
        if(msgrcv(msgid,&buf,sizeof(buf.mesg_text),0,0)==-1){
            printf("%s", buf.mesg_text);
            perror("msgrcv");
            exit(1);
        }
        switch(buf.mesg_type){
            case 3:
                pthread_t tid;
                pthread_attr_t attr;
                pthread_attr_init(&attr);
                if(pthread_create(&tid, &attr, dfsStartRoutine, (void *)buf.mesg_text) != 0)
                {
                    perror("pthread create failed");
                    return 1;
                }
                
                char dfs_rd_done[] = "DFS reading done";
                strcpy(buf2.mesg_text, dfs_rd_done);
                if(msgsnd(msgid,&buf2,strlen(buf2.mesg_text)+1,0)==-1){
                        perror("msgsnd");
                        exit(1);
                }
                if(pthread_join(tid, NULL) != 0){
                    perror("join error");
                    return 1;
                }
                break;
            
            
            
            case 4:
        }  
    }       
    }
}
