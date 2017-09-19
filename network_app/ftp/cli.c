
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define MAXDATASIZE 100 // max number of bytes we can get at once 

int main(int argc, char *argv[])
{
    char *server_ip;
    int port = 4567; // default port client will be connecting to
    int sockfd, numbytes;  
    char buf[MAXDATASIZE];
    struct sockaddr_in their_addr; // connector's address information 

    if (argc < 2 || argc > 3) {
        fprintf(stderr,"usage: client hostip [port]\n");
        exit(1);
    }
    else {
        server_ip = argv[1];
        if(argc == 3)
            port = atoi(argv[2]);
    }

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("cli-socket: ");
        exit(1);
    }

    their_addr.sin_family = AF_INET;    // host byte order 
    their_addr.sin_port = htons(port);  // short, network byte order 
    their_addr.sin_addr.s_addr = inet_addr(server_ip);
    memset(&(their_addr.sin_zero), '\0', 8);  // zero the rest of the struct 

    if (connect(sockfd, (struct sockaddr *)&their_addr, sizeof(struct sockaddr)) == -1) {
        perror("cli-connect: ");
        exit(1);
    }

    printf("Client connected to %s\n", server_ip);

    // client recieve welcome msg
    if ((numbytes=recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1) {
        perror("cli-recv: ");
        exit(1);
    }
    buf[numbytes] = '\0';
    printf("Received: %s",buf);

    while(1) {
        char command[10];

        printf("ftp>> ");  // print prompt
        fgets(command, 10, stdin);  // get command
        // remove newline
        command[strcspn(command, "\n")] = 0;

        if (send(sockfd, command, sizeof(command), 0) == -1)
            perror("cli-send: ");
        memset(command, 0, sizeof(command));
    }

    close(sockfd);

    return 0;
} 
