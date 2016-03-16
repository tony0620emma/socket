#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <kodoc/kodoc.h>

void error(char *msg)
{
	perror(msg);
	exit(1);
}

int main(int argc, char *argv[])
{
	int sockfd, newsockfd, portno, clilen, err;
	struct sockaddr_in server_addr, client_addr;
	uint8_t ack = 0;
	char buffer[256];
	int file_size;
	int remain_data;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
		error("ERROR: can't open socket");

	memset(&server_addr, 0, sizeof(server_addr));
	if (argc < 2) {
		printf("Usage: %s port\n", argv[0]);
		error("Invalid argument");
	}
	portno = atoi(argv[1]);

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(portno);
	server_addr.sin_addr.s_addr = INADDR_ANY;

	if (bind(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0)
		error("ERROR: binding failed");

	listen(sockfd, 5);
	printf("Waiting for client to connect...\n");

	clilen = sizeof(client_addr);
	newsockfd = accept(sockfd, (struct sockaddr *) &client_addr, &clilen);
	if (newsockfd < 0)
		error("ERROR: accept failed");

	uint32_t max_symbols = 10;
	uint32_t max_symbol_size = 100;

	int32_t code_type = kodo_full_vector;
	int32_t finite_field = kodo_binary;

	kodo_factory_t decoder_factory = 
		kodo_new_decoder_factory(code_type, finite_field,
				max_symbols, max_symbol_size,
				kodo_trace_enabled);
	
	/* Receive payload size from client */
	uint32_t payload_size;
	err = read(newsockfd, &payload_size, 4);
	if (err < 0)
		error("ERROR: reading payload size");

	/* Receive block size from client */
	uint32_t block_size;
	err = read(newsockfd, &block_size, 4);
	if (err < 0)
		error("ERROR: reading block size");

	/* Receive file size from client */
	memset(buffer, 0, 256);
	err = read(newsockfd, buffer, 255);
	if (err < 0)
		error("ERROR: reading file size");
	file_size = atoi(buffer);
	remain_data = file_size;
	fprintf(stdout, "%d bytes to receive from client\n\n", file_size);

	printf("payload size = %u\n", payload_size);
	printf("block size = %u\n", block_size);

	uint8_t *payload = (uint8_t *) malloc(payload_size);
	uint8_t *data_out = (uint8_t *) malloc(block_size);

	FILE *fp = fopen("Received", "w");

	while (remain_data > 0) {
		fprintf(stdout, "\nRemain ---> %d bytes of data\n", remain_data);
		kodo_coder_t decoder = kodo_factory_new_decoder(decoder_factory);
		if (remain_data < block_size)
			block_size = remain_data;
		ack = 0;
		while (ack == 0) {
			err = read(newsockfd, payload, payload_size);
			if (err < 0)
				error("ERROR: reading payload");
			kodo_read_payload(decoder, payload);
			if (kodo_is_complete(decoder)) {
				ack = 1;
				fprintf(stdout, "kodo is complete\n");
				err = write(newsockfd, &ack, 1);
				if (err < 0)
					error("ERROR: sending ack");
			}  else {
				fprintf(stdout, "kodo not complete\n");
				err = write(newsockfd, &ack, 1);
				if (err < 0)
					error("ERROR: sending ack");
			}
		}
		kodo_copy_from_symbols(decoder, data_out, block_size);
		fprintf(stdout, "Received %d bytes in a block\n", block_size);
		fwrite(data_out, sizeof(uint8_t), block_size, fp);
		remain_data -= block_size;
		kodo_delete_decoder(decoder);
	}

	printf("\n");
	printf("Receiving finished.\n");
	fclose(fp);
	free(payload);
	free(data_out);
	return 0;
}
