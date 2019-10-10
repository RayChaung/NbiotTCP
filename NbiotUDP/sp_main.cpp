//
// Created by ray on 18年11月7日.
//

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<algorithm>
#include<errno.h>
#include<unistd.h>      //unix std包含UNIX系统服務的函數， ex:read、write、getpid
#include<sys/types.h>   //是Unix/Linux系统的基本系统數據類型的header文件， ex:size_t、time_t、pid_t
#include<sys/wait.h>    //wait、waitid、waitpid...
#include<signal.h>
#include "sp_funct.h"
#define BACKLOG 30 // 在佇列中可以有多少個連線在等待


using namespace std;
vector<Client> client_set;

int main(int argc, char *argv[]) {

    //putenv(const_cast<char *>("PATH=bin:."));

    fd_set master; // master file descriptor 清單
    fd_set read_fds; // 給 select() 用的暫時 file descriptor
    int oldstdout, oldstderr;
    int fdmax; // 最大的 file descriptor 數目

    int listenerfd; // listening socket descriptor
    int newfd; // 新接受的 accept() socket descriptor
    struct sockaddr_storage remoteaddr; // client address 連線者的位址資訊
    struct addrinfo hints, *servinfo, *p;
    socklen_t addr_size;

    char* port = "7001";

    char remoteIP[INET6_ADDRSTRLEN];
    int rv;

    FD_ZERO(&master); // 清除 master 與 temp sets
    FD_ZERO(&read_fds);

    if(argv[1])
    {
        port = argv[1];
    }

    memset(&hints, 0, sizeof hints); // 確保struct為空
    hints.ai_family = AF_UNSPEC; //  use IPv4 or IPv6, whichever
    hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
    hints.ai_flags = AI_PASSIVE; // fill in my IP for me

    // 準備好連線
    if ((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    //bind 到第一個能用的結果,且listen
    my_bind(&listenerfd, &servinfo);

    //listen
    if (listen(listenerfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }


    // 將 listener 新增到 master set
    FD_SET(listenerfd, &master);

    // 持續追蹤最大的 file descriptor
    fdmax = listenerfd; // 到此為止，就是它了

    printf("server: waiting for connections...\n");

    // maintain available ID
    vector<int> IdCells;
    for (int i = 1; i <= MAX_CLIENT; i++)
    {
        IdCells.push_back(i);   // Push the int into the temporary vector<int>
    }

    while(1) { // 主要的 accept() 迴圈
        //config files
        clearenv ();
        auto *buffer = (char*)malloc(sizeof(char) * MAXCOM);
        int nbytes;

        read_fds = master; // 複製 master

        while ((select(fdmax + 1, &read_fds, NULL, NULL, NULL) < 0 )) {
            if(errno == EINTR)
                usleep (1000);
            else{
                perror("select");
                exit(4);
            }
        }
        // 在現存的連線中尋找需要讀取的資料
        for (int s = 0; s <= fdmax; s++) {
            if (FD_ISSET(s, &read_fds)) { // 我們找到一個！！
                if (s == listenerfd) {
                    // handle new connections
                    addr_size = sizeof remoteaddr;
                    newfd = accept(listenerfd, (struct sockaddr *) &remoteaddr, &addr_size);

                    if (newfd == -1) {
                        perror("accept");
                    } else {
                        FD_SET(newfd, &master); // 新增到 master set
                        if (newfd > fdmax) { // 持續追蹤最大的 fd
                            fdmax = newfd;
                        }

                        int minElement = *std::min_element(IdCells.begin(), IdCells.end());

                        // 歡迎 new client
                        welcome(newfd);
                        Client c;
                        c.s_id = minElement;
                        // 刪除此ID
                        IdCells.erase(std::find(IdCells.begin(),IdCells.end(),minElement));
                        c.sockfd = newfd;
                        //c.addr = inet_ntop(remoteaddr.ss_family,
                        //                   get_in_addr((struct sockaddr *) &remoteaddr),
                        //                   remoteIP, INET6_ADDRSTRLEN);
                        //c.port = ntohs(get_in_port((struct sockaddr *) &remoteaddr));

                        client_set.push_back(c);
                        string login_s = c.login();
                        broadcast(login_s.c_str(), login_s.length());


                        // 準備進到shell
                        if(send(newfd, "% ", 3, 0) == -1){
                            perror("send");
                            exit(1);
                        }
                    }

                } else {
                    // 處理來自 client 的資料
                    if ((nbytes = readline(s, buffer, MAXCOM)) <= 0) {
                    //if ((nbytes = recv(s, buffer, sizeof buffer, 0)) <= 0) {
                        // got error or connection closed by client
                        int index ;
                        if((index = find_index(s)) < 0){
                            perror("find");
                        }
                        if (nbytes == 0) {
                            string logout_s = client_set[index].logout();
                            broadcast(logout_s.c_str(), logout_s.length());
                            // 關閉連線
                            printf("selectserver id: %d hung up\n",client_set[index].s_id);
                            fflush(stdout);
                        } else {
                            perror("recv");
                        }
                        IdCells.push_back(client_set[index].s_id);
                        closepipe(client_set[index]);
                        client_set.erase(client_set.begin() + (index) );
                        close(s); // bye!
                        FD_CLR(s, &master); // 從 master set 中移除

                    } else {
                        // 我們從 client 收到一些資料
                        printf("--%d--",nbytes);
                        fflush(stdout);
                        //broadcast(fdmax, master, listenerfd, buffer, nbytes);

                        //創建STDOUT & STDERR 的備份
                        oldstdout = dup(STDOUT_FILENO);
                        oldstderr = dup(STDERR_FILENO);

                        dup2(s, STDOUT_FILENO);
                        dup2(s, STDERR_FILENO);
                        // do shell
                        int index ;
                        if((index = find_index(s)) < 0){
                            perror("find");
                        }
                        if( !cml_loop(s, buffer, client_set[index]) ) {
                            string logout_s = client_set[index].logout();
                            broadcast(logout_s.c_str(), logout_s.length());
                            // 關閉連線
                            IdCells.push_back(client_set[index].s_id);
                            closepipe(client_set[index]);
                            client_set.erase(client_set.begin() + (index) );
                            close(s); // bye!
                            FD_CLR(s, &master); // 從 master set 中移除
                            dup2(oldstdout,STDOUT_FILENO);
                            dup2(oldstderr,STDERR_FILENO);
                            close(oldstdout);
                            close(oldstderr);
                            printf("selectserver sock: %d hung up\n",s);
                            fflush(stdout);
                        } else{
                            dup2(oldstdout,STDOUT_FILENO);
                            dup2(oldstderr,STDERR_FILENO);
                            close(oldstdout);
                            close(oldstderr);
                            // 準備再次進到shell
                            if(send(s, "% ", 3, 0) == -1){
                                perror("send");
                                exit(1);
                            }
                        }
                    }
                } // END handle data from client
            } // END got new incoming connection
        } // END looping through file descriptors

        //debug
        printf("available id:\n");
        for(auto &a : client_set){
            printf("id:%d, sockfd:%d ,\nuser_pipe:\n",a.s_id,a.sockfd);
            for(auto &b : a.userpipe_map)
            {
                printf("sender:%d ",b.first);
                b.second.debugger();
            }
        }

        printf("\n");
        fflush(stdout);
    }
    close(listenerfd);

    exit(EXIT_SUCCESS);
}