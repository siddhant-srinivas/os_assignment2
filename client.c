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

#define PERMS 0644  

struct mesg_content{
	long sequence_num;
	char mesg_text[100];
}; 
typedef struct mesg_content mesg_content;
struct mesg_buffer{
    long mesg_type;
    // long seq_num;
    mesg_content mesg_cont;
};

int main(int argc,char const *argv[]){

    key_t msg_key;
    key_t shm_key;
    int msgid;

    if((msg_key=ftok("load_balancer.c",'B'))==-1){         //message queue
        perror("ftok failed");
        exit(1);
    }
    if((msgid= msgget(msg_key,PERMS))==-1){                //message queue
        perror("msgget failed");
        exit(1);
    }
    
    int sequence_num;
    int operation_num;
    char graph_fn[100];
    
    char menu_options[] = "1. Add a new graph to the database\n2. Modify an existing graph of the database\n3. Perform DFS on an existing graph of the database\n4. Perform BFS on an existing graph of the database\n\n";

    

    char prompt[] = "Enter Sequence number\nEnter Operation Number\nEnter graph file name\n\n";
    char write_prompt[] = "Enter number of nodes of the graph\nEnter adjacency matrix, each row on a separate line and elements of a single row separated by whitespace characters\n\n";
    char read_prompt[] = "Enter starting vertex\n\n";

    while(1)
    {
        struct mesg_buffer buf; 
        struct mesg_buffer buf2;
        int nodes;
        int starting_vertex;
		printf("%s", menu_options);
       printf("Enter Sequence Number: ");
		scanf("%d", &sequence_num);
		printf("Enter Operation Number: ");
		scanf("%d",&operation_num);
		fflush(stdout);
		printf("Enter Graph File Name: ");
		scanf("%s[^\n]",graph_fn);
		printf("%d, %d, %s\n",sequence_num,operation_num,graph_fn);
        int shm_arg = sequence_num + 48;
        
        int shmid;
	int BUF_SIZE = sizeof(int) * 900;
	int *shmptr;
        
        switch(operation_num){
            case 1:
            case 2:	buf.mesg_type = operation_num;
			buf.mesg_cont.sequence_num = sequence_num;
		        strcpy(buf.mesg_cont.mesg_text, graph_fn);
		        printf("Enter the number of nodes of the graph: ");
		        scanf("%d", &nodes);
				printf("Enter adjacency matrix, each row on  a seperate line and elements of a single separated by whitespace character:\n");
		        int** adj_matrix = (int**)malloc(nodes * sizeof(int*));
		        if(adj_matrix == NULL){
		            perror("malloc failed");
		            exit(1);
		        }

		        for(int i = 0; i < nodes; i++){
		            adj_matrix[i] = (int*)malloc(nodes * sizeof(int));
		            if(adj_matrix[i] == NULL){
		                perror("malloc failed");
		                exit(1);
		            }
		        }
		        for(int i = 0; i < nodes; i++){
		            for(int j = 0; j < nodes; j++){
		                scanf("%d", &adj_matrix[i][j]);
		            }
		        }

		        //Creating shared memory segment
		        if((shm_key=ftok("client.c",'D'))== -1){
		            perror("ftok failed");
		            exit(1);
		        }
		        
		        shmid = shmget(shm_key,BUF_SIZE,PERMS|IPC_CREAT);
		        if(shmid == -1){
		            perror("SHM error");
		            return 1;
		        }
		        shmptr = (int *)shmat(shmid, NULL, 0);
		        if(shmptr == (int*)-1){
		            perror("SHMPTR ERROR");
		            return 1;
		        }
		        *shmptr = nodes;
		        int *ptr = shmptr;  //Adjacency matrix values
				shmptr++;
		        for(int i = 0; i < nodes; i++){
		            for(int j = 0; j < nodes; j++){
		                *(shmptr) = adj_matrix[i][j];
		                shmptr++;
		            }
		        }
				shmptr = ptr;
		        if(shmdt(shmptr) == -1){
		            perror("shmdt failed");
		            return 1;
		        }
		        if(msgsnd(msgid,&buf,sizeof(buf.mesg_cont),0)==-1){
		            perror("msgsnd error");
		            exit(1);
		        }
		        if(msgrcv(msgid,&buf2,sizeof(buf2.mesg_cont),0,0)==-1){
		            perror("msgrcv error");
		            exit(1);
		        }
		        printf("%s\n", buf2.mesg_cont.mesg_text);
		        if(shmctl(shmid,IPC_RMID,0)==-1){
		            perror("shmctl failed");
		            return 1;
		        }
		    	break;
            
            case 3:	buf.mesg_type = operation_num;
		    	//buf.seq_num = sequence_num;
		        strcpy(buf.mesg_cont.mesg_text, graph_fn);
		        printf("%s", read_prompt);
		        scanf("%d", &starting_vertex);
		        
		        
		        if((shm_key=ftok("client.c",'D'))== -1){
		            perror("ftok failed");
		            exit(1);
		        }
		        
		        shmid = shmget(shm_key,BUF_SIZE,PERMS|IPC_CREAT);
		        if(shmid == -1){
		            perror("SHM error");
		            return 1;
		        }
		        shmptr = (int *)shmat(shmid, NULL, 0);
		        if(shmptr == (int*)-1){
		            perror("SHMPTR ERROR");
		            return 1;
		        }
		        *shmptr = starting_vertex;
		        
		        if(shmdt(shmptr) == -1){
		            perror("shmdt failed");
		            return 1;
		        }
		        if(msgsnd(msgid,&buf,sizeof(buf.mesg_cont),0)==-1){
		            perror("msgsnd error");
		            exit(1);
		        }
		        if(msgrcv(msgid,&buf2,sizeof(buf2.mesg_cont),0,0)==-1){
		            perror("msgrcv error");
		            exit(1);
		        }
		        printf("%s", buf2.mesg_cont.mesg_text);
		        if(shmctl(shmid,IPC_RMID,0)==-1){
		            perror("shmctl failed");
		            return 1;
		        }
		    	break;
        }
    }

}
