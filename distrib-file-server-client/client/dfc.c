#include <arpa/inet.h> 
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h> 
#include <sys/socket.h>
#include <unistd.h>
#include <openssl/md5.h>
#include <sys/time.h>  // for struct timeval
#define MAX_SERVERS 4
#define SA struct sockaddr
#define MAX_LIST_ENTRIES 100

typedef struct{
    int chunks[MAX_SERVERS];
    char name[100];

}list_entry;

typedef struct{

    char ip[30];
    int port;

}server;

long int put_file_length(char* filename, FILE** file){

    *file = fopen(filename,"rb+");

    if(!*file){
        
        return -1;
    }

    fseek(*file,0,SEEK_END);

    long int length = ftell(*file);

    fseek(*file,0,SEEK_SET);

    return length;

}
//based on geeks4geeks connection protocol + past PA's
int connect_client(char* ip, int port){

    int sockfd, connfd;
    struct sockaddr_in servaddr, cli;

    // socket create and verification
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        
        return -1;
    }
    else
        
    bzero(&servaddr, sizeof(servaddr));

    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(ip);
    servaddr.sin_port = htons(port);

    // connect the client socket to server socket
    if (connect(sockfd, (SA*)&servaddr, sizeof(servaddr))!= 0) {
        
        return -1;
    }

    
    return sockfd;
}

int serve_put(FILE* file,server servers[],int length, int chunk_decider,char* filename){

    //loop through servers, if cannot connect, skip and increase bad_count
    //if bad_count > 1, cannot service put request
    //send chunks according to chunk decider found earlier

    int bad_server_count =0;
    int bad_servers[MAX_SERVERS];

    for(int i=0;i<MAX_SERVERS;i++){
        bad_servers[i] = 0;
    }

    int chunk_pairs[4][2] = {{1,2},{2,3},{3,4},{1,4}};

    for(int i=0;i<MAX_SERVERS;i++){

        if(bad_server_count > 1){
            return 1;
        }

        int socket;

        if((socket = connect_client(servers[i].ip,servers[i].port)) < 0){
            
            bad_server_count++;
            continue;
        }

        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

        send(socket,"put",10,0);

        char ack[10];
        int ack_size;

        ack_size=recv(socket,ack,3,0);

        if(ack_size == -1){
            printf("put ack timeout\n");
            bad_server_count ++;
            continue;
        }
        
        int* chunk_pair = chunk_pairs[(i+chunk_decider)%4];

        int quarter = (int) (length/4);
        
        for(int i=0;i<2;i++){

            int chunk1 = chunk_pair[i] -1;
            int q1;
            int c_length;

            if(chunk1 == 3){
                c_length = length - 3 * quarter;
            }
            else{
                c_length = quarter;
            }

            char q1_buff[100];

            sprintf(q1_buff,"%d",c_length);

            char f1[200];

            sprintf(f1,"%s",filename);

        

            sprintf(f1 + strlen(f1),"%d",chunk1);
            
            

            sprintf(f1 + strlen(f1),"\n%s",q1_buff);
           
            send(socket,f1,strlen(f1),0);

            char head_ack[10];
            int header;
            
            header = recv(socket,head_ack,10,0);
            
            if(header == -1){
                printf("header timeout\n");
                bad_server_count++;
                break;
            }
            
            //ready to send file contents
            //send first chunk
            
            fseek(file,chunk1*quarter,SEEK_SET);
            
            char file_buffer[2000];
            int byte_sent;

            while(c_length > 0){

                if(c_length < 2000){
                    
                    
                    fread(file_buffer,1,c_length,file);
                    
                    byte_sent = send(socket,file_buffer,c_length,0);

                }
                else{

                    fread(file_buffer,1,2000,file);
                    byte_sent = send(socket,file_buffer,2000,0);

                }

                c_length -= byte_sent;

            }
            
            fseek(file,0,SEEK_SET);


        
        }
        
    }
    
    fclose(file);
    return 0;
    



}

