#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>

void error(char *msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}

static double diff_in_second(struct timespec t1, struct timespec t2)
{
    struct timespec diff;
    if (t2.tv_nsec-t1.tv_nsec < 0) {
        diff.tv_sec  = t2.tv_sec - t1.tv_sec - 1;
        diff.tv_nsec = t2.tv_nsec - t1.tv_nsec + 1000000000;
    } else {
        diff.tv_sec  = t2.tv_sec - t1.tv_sec;
        diff.tv_nsec = t2.tv_nsec - t1.tv_nsec;
    }
    return (diff.tv_sec + diff.tv_nsec / 1000000000.0);
}

int main()
{
	int sockfd, numbytes;
	struct sockaddr_in my_addr;
	struct sockaddr_in their_addr;
	struct stat filestat;
	off_t success_bytes = 0, success_rate;

	double cpu_time;
	struct timespec start, end;

	uint32_t payload_size = 100;
	uint8_t* data_out = (uint8_t*) malloc(payload_size);

	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1 )
		error("socket");

	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(5134);
	my_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(sockfd, (struct sockaddr*)&my_addr, sizeof(struct sockaddr)) == -1 )
		error("bind");

	printf("Server is ready to receive\n");

	FILE *fp;
	if ((fp = fopen("getfile", "w")) == NULL)
		error("fopen");

	/* read file size from client to know how much data remained */
	if (recv(sockfd, &filestat.st_size, sizeof(filestat.st_size), 0) < 0)
		error("Error:read file_size failed");

	printf("The file size is %lu bytes\n\n", filestat.st_size);
	clock_gettime(CLOCK_REALTIME, &start);

	while (1) {

		numbytes = recv(sockfd, data_out, payload_size, 0);
		printf("read %d bytes\n", numbytes);

		numbytes = fwrite(data_out, sizeof(char), numbytes, fp);
		printf("fwrite %d bytes\n", numbytes);

		success_bytes += numbytes;
		success_rate = success_bytes * 100 / filestat.st_size;
		printf("success_bytes = %lu \n", success_bytes);
		printf("success_rate = %lu% \n", success_rate);

		clock_gettime(CLOCK_REALTIME, &end);
		cpu_time = diff_in_second(start, end);
		printf("execution time of transmission : %lf sec\n\n", cpu_time);

		if (filestat.st_size == success_bytes)
			break;

	} /* end of while (1) */

	printf("server finished\n");

	fclose(fp);
	free(data_out);
	close(sockfd);
	return 0;
}
