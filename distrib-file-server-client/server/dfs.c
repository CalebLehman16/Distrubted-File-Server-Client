#include<stdio.h>
#include<string.h>	//strlen
#include<stdlib.h>	//strlen
#include<sys/socket.h>
#include<arpa/inet.h>	//inet_addr
#include<unistd.h>	//write
#include<pthread.h> //for threading , link with lpthread
#include <sys/time.h>  // for struct timeval
#include <dirent.h>



//the thread function
void *connection_handler(void *);

typedef struct{

    int *socket_desc;
    char* directory;

}server_args;


int server_put(int sock, char* directory){
    
    //three reads, name of file, length of file, and then the actual content;
    printf("recieved put command\n");

    send(sock,"ack",3,0);

    for(int i=0;i<2;i++){

        int header_size;
        char header[2000]; //filename and content length;

        header_size = recv(sock,header,2000,0);

        
        send(sock,"ack",3,0);

        
        char* length_str = strchr(header,'\n') + 1;
        
        int length = atoi(length_str);

       
        char* filename_chunk = strtok(header,"\n");
        
        char full_file[200];

        sprintf(full_file,"%s/",directory);
        sprintf(full_file + strlen(full_file),"%s",filename_chunk);
        
        FILE* fp;
        fp = fopen(full_file,"wb+");
        
        if(fp){

            int content_read;
            char content_buffer[2000];

            while(length > 0){
                
                if(length < 2000){
                    
                   content_read = recv(sock,content_buffer,length,0);
                }
                else{
                   content_read = recv(sock,content_buffer,2000,0);
                }

                
                if(content_read > 0){
                    write(fileno(fp),content_buffer,content_read);
                }
                else{

                    printf("failed put during content read\n");
                    fclose(fp);
                    return 1;
                }

                length -= content_read;



            }


        }
        else{
           
            printf("couldnt open file for put\n");
            return 1;
        }
        fclose(fp);
    }
    
    return 0;

}

int server_get(int sock, char* directory){

    printf("recieved get command\n");

    send(sock,"a",1,0);

    char filename[100];

    int file_recv = recv(sock,filename,100,0);

    send(sock,"ack",3,0);
    
    DIR *dir = opendir(directory);
    
    if(!dir){
        printf("Could not open specified directory\n");
        return 1;
    }
    
    struct dirent* de;

    //goes through directory to give client the 1/4 of the size
    DIR *dir1 = opendir(directory);
    struct dirent* de1;

    while((de1 = readdir(dir1))!=NULL){

        if(strcmp("..",de1->d_name)==0 || strcmp(".",de1->d_name)==0){
            continue;
        }

        if(strncmp(filename,de1->d_name,strlen(filename))==0){
            
            
            if(de1->d_name[strlen(filename)] == '3'){
                continue;
            }

            FILE* size;
            char full_path[300];
            sprintf(full_path,"%s/%s",directory,de1->d_name);
            size = fopen(full_path,"rb+");
            
            fseek(size,0,SEEK_END);
            long size_f = ftell(size);
            fseek(size,0,SEEK_SET);
            fclose(size);
        
            char size_of_file[10];
            sprintf(size_of_file,"%ld",size_f);
            
            send(sock,size_of_file,10,0);
            break;

        }
    }
    rewinddir(dir1);
    closedir(dir1);
    while((de = readdir(dir))!=NULL){

        

        if(strcmp("..",de->d_name)==0 || strcmp(".",de->d_name)==0){
            continue;
        }

        if(strncmp(filename,de->d_name,strlen(filename))==0){
            
            send(sock,&de->d_name[strlen(filename)],1,0);

            FILE* file;
            
            char full_file_path[300];
            sprintf(full_file_path,"%s/%s",directory,de->d_name);
            file = fopen(full_file_path,"rb+");

            if(!file){
                continue;
            }
            
            fseek(file,0,SEEK_END);
            long f_size = ftell(file);
            fseek(file,0,SEEK_SET); 


            
            char file_length_buff[10];
            sprintf(file_length_buff,"%ld",f_size);
            //printf("%s\n",file_length_buff);
            send(sock,file_length_buff,10,0);

            char file_buffer[2000];
            int byte_sent;

            while(f_size > 0){
                memset(file_buffer, 0, 2000);
                int read_size;

                if(f_size < 2000){
                    read_size = f_size;
                }
                else{
                    read_size = 2000;
                }

                fread(file_buffer,1,read_size,file);
                //printf("%s\n",file_buffer);
                byte_sent = send(sock,file_buffer,read_size,0);

                if(byte_sent == -1){
                    break;
                }
                
                f_size -= byte_sent;



            }
            




            fclose(file);
            
        }


    }
    closedir(dir);
    return 0;


}