int client_put(int argc,char* argv[],server servers[]){


    int file_count = argc - 2; //gets rid of ./dfc and put
    int fail =0;
    
    for(int i=0;i<file_count;i++){

        long int length;
        FILE* file;
        
        if((length = put_file_length(argv[i+2],&file)) < 0){ //opens file and gets length
            printf("Could not open file\n");
            return 1;
        }
        
        //use md5 to hash the filename,then copy it into int
        char hash_file[MD5_DIGEST_LENGTH];
        unsigned long hash_val;

        
        MD5((char *)argv[i+2], strlen(argv[i+2]), hash_file);
        memcpy(&hash_val, hash_file, sizeof(unsigned long));
        

        int chunk_decider = hash_val % MAX_SERVERS;
        int res = serve_put(file,servers,length,chunk_decider,argv[i+2]);
        
        if(res){
            fail = 1;
            fclose(file);
            printf("%s put failed\n",argv[i+2]);
        }
        

        
    }
    
    return fail;
}

int serve_get(char* hash_filename,server servers[],char* filename){

    int bad_server_count =0;
    int chunks[MAX_SERVERS];

    for(int i=0;i<MAX_SERVERS;i++){
        chunks[i] = 0;
    }

    FILE* file;
    file = fopen(filename,"wb+");

    if(!file){

        printf("Couldnt open file in get\n");
        return 1;
    }


    for(int i=0;i<MAX_SERVERS;i++){

        int socket;

        if((socket = connect_client(servers[i].ip,servers[i].port)) < 0){
            
            bad_server_count++;
            continue;
        }

        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

        send(socket,"get",10,0);

        char ack[10];
        int ack_size;

        ack_size=recv(socket,ack,1,0);

        if(ack_size == -1){
            bad_server_count++;
            continue;
        }

        send(socket,filename,100,0);

        int ack_rec;
        char ack_rec_buff[3];

        ack_rec = recv(socket,ack_rec_buff,3,0);

        if(ack_rec == -1){
            bad_server_count++;
            continue;
        }

        int file_size_rec;
        char file_size_q[10];

        file_size_rec = recv(socket,file_size_q,10,0);

        if(file_size_rec==-1){
            bad_server_count++;
            continue;
        }
       
        

        int quarter_size = atoi(file_size_q);
        for(int i=0;i<2;i++){

            int chunk_des_ack;
            char chunk_des[1];

            chunk_des_ack = recv(socket,chunk_des,1,0);

            if(chunk_des_ack == -1){
                bad_server_count++;
                break;
            }

            int chunk_int = atoi(chunk_des);
            
           
            int start_pos = chunk_int * quarter_size;

            int file_size_ack;
            char file_size[10];
            memset(file_size, 0, 10);
            file_size_ack = recv(socket,file_size,10,0);

            
            if(file_size_ack == -1){
                bad_server_count++;
                break;
            }

            long f_length = atol(file_size);

            fseek(file,start_pos,SEEK_SET);
            char file_buffer[2000];
            int file_content_ack;
            int read_size;
            //printf("%ld\n",f_length);
            //printf("%d\n",start_pos);
            while(f_length > 0){

              
                memset(file_buffer, 0, 2000);
                if(f_length < 2000){
                    read_size = f_length;
                }
                else{
                    read_size = 2000;
                }

                file_content_ack = recv(socket,file_buffer,read_size,0);

                if(file_content_ack == -1){
                    bad_server_count++;
                    break;
                }
                
                fwrite(file_buffer, 1, file_content_ack, file);
            

                f_length -= file_content_ack;

                
            }
            
            
            
            
        }


      
    
    }
    fclose(file);
    return 0;

}


int client_get(int argc,char* argv[], server servers[]){

    int file_count = argc -2;
    int fail = 0;
    for(int i=0;i<file_count;i++){

        //query servers for file
        //get all four chunks
        //write the four chunks in order
        //saved in server as hash + chunk number

        char hash_file[MD5_DIGEST_LENGTH];
        unsigned long hash_val;

            
        MD5((char *)argv[i+2], strlen(argv[i+2]), hash_file);
        memcpy(&hash_val, hash_file, sizeof(unsigned long));

        char hash_str[100];
        sprintf(hash_str,"%ld",hash_val);

        int res = serve_get(hash_str,servers,argv[i+2]);

        if(res){
            fail = 1;
        }

    }

    return fail;
}




