//
// Created by ray on 18年11月7日.
//

#ifndef INC_0756536_NP_PROJECT2_SP_FUNCT_H
#define INC_0756536_NP_PROJECT2_SP_FUNCT_H

#include<map>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<iostream>
#include<errno.h>
#include<unistd.h>      //unix std包含UNIX系统服務的函數， ex:read、write、getpid
#include<sys/types.h>   //是Unix/Linux系统的基本系统數據類型的header文件， ex:size_t、time_t、pid_t
#include<sys/wait.h>    //wait、waitid、waitpid...
#include<sys/socket.h>
#include<netdb.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include<vector>
#include<string>



#define MAXCOM          15000    // max length of a single-line input
#define MAXBUILTIN 3
#define MAX_CMD_COUNT   15000
#define MAX_MSG_SIZE	1024	// one message has at most 1024 bytes
#define MAX_CLIENT	    30

//#define MAXCMLIST

using namespace std;

class User_pipe{
public:
    User_pipe(const int fd_in = STDIN_FILENO, const int fd_out = STDOUT_FILENO){
        this->fd_in = fd_in;
        this->fd_out = fd_out;
    }
    int fd_in = STDIN_FILENO;
    int fd_out = STDOUT_FILENO;
    void debugger() {
        printf("fd_in:%d, fd_out:%d\n", fd_in, fd_out);
        fflush(stdout);
    }
};

class Num_pipe{
public:
    Num_pipe(const int num = 0, const int fd_in = STDIN_FILENO, const int fd_out = STDOUT_FILENO){
        this->counter = num;
        this->fd_in = fd_in;
        this->fd_out = fd_out;
    }
    bool sub_counter(){
        if(counter > 0){
            this->counter -= 1;
            return true;
        } else
            return false;
    }
    bool is_counter_zero(){
        return this->counter == 0;
    }
    int get_fd_in(){return fd_in;}
    int get_fd_out(){return fd_out;}
    int get_fd_counter(){return counter;}
    void debugger(){
        printf("counter:%d, fd_in:%d\n",counter, fd_in);
        fflush(stdout);
    }
private:
    int counter = 0;
    int fd_in = STDIN_FILENO;
    int fd_out = STDOUT_FILENO;
};


class Client{
public:
    std::string login();
    std::string logout();
    int sockfd;
    int s_id;
    std::string name = "(no name)";
    std::string addr = "CGILAB";
    int port = 511;
    vector<Num_pipe> num_pipe_list;
    map<int, User_pipe> userpipe_map;
    map<string, string> env = {{"PATH","bin:."}};

};

#endif //INC_0756536_NP_PROJECT2_SP_FUNCT_H;

extern vector<Client> client_set;
extern int uid;

typedef vector<string> Command;
void cml_loop();
void cml_loop(int sockfd);
int cml_loop(int sockfd, char* buffer, Client &client);
void initalenv(Client &c);
void sigpipe_handler(int dummy);
void childHandler(int signo);
void welcome(int sockfd);
void broadcast(const char* msg, int length);
void my_bind(int *sockfd, addrinfo **servinfo);
void closepipe(Client &c);

int is_num_pipe(const string line, bool *e_flag);
string cml_read_line();
string dele_num_pipe(const string line);
vector<string> cml_split_line(const string& s, char delimiter);
vector<string> cml_split_space(string line);
vector<int> cml_execute(vector<Command> command, bool is_num_pipe
        , bool is_errpr_pipe, vector<int> real_in, int last_out);
vector<int> execute_cmd_seq(vector<Command> command, bool is_num_pipe, bool is_error_pipe,
                            vector<int> real_in, int last_out);
void args_execute(Command tokens, int fd_in, int fd_out,
                  int pipeline_count, int pipes_fd[][2], bool is_error_pipe);

int readline(int fd, char *ptr, int maxlen);
int to_user_pipe(string &line);
int from_user_pipe(string &line);
int find_index(int sockfd);
int find_index_byID(int id);
int printenv (int argc, vector<string> argv);
int setupenv (int argc, vector<string> argv);
int yell     (int argc, vector<string> argv);
int tell     (int argc, vector<string> argv);
int name     (int argc, vector<string> argv);
int who ();

void *get_in_addr(struct sockaddr *sa);
in_port_t get_in_port(struct sockaddr *sa);

std::string login(Client c);
std::string logout(Client c);
