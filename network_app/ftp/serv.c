
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#define flush fflush(stdout);
#define MAXDATASIZE 100 // max number of bytes we can get at once 
#define BACKLOG 10     // how many pending connections queue will hold

void ftplist(int new_fd)
{
}

void ftpcommand(int new_fd, char **argv)
{
    /*printf("argv[0]: %s\n", argv[0]);*/

    // select commands
    if(strcmp(argv[0], "help") == 0) {
        fputs("show help\n", stdout);
    }
    else if(strcmp(argv[0], "ls") == 0) {
        fputs("list files\n", stdout);
    }
    else if(strcmp(argv[0], "get") == 0) {
        fputs("send file\n", stdout);
    }
    else if(strcmp(argv[0], "put") == 0) {
        fputs("receive file\n", stdout);
    }
    else {
        fputs("invalid command\n", stdout);
    }
    flush
}

void ftpserv(int new_fd, struct sockaddr_in their_addr)
{
    char buf[MAXDATASIZE];
    int numbytes;
    while(1) {
        memset(buf, 0, sizeof(buf));
        if ((numbytes=recv(new_fd, buf, MAXDATASIZE-1, 0)) == -1) {
            perror("server-recv:");
            exit(1);
        }
        buf[MAXDATASIZE-1] = '\0';
        if(numbytes != 0) {

            printf("command %s received from client %s\n", buf, inet_ntoa(their_addr.sin_addr));

            // tokenise the command
            char **res = NULL;
            char *p= strtok(buf, " ");
            int n_spaces = 0;

            // split strings and append tokens to res
            while(p) {
                res = realloc( res, sizeof(char *) * ++n_spaces);
                if(res == NULL) {
                    printf("Memory allocation failed");
                    exit(-1); 
                }
                res[n_spaces-1] = p;
                p = strtok(NULL, " ");
            }

            // realloc one extra element for the last NULL
            res = realloc(res, sizeof(char*) * (n_spaces+1));
            res[n_spaces] = 0;
            // tokenise complete
            /*printf("argc: %d\n", n_spaces);*/
            flush

            // process
            if(n_spaces > 2) {
                printf("wrong number of arguments recieved.\n");
            }
            else
                ftpcommand(new_fd, res); 
                
            }
    }
}

int main(int argc, char *argv[])
{
    int port;
    int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
    struct sockaddr_in my_addr;    // my address information
    struct sockaddr_in their_addr; // connector's address information
    int sin_size;
    int yes=1;
    pid_t pid;


    if (argc != 2) {
        fprintf(stderr,"Usage: server port\n");
        exit(1);
    }
    else
        port = atoi(argv[1]);

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("serv-socket: ");
        exit(1);
    }

    if (setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int)) == -1) {
        perror("serv-setsockopt: ");
        exit(1);
    }
    
    my_addr.sin_family = AF_INET;         // host byte order
    my_addr.sin_port = htons(port);     // short, network byte order
    my_addr.sin_addr.s_addr = INADDR_ANY; // automatically fill with my IP
    memset(&(my_addr.sin_zero), '\0', 8); // zero the rest of the struct

    if (bind(sockfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr))    == -1) {
        perror("serv-bind: ");
        exit(1);
    }

    if (listen(sockfd, BACKLOG) == -1) {
        perror("serv-listen: ");
        exit(1);
    }

    printf("FTP server started up @ localhost:%d, waiting for clients... \n\n", port);

    while(1) {  // main accept() loop
        sin_size = sizeof(struct sockaddr_in);
        if ((new_fd = accept(sockfd, (struct sockaddr *)&their_addr, (socklen_t *)&sin_size)) == -1)  {
            perror("serv-accept: ");
            continue;
        }
        printf("Server: got connection from %s\n", inet_ntoa(their_addr.sin_addr));

        if((pid = fork()) < 0)
            perror("serv-fork: ");
        if (pid == 0) { // this is the child process
            close(sockfd); // child doesn't need the listener
            if (send(new_fd, "Welcome to your FTP server!\n", 28, 0) == -1)
                perror("serv-send: ");
            ftpserv(new_fd, their_addr);
            close(new_fd);
            exit(0);
        }
        if(pid > 0)
            close(new_fd);  // parent doesn't need this
    }

    return 0;
} 

