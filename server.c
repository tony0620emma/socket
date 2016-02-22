#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

void error(char *msg)
{
	perror(msg);
	exit(1);
}

int main(int argc, char *argv[])
{
	int sockfd, newsockfd, portno, clilen, n;
	char buffer[256];
	struct sockaddr_in server_addr, client_addr;

	if (argc < 2) {
		fprintf(stderr, "ERROR, no port provided\n");
		exit(1);
	}

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
		error("ERROR: can't open socket");

	memset(&server_addr, 0, sizeof(server_addr));
	portno = atoi(argv[1]);

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(portno);
	server_addr.sin_addr.s_addr = INADDR_ANY;

	if (bind(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0)
		error("ERROR on binding");

	listen(sockfd, 5);
	printf("Waiting for client to connect...\n");

	clilen = sizeof(client_addr);
	newsockfd = accept(sockfd, (struct sockaddr *) &client_addr, &clilen);
	if (newsockfd < 0)
		error("ERROR on accept");

	memset(buffer, 0, 256);
	n = read(newsockfd, buffer, 255);
	if (n < 0)
		error("ERROR reading from socket");
	printf("Receive message: %s\n", buffer);

	n = write(newsockfd, "I got your message", 18);
	if (n < 0)
		error("ERROR writing to socket");

	return 0;
}
