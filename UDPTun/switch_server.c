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
#define BUFSIZE 1600	
int main(int argc , char *argv[]) 
{
	unsigned char buffer[BUFSIZE];
	char buff[BUFSIZE];
	struct sockaddr_in nbiot, host, client, nbiot_DST, host_DST;
	int fd_nbiot, fd_host, maxfd, optval = 1;
	int rv_len = 0;
	char pub_priv_map[100][2][16]={0};
	unsigned short int port[100]; 
	int map_len = 0;
	int nread = 0, nwrite = 0;
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
			memset(buff, 0, BUFSIZE);
			if((nread=recvfrom(fd_host, buffer, BUFSIZE, 0, ( struct sockaddr *)&client, &rv_len)) < 0){
				perror("Recvfrom data");
				exit(1);
			}
			if(buffer[0] == '+' && buffer[1] == '+' && buffer[2] == '+'){
				//+++special cmd : map private, public ip and store to array
				if(strlen(buffer) > 18){
					printf("wrong hello msg\n");
					goto CHECK_ANO_FD;
				}
				if(ntohs(client.sin_port) == 0) goto CHECK_ANO_FD;
				int flag = 0;
				for(int i = 0; i< map_len; i++){
					if(strcmp(&buffer[3], pub_priv_map[i][1]) == 0){
						printf("private ip alreadly in used\n");
						flag = 1;
						break;
					}
				}
				if(flag==1) goto CHECK_ANO_FD;
				inet_ntop(AF_INET, &client.sin_addr, buff, BUFSIZE);
				strcpy(pub_priv_map[map_len][0], buff);
				memcpy(pub_priv_map[map_len][1], &buffer[3],strlen(buffer)-3);
				pub_priv_map[map_len][1][strlen(buffer)-3] = '\0';
				port[map_len] = ntohs(client.sin_port);
				printf("host public ip: %s\tprivate ip : %s\tport#%d\n", pub_priv_map[map_len][0], pub_priv_map[map_len][1],port[map_len]);
				map_len ++;
			}
			else{
				fwrite(buffer, 1, nread, debugfd);
				fflush(debugfd);
				//check packet length
				char a[5];
				memcpy(a, &buffer[4],4);
				a[4] = '\0';
				if ( strtol(a, NULL, 16) * 2  != nread){
					printf("wrong ip packet length\n");
					goto CHECK_ANO_FD;;
				}
				//parse tun private ip address   ex : C0,A8,00,01 => 192.168.0.1 
				char priv_ip[16] = "", ip_ascii[3], tmp[4];
				int priv_ip_len = 0;
				for(int i = 0; i < 4; i++){
					memcpy(ip_ascii, &buffer[32+2*i], 2);
					ip_ascii[2] = '\0';
					sprintf(tmp, "%d", strtol(ip_ascii, NULL, 16));
					strcat(priv_ip, tmp);
					if(i != 3)strcat(priv_ip,".");
				}
				for(int i = 0; i < map_len; i++){
					if(strcmp(priv_ip, pub_priv_map[i][1]) == 0){
						//printf("match %s\t port#%d\n",pub_priv_map[i][0], port[i]);
						memset(&nbiot_DST, 0, sizeof(nbiot_DST));
						nbiot_DST.sin_family = AF_INET;
						nbiot_DST.sin_addr.s_addr = inet_addr(pub_priv_map[i][0]);
						nbiot_DST.sin_port = htons(port[i]);
						if((nwrite=sendto(fd_nbiot, buffer, nread, 0, (const struct sockaddr *) &nbiot_DST, sizeof(nbiot_DST))) < 0){
							perror("send nbiot to host");
							exit(1);
						}
						break;
					}	
				}	
			}
		}
CHECK_ANO_FD:		
		if(FD_ISSET(fd_nbiot, &rd_set)) {
			memset(buffer, 0, BUFSIZE);
			memset(buff, 0, BUFSIZE);
			if((nread=recvfrom(fd_nbiot, buffer, BUFSIZE, 0, ( struct sockaddr *)&client, &rv_len)) < 0){
				perror("Recvfrom data");
				exit(1);
			}
			if(buffer[0] == '+' && buffer[1] == '+' && buffer[2] == '+'){
				//+++special cmd : map private, public ip and store to array
				if(strlen(buffer) > 18){
					printf("wrong hello msg\n");
					continue;
				}
				if(ntohs(client.sin_port) == 0) continue;
				//check is there already a private ip
				int flag = 0;
				for(int i = 0; i< map_len; i++){
					if(strcmp(&buffer[3], pub_priv_map[i][1]) == 0){
						printf("private ip alreadly in used\n");
						flag = 1;
						break;
					}
				}
				if(flag==1) continue;
				inet_ntop(AF_INET, &client.sin_addr, buff, BUFSIZE);
				strcpy(pub_priv_map[map_len][0], buff);
				memcpy(pub_priv_map[map_len][1], &buffer[3],strlen(buffer)-3);
				pub_priv_map[map_len][1][strlen(buffer)-3] = '\0';
				port[map_len] = ntohs(client.sin_port);
				printf("nbiot public ip: %s\tprivate ip : %s\tport#%d\n", pub_priv_map[map_len][0], pub_priv_map[map_len][1],port[map_len]);
				map_len ++;
			}
			else{
				fwrite(buffer, 1, nread, debugfd);
				fflush(debugfd);
				//check packet length
				char a[5];
				memcpy(a, &buffer[4],4);
				a[4] = '\0';
				if ( strtol(a, NULL, 16) * 2  != nread){
					printf("wrong ip packet length\n");
					continue;
				}
				//parse tun private ip address   ex : C0,A8,00,01 => 192.168.0.1 
				char priv_ip[16] = "", ip_ascii[3], tmp[4];
				int priv_ip_len = 0;
				for(int i = 0; i < 4; i++){
					memcpy(ip_ascii, &buffer[32+2*i], 2);
					ip_ascii[2] = '\0';
					sprintf(tmp, "%d", strtol(ip_ascii, NULL, 16));
					strcat(priv_ip, tmp);
					if(i != 3)strcat(priv_ip,".");
				}
				for(int i = 0; i < map_len; i++){
					if(strcmp(priv_ip, pub_priv_map[i][1]) == 0){
						//printf("match %s\t port#%d\n",pub_priv_map[i][0], port[i]);
						memset(&host_DST, 0, sizeof(host_DST));
						host_DST.sin_family = AF_INET;
						host_DST.sin_addr.s_addr = inet_addr(pub_priv_map[i][0]);
						host_DST.sin_port = htons(port[i]);
						if((nwrite=sendto(fd_host, buffer, nread, 0, (const struct sockaddr *) &host_DST, sizeof(host_DST))) < 0){
							perror("send nbiot to host");
							exit(1);
						}
						//printf("write %d bytes to host\n", nwrite);
						break;
					}	
				}	
			}
		}
		
	}
	
	return 0; 
} 
