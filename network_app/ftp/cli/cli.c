
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define MAXDATASIZE 100 // max number of bytes we can get at once 
#define flush fflush(stdout);

void writetofile(char *buf, char *filename)
{
    // remove $end$ from buf
    char *endptr;
    endptr = strstr(buf, "$end$");
    if(endptr != NULL)
        *endptr='\0';

    // write buf to file
    FILE *fp;

    fp = fopen(filename, "w");

    if(fp == NULL)
        printf("Could not open file for writing.\n");
    else
    {
        // write to file
        fprintf(fp, "%s", buf);
    }
    // close file
    fclose(fp);
}

int main(int argc, char *argv[])
{
    char *server_ip;
    int port = 4567; // default port client will be connecting to
    int sockfd, numbytes;  
    char buf[MAXDATASIZE];
    struct sockaddr_in their_addr; // connector's address information 
    char command[MAXDATASIZE];

    //************************************ESTABLISH CONN
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

    // *****************************FTP AT WORK

    while(1) {

        // reset command
        bzero(command, MAXDATASIZE);
        flush

        printf("ftp>> ");  // print prompt
        fgets(command, 10, stdin);  // get command
        // remove newline
        command[strcspn(command, "\n")] = 0;

        // send command
        if (send(sockfd, command, strlen(command), 0) == -1)
            perror("cli-send: ");

        if(strcmp(command, "ls") == 0 ) {
            // wait for reply from server
            if ((numbytes=recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1) {
                perror("cli-recv: ");
                exit(1);
            }
            buf[numbytes] = '\0';
            printf("Files:\n%s",buf);
        }
        else if(strcmp(command, "quit") == 0 || strcmp(command, "exit") == 0  ) {
            printf("Closing connection...\n");
            break;
        }
        else if(strcmp(command, "help") == 0) {
            printf("Usage:\n");
            printf("ls: list files on server\n");
            printf("get filename: receive file from server\n");
            printf("put filename: send file to server\n");
            printf("quit: close connection to server and exit\n");
            printf("help: show this help message.\n");
        }
        else if(strncmp(command, "get", 3) == 0 ) {
            char filename[MAXDATASIZE];
            // get filename
            strcpy(filename,strchr(command, ' ')+1);
            printf("%s", filename);

            // wait for file until server closes connection
            char finalbuf[MAXDATASIZE] = "";
            bzero(buf, MAXDATASIZE);
            numbytes = 1;
            while(numbytes != 0) {
                if ((numbytes=recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1) {
                    perror("cli-recv: ");
                    exit(1);
                }
                buf[numbytes] = '\0';
                if(strncmp(buf, "$end$", 5) == 0)
                    break; // file recieve complete
                else {
                    printf("%s",buf);
                    // add to final buf
                    strcat(finalbuf, buf);
                }
                flush
            }
            finalbuf[strlen(finalbuf)] = '\0';
            writetofile(finalbuf, filename);
            printf("File recieved.\n");
        }
        else if(strncmp(command, "put", 3) == 0 ) {
            char filename[MAXDATASIZE];
            // get filename
            strcpy(filename,strchr(command, ' ')+1);
            /*printf("%s", filename);*/
            flush

            printf("Sending file %s to server\n", filename);

            FILE* fp = NULL;
            char data[MAXDATASIZE];
            fp = fopen(filename, "r");
            
            if (fp == NULL) {	
                // send file not foud // error file open
                printf("Could not open file for sending.\n");
            } else {	
                printf("Beginning file sending to server\n"); 
                // send file line by line
                while(fgets(data, MAXDATASIZE, fp) != NULL) {
                    /*fflush(fp);*/
                    /*printf("sending: %s\n", data);*/
                    if (send(sockfd, data, strlen(data), 0) < 0)
                        perror("error sending file\n");
                }
                fclose(fp);
                printf("File sending complete.\n");
                flush
            }
            // send end of file to client
            strcpy(data, "$end$");
            /*printf("sending: %s\n", data);*/
            if (send(sockfd, data, strlen(data), 0) < 0)
                perror("error sending file\n");
        }


    }

    close(sockfd);

    return 0;
} 