int server_list(int sock, char* directory){

    DIR *dir = opendir(directory);

    if(!dir){
        printf("Could not open specified directory\n");
        return 1;
    }
    
    struct dirent* de;
    //go through directory, try to find matching file, the last number should be the chunk
    
    while((de = readdir(dir))!= NULL){
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) {
            continue; 
        }
        
        send(sock,de->d_name,100,0);

        char ack[3];
        int ack_rec;
        ack_rec = recv(sock,ack,3,0);


    }
    send(sock,"done",100,0);
   
    return 0;
}

int main(int argc , char *argv[])
{
	int socket_desc , client_sock , c , *new_sock;
	struct sockaddr_in server , client;

    if(argc != 3){
        printf("invalid arguments\n");
        return 1;
    }
	
	//Create socket
	socket_desc = socket(AF_INET , SOCK_STREAM , 0);
	if (socket_desc == -1)
	{
		printf("Could not create socket\n");
	}
	puts("Socket created\n");
	
	//Prepare the sockaddr_in structure
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons( atoi(argv[2]));
	
	//Bind
	if( bind(socket_desc,(struct sockaddr *)&server , sizeof(server)) < 0)
	{
		//print the error message
		perror("bind failed. Error\n");
		return 1;
	}
	
	
	//Listen
	listen(socket_desc , 3);
	
	//Accept and incoming connection
	
	c = sizeof(struct sockaddr_in);
	
	
	//Accept and incoming connectionputs("Waiting for incoming connections...\n");
	c = sizeof(struct sockaddr_in);
	while( (client_sock = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c)) )
	{
		
		pthread_t sniffer_thread;
		new_sock = malloc(1);
		*new_sock = client_sock;
        server_args *args = malloc(sizeof(server_args));
        args->socket_desc = new_sock;
        args->directory = argv[1];

        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

        
		
		if( pthread_create( &sniffer_thread , NULL ,  connection_handler , (void*) args) < 0)
		{
			perror("could not create thread\n");
			return 1;
		}
		
		//Now join the thread , so that we dont terminate before the thread
		//pthread_join( sniffer_thread , NULL);
		
	}
	
	if (client_sock < 0)
	{
		perror("accept failed\n");
		return 1;
	}
	
	return 0;
}

/*
 * This will handle connection for each client
 * */
void *connection_handler(void *input_args)
{
	server_args* args = (server_args*) input_args;
    int* sock_desc = args->socket_desc;
    int sock = *sock_desc;
    char* directory = args->directory;
    char client_message[2000];
    int read_size;


	read_size = recv(sock , client_message , 10 , 0);
    
    //client_message[strlen(client_message)-1] = '\0';
    

    if(strcmp(client_message,"put")==0){
        server_put(sock,directory);
    }

    if(strcmp(client_message,"get")==0){
        server_get(sock,directory);
    }

    if(strcmp(client_message,"list")==0){
        server_list(sock,directory);
    }
	
	if(read_size == 0)
	{
		puts("Client disconnected\n");
		fflush(stdout);
	}
	else if(read_size == -1)
	{
		perror("recv failed\n");
	}
		
	//Free the socket pointer
	free(sock_desc);
    free(input_args);    
	
	return 0;
}



the cat ran toi thew sztoree