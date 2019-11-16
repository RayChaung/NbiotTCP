#include <stdio.h> 
#include <string.h> 
#include <stdlib.h> 
#include <errno.h> 
#include <unistd.h> 
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/types.h> 
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <sys/time.h> 
#include "slip.h" /* SLIP encode and decode*/
#define TRUE 1 
#define FALSE 0 
#define PORT 8888 
#define BUFSIZE 1400	
int main(int argc , char *argv[]) 
{
	unsigned char buffer[BUFSIZE], encode_buffer[BUFSIZE*2], prev_encode_buffer[BUFSIZE*4];
	char buff[BUFSIZE];
	struct sockaddr_in nbiot, host, client, nbiot_DST, host_DST;
	int fd_nbiot, fd_host, maxfd, optval = 1;
	int rv_len = 0;
	char publicip[100][16]={0};
	uint8_t  privateip[100][4] = {0};
	uint16_t port[100]; 
	int map_len = 0;
	int nread = 0, nwrite = 0;
	unsigned long encode_len = 0, decode_len = 0;
	int prev_len = 0;
	int index, next_index;
	int foundEND;
	if ( (fd_nbiot = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("nbiot socket()");
		exit(1);
	}
	if ( (fd_host = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("host socket()");
		exit(1);
	}
	memset(&nbiot, 0, sizeof(nbiot));
    nbiot.sin_family = AF_INET;
    nbiot.sin_addr.s_addr = INADDR_ANY;
    nbiot.sin_port = htons(atoi(argv[1]));
	memset(&host, 0, sizeof(host));
    host.sin_family = AF_INET;
    host.sin_addr.s_addr = INADDR_ANY;
    host.sin_port = htons(atoi(argv[2]));
	if (bind(fd_nbiot, (struct sockaddr*) &nbiot, sizeof(nbiot)) < 0) {
      perror("bind()");
      exit(1);
    }
	if (bind(fd_host, (struct sockaddr*) &host, sizeof(host)) < 0) {
      perror("bind()");
      exit(1);
    }
	maxfd = (fd_nbiot > fd_host)?fd_nbiot:fd_host;
	while(1){
		int ret;
		fd_set rd_set;

		FD_ZERO(&rd_set);
		FD_SET(fd_nbiot, &rd_set); 
		FD_SET(fd_host, &rd_set);

		ret = select(maxfd + 1, &rd_set, NULL, NULL, NULL);

		if (ret < 0 && errno == EINTR){
			continue;
		}

		if (ret < 0) {
			perror("select()");
			exit(1);
		}
		if(FD_ISSET(fd_host, &rd_set)) {
			memset(buff, 0, BUFSIZE);
			memset(encode_buffer, 0, BUFSIZE*2);
			if((nread=recvfrom(fd_host, encode_buffer, BUFSIZE*2, 0, ( struct sockaddr *)&client, &rv_len)) < 0){
				perror("Recvfrom data");
				exit(1);
			}
			if(encode_buffer[0] == '+' && encode_buffer[1] == '+' && encode_buffer[2] == '+'){
				//+++special cmd : map private, public ip and store to array
				if(strlen(encode_buffer) > 18){
					printf("wrong hello msg\n");
					goto CHECK_ANO_FD;
				}
				if(ntohs(client.sin_port) == 0) goto CHECK_ANO_FD;
				//check is there already a private ip
				uint8_t priv_check[4] = {0};
				char *pch;
				pch = strtok(encode_buffer+3, ".");
				int i = 0, repeat = 0;
				while(pch != NULL && i < 4){
					priv_check[i] = atoi(pch);
					i++;
					pch = strtok(NULL, ".");
				}
				for(i = 0; i < map_len; i++){
					if(privateip[i][0] == priv_check[0] && privateip[i][1] == priv_check[1] && privateip[i][2] == priv_check[2] && privateip[i][3] == priv_check[3])
						repeat = 1;
				}
				if(repeat == 1){
					printf("private ip already in used\n");
					goto CHECK_ANO_FD;
				}
				//store privateip and publicip
				inet_ntop(AF_INET, &client.sin_addr, buff, BUFSIZE);
				strcpy(publicip[map_len], buff);
				privateip[map_len][0] = priv_check[0];
				privateip[map_len][1] = priv_check[1];
				privateip[map_len][2] = priv_check[2];
				privateip[map_len][3] = priv_check[3];
				port[map_len] = ntohs(client.sin_port);
				printf("host public ip: %s\tprivate ip : %d %d %d %d\tport#%d\n", publicip[map_len], privateip[map_len][0], privateip[map_len][1], privateip[map_len][2], privateip[map_len][3], port[map_len]);
				map_len ++;
			}
			else{
				printf("host packet length %d\n",  nread);
				for(int i = 0;i < nread;i ++)printf("%02x",encode_buffer[i]); printf("\n");printf("\n");	
				slip_decode(encode_buffer, nread, buffer, BUFSIZE, &decode_len);
				//check host packet length
				if ( (buffer[2] * 256 + buffer[3])  != decode_len){
					printf("from host wrong ip packet length\n");
					continue;
				}
				//parse tun private ip address   ex : C0,A8,00,01 => 192.168.0.1 
				for(int i = 0; i < map_len; i++){
					if(privateip[i][0] == buffer[16] && privateip[i][1] == buffer[17] && privateip[i][2] == buffer[18] && privateip[i][3] == buffer[19]){
						//printf("match %s\t port#%d\n",publicip[i], port[i]);
						memset(&host_DST, 0, sizeof(host_DST));
						host_DST.sin_family = AF_INET;
						host_DST.sin_addr.s_addr = inet_addr(publicip[i]);
						host_DST.sin_port = htons(port[i]);
						if((nwrite=sendto(fd_nbiot, encode_buffer, nread, 0, (const struct sockaddr *) &host_DST, sizeof(host_DST))) < 0){
							perror("error: send nbiot to host\n");
							exit(1);
						}
						break;
					}	
				}
				
			}
		}
CHECK_ANO_FD:		
		if(FD_ISSET(fd_nbiot, &rd_set)) {
			memset(buff, 0, BUFSIZE);
			memset(encode_buffer, 0, BUFSIZE*2);
			if((nread=recvfrom(fd_nbiot, encode_buffer, BUFSIZE*2, 0, ( struct sockaddr *)&client, &rv_len)) < 0){
				perror("Recvfrom data");
				exit(1);
			}
			if(encode_buffer[0] == '+' && encode_buffer[1] == '+' && encode_buffer[2] == '+'){
				//+++special cmd : map private, public ip and store to array
				if(strlen(encode_buffer) > 18){
					printf("wrong hello msg\n");
					continue;
				}
				if(ntohs(client.sin_port) == 0) continue;
				//check is there already a private ip
				uint8_t priv_check[4] = {0};
				char *pch;
				pch = strtok(encode_buffer+3, ".");
				int i = 0, repeat = 0;
				while(pch != NULL && i < 4){
					priv_check[i] = atoi(pch);
					i++;
					pch = strtok(NULL, ".");
				}
				for(i = 0; i < map_len; i++){
					if(privateip[i][0] == priv_check[0] && privateip[i][1] == priv_check[1] && privateip[i][2] == priv_check[2] && privateip[i][3] == priv_check[3])
						repeat = 1;
				}
				if(repeat == 1){
					printf("private ip already in used\n");
					continue;
				}
				//store privateip and publicip
				inet_ntop(AF_INET, &client.sin_addr, buff, BUFSIZE);
				strcpy(publicip[map_len], buff);
				privateip[map_len][0] = priv_check[0];
				privateip[map_len][1] = priv_check[1];
				privateip[map_len][2] = priv_check[2];
				privateip[map_len][3] = priv_check[3];
				port[map_len] = ntohs(client.sin_port);
				printf("nbiot public ip: %s\tprivate ip : %d %d %d %d\tport#%d\n", publicip[map_len], privateip[map_len][0], privateip[map_len][1], privateip[map_len][2], privateip[map_len][3], port[map_len]);
				map_len ++;
			}
			else{
				printf("nbiot packet length %d\n",  nread);
                for(int i = 0;i < nread;i ++)printf("%02x",encode_buffer[i]); printf("\n");printf("\n");fflush(stdout);
				
				memcpy(&prev_encode_buffer[prev_len], encode_buffer, nread);
				foundEND = 0;
				for(index = 0; index < nread+prev_len; index++){
					if(prev_encode_buffer[index] == SLIP_END){
						foundEND = 1;
						for(next_index = index+1; next_index < nread+prev_len; next_index++){
							if(prev_encode_buffer[next_index] == SLIP_END){
								printf("send a packet %d bytes to the host\n", next_index-index+1);
								for(int i = index; i<= next_index; i++)	printf("%02x",prev_encode_buffer[i]);printf("\n");
								memset(buffer, 0, BUFSIZE);
								slip_decode(&prev_encode_buffer[index], next_index-index+1, buffer, BUFSIZE, &decode_len);
								memset(&host_DST, 0, sizeof(host_DST));
								host_DST.sin_family = AF_INET;
								host_DST.sin_addr.s_addr = inet_addr(publicip[0]);
								host_DST.sin_port = htons(port[0]);
								if((nwrite=sendto(fd_host, buffer, decode_len, 0, (const struct sockaddr *) &host_DST, sizeof(host_DST))) < 0){
									perror("error: send nbiot to host\n");
									exit(1);
								}
								break;
							}
						}
						if(next_index == nread+prev_len){
							//cannot find END
							//store the rest to the start of prev_encode_buffer
							memmove(prev_encode_buffer, prev_encode_buffer+index, nread+prev_len-index);
							prev_len = nread+prev_len-index;
							break;
						}
						else if(next_index == nread+prev_len-1){
							//find END and finish reading prev_encode_buffer
							memset(prev_encode_buffer, 0, BUFSIZE*4);
							prev_len = 0;
							break;
						}
						else index = next_index;
					}
					
				}
				if(foundEND == 0){
					//no any END in prev_encode_buffer
					memset(prev_encode_buffer, 0, BUFSIZE*4);
					prev_len = 0;
				}	
			}
			
		}
		
	}
	
	return 0; 
} 
