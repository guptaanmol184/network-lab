#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>

#define BUFLEN 1024
#define endl printf("\n");

void except(char *error) {
	perror(error);
	exit(1);
}

#define RRQ 01			/* read request */
#define WRQ 02			/* write request */
#define DATA 03			/* data packet */
#define ACK 04			/* acknowledgement */
#define ERROR 05		/* error code */
#define EUNDEF 0		/* not defined */
#define ENOTFOUND 1 /* file not found */
#define EACCESS 2		/* access violation */
#define ENOSPACE 3	/* disk full or allocation exceeded */
#define EBADOP 4		/* illegal TFTP operation */
#define EBADID 5		/* unknown transfer ID */
#define EEXISTS 6		/* file already exists */
#define ENOUSER 7		/* no such user */

typedef struct packet {
	short opcode;
	union {
		unsigned short block;
		short code;
	};
	char *data;
	char *stuff;
} TFTPPACK;

int error = 0;

void rawPrint(char *buf, size_t length) {
	endl for (int i = 0; i < length; i++) { printf("%02X ", buf[i]); }
}

TFTPPACK rawToTFTP(char *buf, size_t packetLength) {
	TFTPPACK *packetptr = calloc(1, sizeof(packetptr));
	memcpy(&packetptr->opcode, buf, sizeof(short));
	packetptr->opcode = ntohs(packetptr->opcode);

	switch (packetptr->opcode) {
	case RRQ:
	case WRQ:
		packetptr->stuff = strdup(buf + 2);
		packetptr->data = strdup(buf + strlen(packetptr->stuff) + 3);
		break;
	case DATA:
		memcpy(&packetptr->block, buf + 2, sizeof(short));
		packetptr->block = ntohs(packetptr->block);
		packetptr->data = strndup(buf + 4, packetLength);
		break;
	case ACK:
		memcpy(&packetptr->block, buf + 2, sizeof(short));
		packetptr->block = ntohs(packetptr->block);
		break;
	case ERROR:
		memcpy(&packetptr->block, buf + 2, sizeof(short));
		packetptr->block = ntohs(packetptr->block);
		packetptr->data = strndup(buf + 3, packetLength);
		break;
	default:
		packetptr->data = "\0";
		packetptr->stuff = "\0";
		;
	}
	return *packetptr;
}

char *TFTPToRaw(TFTPPACK packet, size_t *len) {
	*len = 0;
	*len += sizeof(short);
	char *raw = 0, curPos = 0;
	short opcodeN = htons(packet.opcode);
	short errorcodeN = htons(packet.code);
	switch (packet.opcode) {
	case RRQ:
	case WRQ:
		*len += strlen(packet.stuff) + 1;
		raw = calloc(*len, 1);
		curPos = mempcpy(mempcpy(raw, &opcodeN, sizeof(short)), packet.stuff, strlen(packet.stuff) + 1);
		/* curPos = mempcpy(curPos, packet.stuff, strlen(packet.stuff)+1); */
		break;
	case ACK:
		*len += sizeof(short);
		raw = calloc(*len, 1);
		mempcpy(mempcpy(raw, &opcodeN, sizeof(short)), &errorcodeN, sizeof(short));
		break;
	case DATA:
		*len += sizeof(short);
		*len += strlen(packet.data);
		raw = calloc(*len, 1);
		mempcpy(mempcpy(mempcpy(raw, &opcodeN, sizeof(short)), &errorcodeN,
										sizeof(short)),
						packet.data, strlen(packet.data));
		break;
	case ERROR:
		*len += sizeof(short);
		*len += strlen(packet.data) + 1;
		raw = calloc(*len, 1);
		mempcpy(mempcpy(mempcpy(raw, &opcodeN, sizeof(short)), &errorcodeN,
										sizeof(short)),
						packet.data, strlen(packet.data) + 1);
		break;
	default:
		*len = 0;
	}
	return raw;
}

TFTPPACK makeDataPacket(char *buf, int numBytes, int blockNo) {
	TFTPPACK *packet = calloc(1, sizeof(TFTPPACK));
	packet->opcode = DATA;
	packet->data = calloc(1, numBytes);
	memcpy(packet->data, buf, numBytes);
	packet->block = blockNo;
	return *packet;
}

TFTPPACK makeAckPacket(int blockNo) {
	TFTPPACK *packet = calloc(1, sizeof(TFTPPACK));
	packet->opcode = ACK;
	packet->block = blockNo;
	return *packet;
}

TFTPPACK makeErrorPacket(int code) {
	TFTPPACK *packet = calloc(1, sizeof(TFTPPACK));
	packet->opcode = ERROR;
	packet->code = code;
	packet->data = calloc(1, 20);
	switch (packet->code) {
	case ENOTFOUND:
		strcpy(packet->data, "File not found.");
		break;
	case EACCESS:
		strcpy(packet->data, "Access violation");
		break;
	case EBADOP:
		strcpy(packet->data, "Illegal operation.");
		break;
	default:
		strcpy(packet->data, "Undefined error");
	}
	return *packet;
}

void sendPacket(TFTPPACK packet, int sockfd, struct sockaddr_in sockClient) {
	size_t packetSize;
	char *buff = TFTPToRaw(packet, &packetSize);
	sendto(sockfd, buff, packetSize, 0, (struct sockaddr *)&sockClient, (socklen_t)sizeof(sockClient));
}

