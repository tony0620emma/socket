#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>

#include <kodoc/kodoc.h>

int ack = 0; /* 判斷是否換block的共享變數 */
pthread_t tcp_thread;
pthread_mutex_t mutex;
pthread_cond_t alternate;
struct stat filestat;

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

void trace_callback(const char* zone, const char* data, void* context)
{
	(void) context;

	if (strcmp(zone, "decoder_state") == 0) {
		printf("%s:\n", zone);
		printf("%s\n", data);
	}
}

void swap (int *a, int *b)
{
	int temp = *a;
	*a = *b;
	*b = temp;
}

void error(char *msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}

void *tcp_ack()
{
	pthread_mutex_lock(&mutex);

	int sockfd, new_fd, sin_size;
	struct sockaddr_in my_addr;
	struct sockaddr_in their_addr;

	/* 建立tcp連線(Server) */
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1 )
		error("socket");

	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(2325);
	my_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(sockfd, (struct sockaddr*)&my_addr, sizeof(struct sockaddr)) == -1 )
		error("bind");

	if (listen(sockfd, 10) == -1 )
		error("listen");

	if ((new_fd = accept(sockfd, (struct sockaddr*)&their_addr, &sin_size)) == -1 )
		error("accept");

	/* 讀取檔案大小 */
	if (read(new_fd, &filestat.st_size, sizeof(filestat.st_size)) < 0)
		error("Error:read file_size failed");

	printf("The file size is %lu bytes\n", filestat.st_size);

	pthread_cond_signal(&alternate);
	pthread_cond_wait(&alternate, &mutex);

	while (1) {

		if (write(new_fd,&ack,sizeof(ack)) < 0)
			error("\n Error:write ack failed\n");

		ack = 0;

		pthread_cond_signal(&alternate);

		if (filestat.st_size <= 0)
			break;

		pthread_cond_wait(&alternate, &mutex);
	}

	pthread_mutex_unlock(&mutex);
}

void thread_create(void)
{
	if ((pthread_create(&tcp_thread, NULL, tcp_ack, NULL)) != 0)
		printf("create function : tcp_thread create fail\n");

	else
		printf("create function : tcp_thread is established\n");
}

int main()
{
	struct timespec start, end;
    	double cpu_time;

	pthread_mutex_init(&mutex,NULL);

	pthread_mutex_lock(&mutex);

	/* 1.建立decoder */
	uint32_t max_symbols = 10;
	uint32_t max_symbol_size = 1000;

	int32_t codec = kodoc_full_vector;	
	int32_t finite_field = kodoc_binary;

	kodoc_factory_t decoder_factory =
        kodoc_new_decoder_factory(codec, finite_field,
                                 max_symbols, max_symbol_size);
	kodoc_coder_t decoder = kodoc_factory_build_coder(decoder_factory);

	uint32_t payload_size = kodoc_payload_size(decoder);
	uint32_t block_size = kodoc_block_size(decoder);	
	uint8_t* control = (uint8_t*) malloc(payload_size + 1);
	uint8_t* data_out = (uint8_t*) malloc(block_size);
	uint8_t* payload = control + 1;

	kodoc_delete_coder(decoder);

	/* 2.建立tcp_thread */
	thread_create();

	pthread_cond_wait(&alternate, &mutex);

	/* 3.建立udp連線(Server) */

	int sockfd, last_size = 0, numbytes, control_num = 0, swap_num = 1;
	struct sockaddr_in my_addr;

	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1 )
		error("socket");

	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(5134);
	my_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(sockfd, (struct sockaddr*)&my_addr, sizeof(struct sockaddr)) == -1 )
		error("bind");

	/* 開檔 */
	FILE *fp;
	if ((fp = fopen("456.mp4", "w")) == NULL)
		error("fopen");

	clock_gettime(CLOCK_REALTIME, &start);

	/* 接收payload並decode */
	while (filestat.st_size > 0) {

		if (filestat.st_size < block_size)
			last_size = filestat.st_size;

		filestat.st_size -= block_size;
		kodoc_coder_t decoder = kodoc_factory_build_coder(decoder_factory);		
		kodoc_set_mutable_symbols(decoder, data_out, block_size);
		kodoc_set_trace_stdout(decoder);
		kodoc_set_trace_callback(decoder, trace_callback, NULL);

		while (1) {

			if (recv(sockfd, control, payload_size + 1, 0) < 0)
				error("Error:read payload failed");

			if (*control == control_num) {

				kodoc_read_payload(decoder, payload);
				printf("Payload processed by decoder, current rank = %d\n\n",kodoc_rank(decoder));
				ack = kodoc_is_complete(decoder);
			}

			if (ack) {

				kodoc_delete_coder(decoder);

				swap (&control_num, &swap_num);

				if (last_size == 0)
					numbytes = fwrite(data_out, sizeof(char), block_size, fp);

				else
					numbytes = fwrite(data_out, sizeof(char), last_size, fp);

				printf("fwrite %d bytes\n\n", numbytes);
				memset(control, 0, payload_size + 1);
				

				pthread_cond_signal(&alternate);
				pthread_cond_wait(&alternate, &mutex);

				break;
			}
		}
	}

	pthread_mutex_unlock(&mutex);

	clock_gettime(CLOCK_REALTIME, &end);
	cpu_time = diff_in_second(start, end);
	printf("execution time of transmission : %lf sec\n\n", cpu_time);
	printf("server finished\n");

	free(data_out);
	free(control);
	kodoc_delete_factory(decoder_factory);
	fclose(fp);
	close(sockfd);
	return 0;
}
