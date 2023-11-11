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
int main(int argc,char const *argv[]){

    key_t msg_key;
    key_t shm_key;
    int len;
    int msgid;
    if((msg_key=ftok("load_balancer.c",'B'))==-1){
        perror("ftok failed");
        exit(1);
    }
    if((msgid= msgget(msg_key,PERMS))==-1){
        perror("msgget failed");
        exit(1);
    }

    char menu_options[] = "1. Add a new graph to the database\n2. Modify an existing graph of the database\n3. Perform DFS on an existing graph of the database\n4. Perform BFS on an existing graph of the database\n\n";
    
    int sequence_num;
    int operation_num;
    char graph_fn[100];

    int nodes;
    int **adj_matrix;

    printf("%s", menu_options);

    char prompt[] = "Enter Sequence number\nEnter Operation Number\nEnter graph file name\n\n";
    char write_prompt[] = "Enter number of nodes of the graph\nEnter adjacency matrix, each row on a separate line and elements of a single row separated by whitespace characters\n\n";
    char read_prompt[] = "Enter starting vertex\n\n";

    while(1){
        struct mesg_buffer buf; 
        struct mesg_buffer buf2;
        printf("%s", prompt);
        scanf("%d %d", &sequence_num, &operation_num);
        fgets(graph_fn, 100, stdin);    //Error with this line, replace it with anything taking char*[100] as string input
        switch(operation_num){
            case 1:
            case 2:
                buf.mesg_type = operation_num;
                strcpy(buf.mesg_text, graph_fn);
                printf("%s", write_prompt);
                scanf("%d", &nodes);
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
                *shmptr = nodes;
                int *ptr = shmptr + 1;  //Adjacency matrix values
                for(int i = 0; i < nodes; i++){
                    for(int j = 0; j < nodes; j++){
                        *(ptr) = adj_matrix[i][j];
                        ptr++;
                    }
                }
                if(shmdt(shmptr) == -1){
                    perror("shmdt failed");
                    return 1;
                }
                if(shmctl(shmid,IPC_RMID,0)==-1){
                    perror("shmctl failed");
                    return 1;
                }
                if(msgsnd(msgid,&buf,strlen(buf.mesg_text)+1,0)==-1){
                        perror("msgsnd error");
                        exit(1);
                }
                if(msgrcv(msgid,&buf2,sizeof(buf2.mesg_text),0,0)==-1){
                    perror("msgrcv error");
                    exit(1);
                }
                printf("%s", buf2.mesg_text);
            break;
        }
    }

}