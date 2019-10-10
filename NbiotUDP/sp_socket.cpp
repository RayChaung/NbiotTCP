//
// Created by ray on 18年11月4日.
//


#include <vector>
#include "sp_funct.h"

int readline(int fd, char *ptr, int maxlen)
{
    int n,rc;
    char c;
    *ptr = 0;
    for(n=1;n<maxlen;n++)
    {
        if((rc=read(fd,&c,1)) == 1)
        {
            if(c=='\n')  break;
            *ptr++ = c;

        }
        else if(rc==0)
        {
            if(n==1)     return(0);
            else         break;
        }
        else
            return(-1);
    }
    *ptr = 0;
    return(n);
}


//send Welcome message
void welcome(int sockfd)
{
    char *msg = "****************************************\n"
                "** Welcome to the information server. **\n"
                "****************************************\n";
    size_t len;
    len = strlen(msg);
    send(sockfd, msg, len, 0);
}

int find_index(int sockfd)
{
    int res = -1;
    int index = 0;
    int flag = 0;
    for(auto &p : client_set)
    {
        if(sockfd == p.sockfd){
            res = index;
            flag ++;
        }
        index ++;
    }
    if(flag > 1)
        return -1;
    else
        return  res;
}

int find_index_byID(int id)
{
    int res = -1;
    int index = 0;
    int flag = 0;
    for(auto &p : client_set)
    {
        if(id == p.s_id){
            res = index;
            flag ++;
        }
        index ++;
    }
    if(flag > 1)
        return -1;
    else
        return  res;
}

std::string Client::login() {
    std::string login = "*** User '";
    login = login
            + this->name + "' entered from "
            + this->addr
            + "/"
            + std::to_string(this->port)
            + ". ***\n";
    return login;
}

std::string Client::logout() {
    std::string logout = "*** User '";
    logout = logout
             + this->name + "'"
             //+ this->addr
             //+ "/"
             //+ std::to_string(this->port)
             + " left. ***\n";
    return logout;
}

void broadcast(const char* msg, int length)
{
    for (auto &c : client_set) {
        if (send(c.sockfd, msg, length, 0) == -1) {
            perror("send");
        }
    }
}

// get port, IPv4 or IPv6:
in_port_t get_in_port(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return (((struct sockaddr_in*)sa)->sin_port);
    }

    return (((struct sockaddr_in6*)sa)->sin6_port);
}

// 取得sockaddr，IPv4或IPv6：
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void my_bind(int *sockfd, addrinfo **servinfo)
{
    struct addrinfo *p;
    int yes=1;
    // 以迴圈找出全部的結果，並 bind 到第一個能用的結果
    for(p = *servinfo; p != NULL; p = p->ai_next) {
        if ((*sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if (setsockopt(*sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(*sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            perror("server: bind");
            close(*sockfd);
            continue;
        }
        break;
    }

    if (p == NULL) {
        fprintf(stderr, "server: failed to bind\n");
        exit(2);
    }

    freeaddrinfo(*servinfo); // 全部都用這個 structure

}

//built_in
int printenv (int argc, vector<string> argv)
{
    if(getenv(argv[1].c_str())){
        printf ("%s\n",getenv(argv[1].c_str()));
        fflush(stdout);
    }
    return 1;
}
int setupenv (int argc, vector<string> argv)
{
    if (argc == 3) {
        int	index = find_index_byID(uid);
        for (auto & x : client_set[index].env)
        {
            if(x.first == argv[1]){
                x.second = argv[2];
                return 1;
            }
        }
        client_set[index].env.insert(std::pair<string,string>(argv[1],argv[2]));
    } else
        fputs ("Usage: setenv <name> <value>\n", stderr);
    return 1;
}
int name (int argc, vector<string> argv)
{
    int	index = find_index_byID(uid);
    std::string msg = "***  User from ";
    msg = msg + client_set[index].addr + "/"
              + std::to_string(client_set[index].port) + " is named '"
              + argv[1] + "'. ***\n";
    if (argc == 2) {
        for (auto &c : client_set) {
            if (c.name == argv[1]) {
                // 'name' already exists changing will be failed
                printf("*** User '%s' already exists. ***\n", c.name.c_str());
                return 1;
            }
        }
        client_set[index].name = argv[1];
        broadcast(msg.c_str(),msg.length());
    } else
        fputs ("Usage: name <name> <value>\n", stderr);
    return 1;
}
int yell (int argc, vector<string> argv)
{
    int	index;
    index = find_index_byID(uid);
    std::string msg = "*** ";
    msg = msg + client_set[index].name
              + " yelled ***: ";
    for (int i = 1; i < argc; i++) {
        msg += argv[i];
        if (i == argc - 1)
            msg += "\n";
        else
            msg += " ";
    }
    broadcast (msg.c_str(), msg.length());
    return 1;
}
int tell (int argc, vector<string> argv)
{
    int	s_index = find_index_byID(uid);
    int rid = stoi(argv[1]), r_index = find_index_byID(rid) ;
    std::string msg ;
    if ( r_index  < 0 ){
        // receiver id not find *** Error: user #5 does not exist yet. ***
        msg = "*** Error: user #" + to_string(rid) + " does not exist yet. ***\n";
        write (STDERR_FILENO, msg.c_str(), msg.length());
    } else{
        msg = "*** " + client_set[s_index].name + " told you ***: ";
        for (int i = 2; i < argc; i++) {
            msg += argv[i];
            if (i == argc - 1)
                msg += "\n";
            else
                msg += " ";
        }
        write (client_set[r_index].sockfd, msg.c_str(), msg.length());
    }
    return 1;
}
int who ()
{
    int	index;
    char	msg[MAX_MSG_SIZE + 1];
    snprintf (msg, MAX_MSG_SIZE + 1, "<ID>\t<nickname>\t<IP/port>\t<indicate me>\n");
    write (STDOUT_FILENO, msg, strlen (msg));
    for (int idx = 1; idx <= MAX_CLIENT; idx++) {
        if ( (index =find_index_byID(idx)) >= 0) {
            if ((client_set[index].name).length() < 8)
                snprintf (msg, MAX_MSG_SIZE + 1, "%d\t%s\t\t%s/%d"
                        , client_set[index].s_id, client_set[index].name.c_str(), client_set[index].addr.c_str(), client_set[index].port);
            else
                snprintf (msg, MAX_MSG_SIZE + 1, "%d\t%s\t%s/%d"
                        , client_set[index].s_id, client_set[index].name.c_str(), client_set[index].addr.c_str(), client_set[index].port);
            if (idx == uid)
                strcat (msg, "\t\t<-me\n");
            else
                strcat (msg, "\n");
            write (STDOUT_FILENO, msg, strlen(msg));
        }
    }
    return 1;
}
void initalenv(Client &c)
{
    for (auto const& x : c.env)
    {
        setenv(x.first.c_str(), x.second.c_str(), 1);
    }
}

