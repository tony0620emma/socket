#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <netdb.h>
#include <sys/stat.h>

#include <kodoc/kodoc.h>

void error(char *msg)
{
	perror(msg);
	exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
	int sockfd, portno, err;
	struct sockaddr_in server_addr;
	struct hostent *server;
	uint8_t ack;

	int fd;
	char file_size[256];
	struct stat file_stat;
	int offset;
	int remain_data;

	if (argc < 3) {
		printf("Usage: %s hostname hostport\n", argv[0]);
		error("ERROR: insufficient argument");
	}

	portno = atoi(argv[2]);
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
		error("ERROR: can't open socket");

	server = gethostbyname(argv[1]);
	if (server == NULL) {
		fprintf(stderr, "ERROR: no such host %s\n", argv[1]);
		exit(1);
	}

	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	memcpy(&server_addr.sin_addr.s_addr, server->h_addr, server->h_length);
	server_addr.sin_port = htons(portno);
	if (connect(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0)
		error("ERROR: connecting");

	uint32_t max_symbols = 10;
	uint32_t max_symbol_size = 100;
	int32_t code_type = kodo_full_vector;
	int32_t finite_field = kodo_binary;

	kodo_factory_t encoder_factory = 
		kodo_new_encoder_factory(code_type, finite_field,
				max_symbols, max_symbol_size,
				kodo_trace_disabled);
	kodo_coder_t encoder = kodo_factory_new_encoder(encoder_factory);
	
	uint32_t payload_size = kodo_payload_size(encoder);
	uint8_t *payload = (uint8_t *) malloc(payload_size);

	uint32_t block_size = kodo_block_size(encoder);
	uint8_t *data_in = (uint8_t *) malloc(block_size);

	if (kodo_is_systematic_on(encoder))
		printf("Systematic encoding enabled\n");
	else
		printf("Systematic encoding disabled\n");

	/* Tell server the payload size */
	err = write(sockfd, &payload_size, 4);
	if (err < 0)
		error("ERROR: sending payload size");

	/* Tell server the block size */
	err = write(sockfd, &block_size, 4);
	if (err < 0)
		error("ERROR: sending block size");

	FILE *fp = fopen("Transmit", "r");
	if (!fp)
		error("ERROR: opening file");
	fd = fileno(fp);

	/* Get file stats */
	if (fstat(fd, &file_stat) < 0)
		error("ERROR: fstat");

	fprintf(stdout, "File Size: %d bytes\n", file_stat.st_size);
	sprintf(file_size, "%d", file_stat.st_size);

	/* Tell server the file size */
	err = write(sockfd, file_size, sizeof(file_size));
	if (err < 0)
		error("ERROR: sending file size");

	fprintf(stdout, "%d bytes file size sent to server\n\n", err);
	remain_data = file_stat.st_size;

	while (remain_data > 0) {
		fprintf(stdout, "\nRemain ---> %d bytes of data to transmit\n", remain_data);
		if (remain_data < block_size)
			block_size = remain_data;
		fread(data_in, sizeof(uint8_t), block_size, fp);
		kodo_set_symbols(encoder, data_in, block_size);
		fprintf(stdout, "Encoding... block size = %u\n", block_size);
		do {
			kodo_write_payload(encoder, payload);
			err = write(sockfd, payload, payload_size);
			if (err < 0)
				error("ERROR: writing to socket");
			fprintf(stdout, "Sent %d bytes data to server\n", err);
			err = read(sockfd, &ack, 1);
			if (err < 0)
				error("ERROR: reading ack from socket");
		} while (ack == 0);
		remain_data -= block_size;
	}

	printf("Transmission Finished.\n");
	fclose(fp);
	free(payload);
	free(data_in);
	return 0;
}
