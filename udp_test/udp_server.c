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
	int sockfd, packet_num, count = 1, check_num = 1, loop_time = 0, lost_end;
	struct sockaddr_in my_addr;

	double cpu_time, average_time, total_time, lost_rate, total_packet;
	struct timespec start, end;

	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1 )
		error("socket");

	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(5134);
	my_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(sockfd, (struct sockaddr*)&my_addr, sizeof(struct sockaddr)) == -1 )
		error("bind");

	printf("Server is ready to receive\n");

	while (1) {

		clock_gettime(CLOCK_REALTIME, &start);

		recv(sockfd, &packet_num, sizeof(int), 0);

		clock_gettime(CLOCK_REALTIME, &end);
		cpu_time = diff_in_second(start, end);
		
		total_time += cpu_time;
		average_time = total_time / count;

		if (packet_num != check_num) {

			if (packet_num < check_num) {
				loop_time ++;
				printf("\nwarning %d W packets\n\n", loop_time);}

			total_packet = (loop_time * 10000 + packet_num);
			lost_rate = (total_packet - count) * 100 / total_packet;

			lost_end = packet_num - 1;

			if (lost_end == 0)
				lost_end = 10000;

			printf(" ===============================================================================\n");
			printf("|packet lost\t|lost rate\t|packet transmission\t|average transmission\t|\n");
			printf("|%d ~ %d\t|%lf %\t|%lf sec\t\t|%lf sec\t\t|\n", check_num, lost_end, lost_rate, cpu_time, average_time);
			printf(" ===============================================================================\n");
			check_num = packet_num;
		}

		check_num ++;
		count ++;

		if (check_num == 10001) {

			check_num = 1;
			loop_time ++;
			printf("\nwarning %d W packets\n\n", loop_time);
		}
	}
	
	printf("server finished\n");

	close(sockfd);
	return 0;
}
