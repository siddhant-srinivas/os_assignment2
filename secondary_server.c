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
    int buffer;
    int *visited;
    pthread_mutex_t mutex;
};

struct bfsStruct{
    int n;
    int starting_vertex;
    int **adjacency_matrix;
    int *queue;
    int front;
    int back;
    int buffer;
    int* visited;
    int* path;
    pthread_mutex_t mutex;
};

struct thread_args{
    int msgid;
    char graph_fn[20];
    int server_num;
};

void* bfs(void* args){
    struct bfsStruct* bfsReqs = (struct bfsStruct*)args;
    int row = bfsReqs->queue[bfsReqs->front];
        bfsReqs->path[(bfsReqs->buffer)++] = bfsReqs->queue[bfsReqs->front++]+1;
        for(int i = 0; i < bfsReqs->n; i++){
            
            if (bfsReqs->adjacency_matrix[row][i] && !bfsReqs->visited[i]){
                bfsReqs->queue[(bfsReqs->back)++] = i;
                bfsReqs->visited[i] = 1;
            }
        }
}
void* bfsInit(void* args)
{  printf("\n");
    struct bfsStruct* bfsReqs = (struct bfsStruct*)args;
    int startNode = bfsReqs->starting_vertex; //-1 becasue array indexing starts from 0
    bfsReqs->queue[(bfsReqs->back)++] = startNode;
    pthread_mutex_lock(&bfsReqs->mutex);   //locking mutex (visited array is the critical section)
    bfsReqs->visited[startNode] = 1;
    pthread_mutex_unlock(&bfsReqs->mutex); //unlocking mutex
    
    while((bfsReqs->front) < (bfsReqs->back))   //exploring adjacent dfs_nodes
    {   
         int size = (bfsReqs->back)-(bfsReqs->front);
          pthread_t tid[size];
	    pthread_attr_t attr[size];
        for(int j=0;j<size;j++){
	    pthread_attr_init(&attr[j]);
        }
        for(int j=0;j<size;j++){
            if(pthread_create(&tid[j], &attr[j], bfs, bfsReqs) != 0){
	    	perror("pthread create failed");
		    pthread_exit(NULL);
	         }
        }
          for(int j=0;j<size;j++){
             if(pthread_join(tid[j], NULL) != 0){
		        perror("join error");
	    	    pthread_exit(NULL);
	        }
          } 
          
        }

       
    
    pthread_exit(NULL);
}
void startBFS(struct bfsStruct* bfsReqs)
{   printf("\n");
    pthread_mutex_init(&bfsReqs->mutex, NULL);                          //initializing the visited and mutex of the struct
    bfsReqs->visited = (int*)calloc(bfsReqs->n, sizeof(int));

    pthread_t tid;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    if(pthread_create(&tid, &attr, bfsInit, bfsReqs) != 0)  //creating a new thread to service this request
    {
    	perror("pthread create failed");
        pthread_exit(NULL);
    }
    if(pthread_join(tid, NULL) != 0)
    {
        perror("join error");
    	pthread_exit(NULL);
    }
    
    pthread_mutex_destroy(&bfsReqs->mutex);
    free(bfsReqs->visited);
}
void* bfsStartRoutine(void *arg)
{
    sem_t *sem_read;
    sem_t *sem_mutex;
    struct bfsStruct bfsReqs;
   struct mesg_buffer buf2;
    key_t shm_key;
    struct thread_args threadArgs = *(struct thread_args*)arg;
    char graph_fn[20];
    int msgid = threadArgs.msgid;
    strcpy(graph_fn,threadArgs.graph_fn);

     if((sem_mutex = sem_open("sem_mutex",0))==SEM_FAILED){
        perror("sem open error");
        exit(1);
    }           
       
    int server_num = threadArgs.server_num;
    char key_letter = server_num%2==0 ? 'E' + threadArgs.server_num : 'F' + threadArgs.server_num;
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
    bfsReqs.starting_vertex = *shmptr;
  
    bfsReqs.starting_vertex--;
    sem_wait(sem_mutex);

    FILE* file = fopen(graph_fn, "r");
    if (file == NULL)
    {
        perror("Error opening file");
        pthread_exit(NULL);
    }
    fscanf(file, "%d", &bfsReqs.n);

    int n = bfsReqs.n;
    bfsReqs.front = 0;
    bfsReqs.back = 0;
    bfsReqs.buffer = 0;
    bfsReqs.queue = (int *)calloc(n,sizeof(int));
    bfsReqs.path = (int *)calloc(n,sizeof(int));
    bfsReqs.adjacency_matrix = (int **)malloc(sizeof(int *) * n);
    for (int i = 0; i < n; ++i)
    {
        bfsReqs.adjacency_matrix[i] = (int *)malloc(sizeof(int) * n);
    }
    for (int i = 0; i < n; ++i)
    {
        for (int j = 0; j < n; ++j)
        {
            fscanf(file, "%d", &bfsReqs.adjacency_matrix[i][j]);
        }
    }
    
    fclose(file);
    sem_post(sem_mutex);
    //at this stage, we have the first 3 elements of dfsReqs
    //now we will do dfs

      startBFS(&bfsReqs);
    

  

    if (shmdt(shmptr) == -1)
    {
        perror("shmdt");
        exit(EXIT_FAILURE);
    }

     char bfs_rd_done[20];
     strcpy(bfs_rd_done,"");
     for(int i=0;i<bfsReqs.n;i++){
        char temp[10];
        sprintf(temp,"%d",bfsReqs.path[i]);
        strcat(bfs_rd_done,temp);
        if(i!=(bfsReqs.n)-1) strcat(bfs_rd_done," ");
     }
                strcpy(buf2.mesg_cont.mesg_text, bfs_rd_done);
                 buf2.mesg_cont.sequence_num = threadArgs.server_num;
    		buf2.mesg_cont.operation_num = 3;
	    	buf2.mesg_type = MSG_TYPE;
                if(msgsnd(msgid,&buf2,sizeof(buf2.mesg_cont),0)==-1)
                {
                        perror("msgsnd");
                        exit(1);
                }
}


