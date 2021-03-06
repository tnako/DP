/*
    C socket server example
*/
 
#include<stdio.h>
#include<string.h>    //strlen
#include<sys/socket.h>
#include<arpa/inet.h> //inet_addr
#include<unistd.h>    //write
 
int main(int argc , char *argv[])
{
    int socket_desc , client_sock , c , read_size;
    struct sockaddr_in server , client;
    char client_message[6];
    memset(client_message, 0x0, sizeof(char)*6);
     
    //Create socket
    socket_desc = socket(AF_INET , SOCK_STREAM , 0);
    if (socket_desc == -1)
    {
        printf("Could not create socket");
    }
    puts("Socket created");
     
    //Prepare the sockaddr_in structure
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons( 12345 );
     
    //Bind
    if( bind(socket_desc,(struct sockaddr *)&server , sizeof(server)) < 0)
    {
        //print the error message
        perror("bind failed. Error");
        return 1;
    }
    puts("bind done");
     
    //Listen
    listen(socket_desc , 5000);
     
    //Accept and incoming connection
    puts("Waiting for incoming connections...");
    c = sizeof(struct sockaddr_in);
     
    //accept connection from an incoming client
    while(1) {
	client_sock = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c);
	if (client_sock < 0)
	{
	    perror("accept failed");
	    return 1;
	}
	
	//Receive a message from client
	while( (read_size = recv(client_sock , client_message , 5 , 0)) > 0 )
	{
	    //printf("<- %s\n", client_message);
	    //Send the message back to client
	    write(client_sock , client_message , read_size);
	    memset(client_message, 0x0, sizeof(char)*6);
	}
	
	if(read_size == 0)
	{
	    fflush(stdout);
	}
	else if(read_size == -1)
	{
	    perror("recv failed");
	}
	close(client_sock);
    }
    return 0;
}
