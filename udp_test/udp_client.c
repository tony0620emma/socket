#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <sys/socket.h>

void error(char *msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}

int main(int argc, char* argv[])
{
	int sockfd, packet_num = 1;
	struct sockaddr_in my_addr;
	struct sockaddr_in serv_addr;

	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
		error("socket");

	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(2324);
	my_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

	if (bind(sockfd, (struct sockaddr *)&my_addr, sizeof(my_addr)) == -1)
		error("bind");

	serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(5134);

	while (1) {

		sendto(sockfd, &packet_num, sizeof(int), 0, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
		printf("Sending packet number : %d \n\n", packet_num);

		packet_num ++;

		if (packet_num == 10001) {
			packet_num = 1;
		}
	}

	printf("client finished\n");

	close(sockfd);
	return 0;
}