int dfs_nodes[30];
int buffer = 0;
void* dfs(void* args)
{   
    struct dfsStruct* dfsReqs = (struct dfsStruct*)args;
    int currentNode = dfsReqs->starting_vertex; //-1 becasue array indexing starts from 0
    pthread_mutex_lock(&dfsReqs->mutex);   //locking mutex (visited array is the critical section)
    dfsReqs->visited[currentNode] = 1;
    pthread_mutex_unlock(&dfsReqs->mutex); //unlocking mutex
    int isLeafNode = 1;
    for(int i=0;i<dfsReqs->n;i++){
        if(i!=currentNode && (dfsReqs->adjacency_matrix)[currentNode][i]==1){
              if(!dfsReqs->visited[i]){
            isLeafNode *=0;
        }
        }
      
    }
    if(isLeafNode){
        printf("%d is a leaf node\n",currentNode+1);
         dfs_nodes[buffer++] = currentNode+1;
    }
    for (int i = 0; i < dfsReqs->n; i++)    //exploring adjacent dfs_nodes
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
    sem_t *sem_read;
    sem_t *sem_mutex;
    struct dfsStruct dfsReqs;
   struct mesg_buffer buf2;
    key_t shm_key;
    for(int i=0;i<30;i++){
        dfs_nodes[i] = 0;
    }
    struct thread_args threadArgs = *(struct thread_args*)arg;
    char graph_fn[20];
    int msgid = threadArgs.msgid;
    strcpy(graph_fn,threadArgs.graph_fn);
    int server_num = threadArgs.server_num;
        if((sem_mutex = sem_open("sem_mutex",0))==SEM_FAILED){
        perror("sem open error");
        exit(1);
    }           
    char key_letter = server_num%2==0 ? 'E' + server_num : 'F' + server_num;
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
       sem_wait(sem_mutex);
    FILE* file = fopen(graph_fn, "r");
    if (file == NULL)
    {
        perror("Error opening file");
        pthread_exit(NULL);
    }
    fscanf(file, "%d", &dfsReqs.n);

    int n = dfsReqs.n;
    dfsReqs.buffer = 0;
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
       sem_post(sem_mutex);
    //at this stage, we have the first 3 elements of dfsReqs
    //now we will do dfs
   
  
      startDFS(&dfsReqs);
    

  

    if (shmdt(shmptr) == -1)
    {
        perror("shmdt");
        exit(EXIT_FAILURE);
    }

     char dfs_rd_done[40];
        strcpy(dfs_rd_done,"");
        for(int i=0;i<(buffer);i++){
            char temp[10];
    sprintf(temp,"%d",dfs_nodes[i]);
    strcat(dfs_rd_done,temp);
    if(i!=buffer-1) strcat(dfs_rd_done," ");
}
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
   
    printf("Named Semaphores initalized\n");
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
    printf("Listening to messages of type: %d\n",listen_no);
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
                if(pthread_create(&tid, &attr, bfsStartRoutine, (void *)&args) != 0)  //creating a new thread to service this request
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
            struct mesg_buffer buf2;
                char cont[] = "Terminating";
                buf2.mesg_type = listen_no;
				buf2.mesg_cont.operation_num = 10;
				buf2.mesg_cont.sequence_num = -1;
				strcpy(buf2.mesg_cont.mesg_text, cont);
                if(msgsnd(msgid,&buf2,sizeof(buf2.mesg_cont),0)==-1){
					perror("msgsnd");
					exit(1);
				}
                printf("Secondary server %d terminated\n", server_num);
                exit(0);
            default:
                break;
        }  
    }    
    
    return 0;   
}