int client_list(int argc, char* argv[], server servers[]){

    list_entry list[MAX_LIST_ENTRIES];
    int index =0;
    for(int i=0;i<MAX_SERVERS;i++){
        int socket;

        if((socket = connect_client(servers[i].ip,servers[i].port)) < 0){
            
            continue;
        }

        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

        send(socket,"list",10,0);

        while(1){
            char file_name[100];
            int num_rec;
            num_rec = recv(socket,file_name,100,0);
            
            if(num_rec == -1){
                break;
            }

            if(strcmp(file_name,"done")==0){
                break;
            }

            int chunk = atoi(&file_name[strlen(file_name)-1]);
            
            file_name[strlen(file_name)-1] = '\0';
            int in_list = 0;
            for(int i=0;i<index;i++){

                if(strcmp(list[i].name,file_name)==0){
                    
                    list[i].chunks[chunk] = 1;
                    in_list = 1;
                }


        
            }

            if(!in_list){

                list_entry entry;

                for(int i=0;i<MAX_SERVERS;i++){
                    entry.chunks[i] = 0;
                }
                strcpy(entry.name,file_name);
                entry.chunks[chunk] = 1;
                list[index] = entry;
                index++;
            }
            send(socket,"ack",3,0);

        }

    
    }//list all
    if(argc == 2){
        for(int i=0;i<index;i++){

            int incomplete = 0;
    
            for(int j=0;j<MAX_SERVERS;j++){
                if(list[i].chunks[j]==0){
                    incomplete = 1;
                }
    
            }
            if(incomplete){
                printf("%s is incomplete\n",list[i].name);
            }
            else{
                printf("%s\n",list[i].name);
            }
        }

    }//only list those in the argumetns
    else{
        for(int i=0;i<index;i++){

            for(int j=0;j<argc-2;j++){
                
                
                if(strcmp(list[i].name,argv[j+2])==0){

                    int incomplete = 0;
    
                    for(int j=0;j<MAX_SERVERS;j++){
                        if(list[i].chunks[j]==0){
                            incomplete = 1;
                        }
            
                    }
                    if(incomplete){
                        printf("%s is incomplete\n",list[i].name);
                    }
                    else{
                        printf("%s\n",list[i].name);
                    }


                }


            }
        }
    }
    

    return  0;
}   

int server_parser(server servers[]){

    char* config = "~/dfc.conf";
    char* config2 = "dfc.conf";


    FILE* config_file = fopen(config,"r");
    if(!config_file){

        config_file = fopen(config2,"r");

        if(!config_file){
            printf("Could not get config for servers\n");
            return 1;
        }
    }

    char line[100];
    
    int count =0;
    while (fgets(line,sizeof(line),config_file)){
        
        if(count >= MAX_SERVERS){
            
            break;
        }

        char *token = strtok(line, " ");     
        token = strtok(NULL, " ");      
        char *ip_port = strtok(NULL, " ");    

        char *ip = strtok(ip_port, ":");      
        char *port = strtok(NULL, ":");   


        strcpy(servers[count].ip,ip);
        servers[count].port = atoi(port);
        
       
        count++;

    }


    fclose(config_file);
    return 0;
}

int main(int argc, char* argv[]){

    if(argc < 2){

        printf("Invalid arguments\n");
        return 1;
    }

    char* command = argv[1];

   

    server servers[MAX_SERVERS];
    if(server_parser(servers)){
        printf("couldnt parse server config\n");
        return 1;
    }

    /*
    for(int i=0;i<MAX_SERVERS;i++){

        printf("%s\n",servers[i].ip);
        printf("%d\n",servers[i].port);
    }*/

    

    if(strcmp(command,"get") == 0 && argc > 2){


        return client_get(argc,argv,servers);

    }
    else if(strcmp(command,"list")==0){

        return client_list(argc,argv,servers);
    }
    else if(strcmp(command,"put") == 0 &&  argc > 2){

        return client_put(argc,argv,servers);

    }
    else{
        printf("Invalid command\n");
        return 1;
    }



}