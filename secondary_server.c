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
#define MSG_TYPE 5 

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
struct dfsStruct        //This is the stuff we need in order to do dfs. Using struct because we can pass only 1 arg in thread creation
{
    int n;
    int starting_vertex;
    int **adjacency_matrix;
    int *num_neighbours;
    int *visited;
    pthread_mutex_t mutex;
};

struct bfsSturct{
    int n;
    int starting_vertex;
    int **adjacency_matrix;
    int queue[50];
    int front;
    int back;
    int* visited;
    pthread_mutex_t mutex;
};

struct thread_args{
    int msgid;
    char graph_fn[20];
    int server_num;
};

void* dfs(void* args)
{  
    struct dfsStruct* dfsReqs = (struct dfsStruct*)args;
    int currentNode = dfsReqs->starting_vertex; //-1 becasue array indexing starts from 0
    pthread_mutex_lock(&dfsReqs->mutex);   //locking mutex (visited array is the critical section)
    printf("Visiting node: %d\n", currentNode + 1);  //because we had done -1 above
    dfsReqs->visited[currentNode] = 1;
    pthread_mutex_unlock(&dfsReqs->mutex); //unlocking mutex
    
    for (int i = 0; i < dfsReqs->n; i++)    //exploring adjacent nodes
    {
        if (!dfsReqs->visited[i] && dfsReqs->adjacency_matrix[currentNode][i])
        {
            struct dfsStruct* dfsReqsChild = malloc(sizeof(struct dfsStruct));
            memcpy(dfsReqsChild, dfsReqs, sizeof(struct dfsStruct));
            dfsReqsChild->starting_vertex = i; // again, similar reason as before

            pthread_t tid;
	    pthread_attr_t attr;
	    pthread_attr_init(&attr);
	    if(pthread_create(&tid, &attr, dfs, dfsReqsChild) != 0)  //creating a new thread for the unvisited node
	    {
	    	perror("pthread create failed");
		pthread_exit(NULL);
	    }
            if(pthread_join(tid, NULL) != 0)
	    {
		perror("join error");
	    	pthread_exit(NULL);
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
        pthread_exit(NULL);
    }
    if(pthread_join(tid, NULL) != 0)
    {
        perror("join error");
    	pthread_exit(NULL);
    }
    
    pthread_mutex_destroy(&dfsReqs->mutex);
    free(dfsReqs->visited);
}


void* dfsStartRoutine(void *arg)
{

    struct dfsStruct dfsReqs;
   struct mesg_buffer buf2;
    key_t shm_key;
    struct thread_args threadArgs = *(struct thread_args*)arg;
    char graph_fn[20];
    int msgid = threadArgs.msgid;
    strcpy(graph_fn,threadArgs.graph_fn);
    int server_num = threadArgs.server_num;
    char key_letter = server_num%2==0 ? 'E' : 'F';
    if((shm_key=ftok("client.c",key_letter))== -1)
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
    printf("Read value from shared memory: %d\n", *shmptr);
    dfsReqs.starting_vertex = *shmptr;
  
    dfsReqs.starting_vertex--;
    
    FILE* file = fopen(graph_fn, "r");
    if (file == NULL)
    {
        perror("Error opening file");
        pthread_exit(NULL);
    }
    fscanf(file, "%d", &dfsReqs.n);

    int n = dfsReqs.n;
    dfsReqs.num_neighbours = (int *)calloc(n,sizeof(int));
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
    for(int i=0;i<n;i++){
        for(int j=0;j<n;j++){
            if(dfsReqs.adjacency_matrix[i][j] == 1)
            dfsReqs.num_neighbours[i]++;
        }
    }
    printf("The number of neighbours for each node are: \n");
    for(int i=0;i<n;i++){
        printf("%d ",dfsReqs.num_neighbours[i]);
    }
   printf("\n");
      startDFS(&dfsReqs);
    

  

    if (shmdt(shmptr) == -1)
    {
        perror("shmdt");
        exit(EXIT_FAILURE);
    }

     char dfs_rd_done[] = "DFS reading done";
                strcpy(buf2.mesg_cont.mesg_text, dfs_rd_done);
                 buf2.mesg_cont.sequence_num = threadArgs.server_num;
    		buf2.mesg_cont.operation_num = 3;
	    	buf2.mesg_type = MSG_TYPE;
                if(msgsnd(msgid,&buf2,sizeof(buf2.mesg_cont),0)==-1)
                {
                        perror("msgsnd");
                        exit(1);
                }
}


int main(int argc,char const *argv[])
{
    //Handling server numbers
    int server_num = atoi(argv[1]);
    server_num %= 2;
    if(server_num%2 ==0) server_num +=2;
    printf("Secondary Server %d Running! Established Communication with the Message Queue\n",server_num);
    struct mesg_buffer buf;
    
    key_t key;
    int msgid;
    int listen_no = server_num%2==0 ? 4 : 6;
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
        if(msgrcv(msgid,&buf,sizeof(buf),listen_no,0)==-1)
        {
            perror("msgrcv");
            exit(1);
        }
 //printf("Received Message from: %ld\n",buf.mesg_type);
 if(buf.mesg_cont.sequence_num%server_num!=0) continue;
 
        printf("Sequence Number: %ld\n",buf.mesg_cont.sequence_num);
        printf("Contents: %s\n", buf.mesg_cont.mesg_text);
        pthread_t tid;
        pthread_attr_t attr;
        struct thread_args args;
        switch(buf.mesg_cont.operation_num)
        {
            case 3:
           
                pthread_t tid;
                pthread_attr_t attr;
                struct thread_args args;
                strcpy(args.graph_fn, buf.mesg_cont.mesg_text);
                args.server_num = buf.mesg_cont.sequence_num;
                args.msgid = msgid;
                pthread_attr_init(&attr);
                if(pthread_create(&tid, &attr, dfsStartRoutine, (void *)&args) != 0)  //creating a new thread to service this request
                {
                    perror("pthread create failed");
                    return 1;
                }
                if(pthread_join(tid, NULL) != 0)
                {
                    perror("join error");
                    return 1;
                }
               
                
                break;
            
            
            case 4:
             
                strcpy(args.graph_fn, buf.mesg_cont.mesg_text);
                args.server_num = buf.mesg_cont.sequence_num;
                args.msgid = msgid;
                pthread_attr_init(&attr);
                if(pthread_create(&tid, &attr, dfsStartRoutine, (void *)&args) != 0)  //creating a new thread to service this request
                {
                    perror("pthread create failed");
                    return 1;
                }
                if(pthread_join(tid, NULL) != 0)
                {
                    perror("join error");
                    return 1;
                }
                break;
            case 10:
                printf("Secondary server %d terminated\n", server_num);
                exit(0);
            default:
                break;
        }  
    }    
    
    return 0;   
}

