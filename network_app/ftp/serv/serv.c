
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#define flush fflush(stdout);
#define MAXDATASIZE 100 // max number of bytes we can get at once 
#define BACKLOG 10     // how many pending connections queue will hold

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

void ftpput(int new_fd, char *filename)
{
    char bufr[MAXDATASIZE];
    // wait for file until server closes connection
    bzero(bufr, MAXDATASIZE);
    int numbytes = 1;
    while(numbytes != 0) {
        if ((numbytes=recv(new_fd, bufr, MAXDATASIZE-1, 0)) == -1) {
            perror("cli-recv: ");
            exit(1);
        }
        bufr[numbytes] = '\0';
        /*printf("bif:%s", bufr);*/
        flush
        if(strstr(bufr, "$end$") != NULL) {
            printf("%s",bufr);

            // write to file
            writetofile(bufr, filename);

            break; // file recieve complete
        }
        /*else*/
            /*printf("%s",bufr);*/
        /*flush*/
    }
    printf("\nFile recieved.\n");
}

void ftpget(int new_fd, char *filename)
{
    printf("Client asked for file %s\n", filename);
    FILE* fd = NULL;
	char data[MAXDATASIZE];
		
	fd = fopen(filename, "r");
	
	if (fd == NULL) {	
        // send file not foud // error file open
        printf("While client asked for was not found on the server.\n");
        strcpy(data, "File not found\n");
        if (send(new_fd, data, strlen(data), 0) < 0)
            perror("serv-error sending file not found\n");

	} else {	
        printf("Beginning file sending\n"); 
        // send file line by line
        while(fgets(data, MAXDATASIZE, fd) != NULL) {
            if (send(new_fd, data, strlen(data), 0) < 0)
                perror("error sending file\n");
        }
        fclose(fd);
        printf("File sending complete\n");
        flush
	}
    // send end of file to client
    strcpy(data, "$end$");
    if (send(new_fd, data, strlen(data), 0) < 0)
        perror("error sending file\n");
}

void ftplist(int new_fd)
{
    char data[MAXDATASIZE];
	size_t num_read;									
	FILE* fd;

    printf("listing files\n");
    flush
	int rs = system("ls > /tmp/ftptmpls.txt");
	if ( rs < 0) {
		exit(1);
	}
	
	fd = fopen("/tmp/ftptmpls.txt", "r");	
	if (!fd) {
		exit(1);
	}

	// Seek to the beginning of the file
	fseek(fd, SEEK_SET, 0);

	memset(data, 0, MAXDATASIZE);
	while ((num_read = fread(data, 1, MAXDATASIZE, fd)) > 0) {
		if (send(new_fd, data, num_read, 0) < 0) {
			perror("serv-send: ");
		}
		memset(data, 0, MAXDATASIZE);
	}

	fclose(fd);
    printf("listing complete\n");
}

int ftpcommand(int new_fd, char **argv)
{
    int result = 0;
    /*printf("argv[0]: %s\n", argv[0]);*/

    // select commands
    if(strcmp(argv[0], "help") == 0) {
        fputs("show help\n", stdout);
    }
    else if(strcmp(argv[0], "ls") == 0) {
        fputs("list files\n", stdout);
        ftplist(new_fd);
    }
    else if(strcmp(argv[0], "get") == 0) {
        fputs("send file\n", stdout);
        ftpget(new_fd, argv[1]);
    }
    else if(strcmp(argv[0], "put") == 0) {
        fputs("receive file\n", stdout);
        ftpput(new_fd, argv[1]);
    }
    else if(strcmp(argv[0], "exit") == 0 || strcmp(argv[0], "quit") == 0) {
        fputs("exit recieved\n", stdout);
        result = 1;
        return result;
    }
    else {
        fputs("invalid command\n", stdout);
    }
    flush

    return result;
}

void ftpserv(int new_fd, struct sockaddr_in their_addr)
{
    char buf[MAXDATASIZE];
    int result;
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
                result = ftpcommand(new_fd, res); 

            }
        // quit server
        if(result == 1)
            break;
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
            printf("Closing connection @ localhost:%d.\n", port);
            exit(0);
        }
        if(pid > 0)
            close(new_fd);  // parent doesn't need this
    }

    return 0;
} 