size_t getPacket(TFTPPACK *packet, int sockfd, struct sockaddr_in *sockClient) {
	size_t packetSize;
	char *buf = calloc(1, BUFLEN);

	packetSize = recvfrom(sockfd, buf, BUFLEN, 0, (struct sockaddr *)&sockClient,
												(socklen_t *)sizeof(sockClient));
	if (packetSize > 0)
		*packet = rawToTFTP(buf, packetSize);
	return packetSize;
}

int getAckPacket(int sockfd, struct sockaddr_in sockClient, int blockNo) {
	TFTPPACK packet;
	size_t packetLength = getPacket(&packet, sockfd, &sockClient);
	if (packetLength > 0)
		if (packet.opcode == ACK)
			if (packet.block == blockNo)
				return 1;
	return 0;
}

void TFTPRead(TFTPPACK readrequest, struct sockaddr_in Client) {
	char *filename = strdup(readrequest.stuff);

	struct sockaddr_in tempSock, sockClient;
	int sockfd = 0;
	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		except("ERROR OPENING SOCKET");
	bzero(&tempSock, sizeof(tempSock));
	bzero(&sockClient, sizeof(sockClient));
	sockClient.sin_port = Client.sin_port;
	sockClient.sin_family = Client.sin_family;
	sockClient.sin_addr = Client.sin_addr;

	tempSock.sin_family = AF_INET;
	tempSock.sin_addr.s_addr = htonl(INADDR_ANY);
	tempSock.sin_port = htons(0); // Port 0 to get a random port
	int optval;
	optval = 1;

	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int));
	if ((bind(sockfd, (struct sockaddr *)&tempSock, sizeof(tempSock)) < 0))
		except("FAILED TO BIND, IS THE ADDRESS ALREADY IN USE?");
	struct timeval read_timeout;
	read_timeout.tv_sec = 2;
	read_timeout.tv_usec = 0;
	setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &read_timeout, sizeof read_timeout);

	if (access(filename, F_OK) == -1) {
		TFTPPACK epacket = makeErrorPacket(ENOTFOUND);
		perror(NULL);
		error = 1;
		sendPacket(epacket, sockfd, sockClient);
		return;
	}
	/* return; */

	/* Get random port to start transmission */

	int fd = open(filename, O_RDONLY);
	char *buf = calloc(1, 512);
	/* int packetLength = recvfrom(sockfd, buf, BUFLEN, 0, (struct sockaddr
	* *)&sockClient, */
	/*(socklen_t *)sizeof(sockClient)); */
	int numBytes, block = 1, retries = 3;
	size_t packetSize;
	while ((numBytes = read(fd, buf, 512)) > 0) {
		TFTPPACK packet = makeDataPacket(buf, numBytes, block);
		int tries;
		for (tries = 0; tries < retries; tries++) {
			sendPacket(packet, sockfd, sockClient);
			if (getAckPacket(sockfd, sockClient, block))
				break;
		}
		if (tries == retries)
			break;
		block++;
	}
}

int main() {
	char path[64];
	strcpy(path, getenv("HOME"));
	strcat(path, "/tftproot");
	chdir(path);
	struct sockaddr_in sockServer, sockClient;
	size_t slen = sizeof(sockClient);
	int sockfd;
	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		except("ERROR OPENING SOCKET");
	bzero(&sockServer, sizeof(sockServer));
	sockServer.sin_family = AF_INET;
	sockServer.sin_addr.s_addr = htonl(INADDR_ANY);
	sockServer.sin_port = htons(69);
	int optval; /* flag value for setsockopt */
	optval = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int));
	if ((bind(sockfd, (struct sockaddr *)&sockServer, sizeof(sockServer)) < 0))
		except("FAILED TO BIND, IS THE ADDRESS ALREADY IN USE?");

	char *buf = calloc(1, BUFLEN);
	size_t packetLength;
	TFTPPACK packet;
	/* printf("%lu", sizeof(packet)); */
	/*		fflush(stdout); */
	while (error == 0) {
		bzero(buf, BUFLEN);
		packetLength = recvfrom(sockfd, buf, BUFLEN, 0, (struct sockaddr *)&sockClient, (socklen_t *)&slen);
		if (packetLength > 0) {
			system("clear");
			printf("Received packet length %lu from %s:%d\nData:\n", packetLength, inet_ntoa(sockClient.sin_addr), ntohs(sockClient.sin_port));
			for (int i = 0; i < packetLength; i++)
				printf("%02X ", buf[i]);
			endl
			/* for (int i = 0; i < packetLength; i++) */
			/*		printf("%c ", buf[i]); */
			packet = rawToTFTP(buf, packetLength);
			size_t length;
			pid_t pid;
			switch (packet.opcode) {
				case RRQ:
					endl
					printf("READ REQUEST.");
					endl
					pid = fork();
					if (pid > 0) {
						continue;
					}
					else {
						printf("Forked.");
						endl
						fflush(stdout);
						TFTPRead(packet, sockClient);
					}
					break;
				case WRQ:
					printf("WRITE REQUEST"); break;
				case DATA:
					printf("DATA"); break;
				case ACK:
					printf("ACK"); break;
				case ERROR:
					printf("ERROR"); break;
				default:
					printf("NOT A TFTP PACKET");
					;
			}
			printf("\n---\n");
			fflush(stdout);
		}
	}
	wait(NULL);
	return 0;
}
