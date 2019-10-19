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
#define TRUE 1 
#define FALSE 0 
#define PORT 8888 
#define BUFSIZE 800	
int main(int argc , char *argv[]) 
{
	unsigned char buffer[BUFSIZE];
	char buff[BUFSIZE];
	struct sockaddr_in nbiot, host, client;
	int fd_nbiot, fd_host, maxfd, optval = 1;
	int rv_len = 0;
	char pub_priv_map[100][2][16]={0};
	unsigned short int port[100]; 
	int map_len = 0;
	int nread = 0;
	//debug
	FILE* debugfd;
	debugfd = fopen("/home/gemproject/tun0411081/switch.txt", "w");
	//
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
    nbiot.sin_port = htons(4567);
	memset(&host, 0, sizeof(host));
    host.sin_family = AF_INET;
    host.sin_addr.s_addr = INADDR_ANY;
    host.sin_port = htons(5678);
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
			memset(buffer, 0, BUFSIZE);
			if((nread=recvfrom(fd_host, buffer, BUFSIZE, 0, ( struct sockaddr *)&client, &rv_len)) < 0){
				perror("Recvfrom data");
				exit(1);
			}
			if(buffer[0] == '+' && buffer[1] == '+' && buffer[2] == '+'){
				//+++special cmd : map private, public ip and store to array
				if(strlen(buffer) > 18){
					printf("wrong hello msg\n");
					continue;
				}
				printf("from static ip %s, port %d\n", inet_ntop(AF_INET, &client.sin_addr, buff, sizeof(buff)), ntohs(client.sin_port));
				if(ntohs(client.sin_port) == 0) continue;
				strcpy(pub_priv_map[map_len][0], buff);
				memcpy(pub_priv_map[map_len][1], &buffer[3],strlen(buffer)-3);
				pub_priv_map[map_len][1][strlen(buffer)-3] = '\0';
				port[map_len] = ntohs(client.sin_port);
				printf("public ip: %s\tprivate ip : %s\tport#%d\n", pub_priv_map[map_len][0], pub_priv_map[map_len][1],port[map_len]);
				
			}
			else{
				fwrite(buffer, 1, nread, debugfd);
				fflush(debugfd);
			}
		}
		if(FD_ISSET(fd_nbiot, &rd_set)) {
			printf("from host\n");
		
		
		}
		
	}
	
	return 0; 
} 
