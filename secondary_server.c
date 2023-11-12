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

struct mesg_buffer
{
    long mesg_type;
    long seq_num;
    char mesg_text[100];
};
struct dfsStruct        //This is the stuff we need in order to do dfs. Using struct because we can pass only 1 arg in thread creation
{
    int n;
    int starting_vertex;
    int **adjacency_matrix;
    int *visited;
    pthread_mutex_t mutex;
};


void* dfs(struct dfsStruct* dfsReqs)
{   
    int currentNode = dfsReqs->starting_vertex - 1; //-1 becasue array indexing starts from 0
    
    pthread_mutex_lock(&dfsReqs->mutex);   //locking mutex (visited array is the critical section)
    printf("Visiting node: %d\n", currentNode + 1);  //because we had done -1 above
    dfsReqs->visited[currentNode] = 1;
    pthread_mutex_unlock(&dfsReqs->mutex); //unlocking mutex

    for (int i = 0; i < dfsReqs->n; i++)    //exploring adjacent nodes
    {
        if (dfsReqs->adjacency_matrix[currentNode][i] && !dfsReqs->visited[i])
        {
            struct dfsStruct* dfsReqsChild = malloc(sizeof(struct dfsStruct));
            memcpy(dfsReqsChild, dfsReqs, sizeof(struct dfsStruct));
            dfsReqsChild->starting_vertex = i + 1; // again, similar reason as before

            pthread_t tid;
	    pthread_attr_t attr;
	    pthread_attr_init(&attr);
	    if(pthread_create(&tid, &attr, dfs, threadArgs) != 0)  //creating a new thread for the unvisited node
	    {
	    	perror("pthread create failed");
		return 1;
	    }
            if(pthread_join(tid, NULL) != 0)
	    {
		perror("join error");
	    	return 1;
	    }
        }
    }
    pthread_exit(NULL);
}



void startDFS(struct dfsStruct* dfsReqs)
{

    pthread_mutex_init(&dfsReqs->mutex, NULL);                          //initializing the visited and mutex of the struct
    dfsReqs->visited = (int*)calloc(dfsReqs->n, sizeof(int));
    
    pthread_t tid;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    if(pthread_create(&tid, &attr, dfs, dfsReqs) != 0)  //creating a new thread to service this request
    {
    	perror("pthread create failed");
        return 1;
    }
    if(pthread_join(tid, NULL) != 0)
    {
        perror("join error");
    	return 1;
    }
    
    pthread_mutex_destroy(&dfsReqs->mutex);
    free(dfsReqs->visited);
}


void* dfsStartRoutine(void *arg)
{
    struct dfsStruct dfsReqs;
    
    key_t shm_key;
    char *graph_fn = (char *)arg;
    if((shm_key=ftok("client.c",'D'))== -1)
    {
        perror("ftok failed");
        exit(1);
    }
    int shmid;
    int BUF_SIZE = sizeof(int) * 900;
    shmid = shmget(shm_key,BUF_SIZE,PERMS|IPC_CREAT);
    if(shmid == -1)
    {
        perror("SHM error");
        pthread_exit(NULL);
    }
    int *shmptr = (int *)shmat(shmid, NULL, 0);
    if(shmptr == (int*)-1)
    {
        perror("SHMPTR ERROR");
        pthread_exit(NULL);
    }
    dfsReqs.starting_vertex = *shmptr;
    
    FILE* file = fopen(graph_fn, "r");
    if (file == NULL)
    {
        perror("Error opening file");
        pthread_exit(NULL);
    }
    fscanf(file, "%d", &dfsReqs.n);

    int n = dfsReqs.n;
    dfsReqs.adjacency_matrix = (int **)malloc(sizeof(int *) * n);
    for (int i = 0; i < n; ++i)
    {
        dfsReqs.adjacency_matrix[i] = (int *)malloc(sizeof(int) * n);
    }
    for (int i = 0; i < n; ++i)
    {
        for (int j = 0; j < n; ++j)
        {
            fscanf(file, "%d", &dfsReqs.adjacency_matrix[i][j]);
        }
    }
    fclose(file);
    //at this stage, we have the first 3 elements of dfsReqs
    //now we will do dfs
    startDFS(&dfsReqs);

    if (shmdt(shmptr) == -1)
    {
        perror("shmdt");
        exit(EXIT_FAILURE);
    }
}


int main(int argc,char const *argv[])
{
    /*//Handling server numbers
    int server_num = atoi(argv[1]);
    server_num %= 2;
    if(server_num%2 ==0) server_num +=2;*/

    struct mesg_buffer buf;
    struct mesg_buffer buf2;
    key_t key;
    int msgid;
    if((key=ftok("load_balancer.c",'B'))==-1)
    {
        perror("ftok failed");
        exit(1);
    }
    if((msgid= msgget(key,PERMS))==-1)
    {
        perror("msgget failed");
        exit(1);
    }

    while(1)
    {
        if(msgrcv(msgid,&buf,sizeof(buf.mesg_text),0,0)==-1)
        {
            printf("%s", buf.mesg_text);
            perror("msgrcv");
            exit(1);
        }
        switch(buf.mesg_type)
        {
            case 3:
                pthread_t tid;
                pthread_attr_t attr;
                pthread_attr_init(&attr);
                if(pthread_create(&tid, &attr, dfsStartRoutine, (void *)buf.mesg_text) != 0)  //creating a new thread to service this request
                {
                    perror("pthread create failed");
                    return 1;
                }
                
                char dfs_rd_done[] = "DFS reading done";
                strcpy(buf2.mesg_text, dfs_rd_done);
                if(msgsnd(msgid,&buf2,strlen(buf2.mesg_text)+1,0)==-1)
                {
                        perror("msgsnd");
                        exit(1);
                }
                if(pthread_join(tid, NULL) != 0)
                {
                    perror("join error");
                    return 1;
                }
                break;
            
            
            case 4:
        }  
    }    
    
    return 0;   
}

