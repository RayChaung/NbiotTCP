//
// Created by ray on 18年11月7日.
//

#include<sstream>
#include<iterator>
#include<cstring>
#include<unistd.h>
#include<errno.h>
#include<sys/socket.h>
#include<netdb.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include<sys/types.h>
#include<sys/wait.h>    //wait、waitid、waitpid...
#include<sys/stat.h>
#include<fcntl.h>
//#include<readline/readline.h>
//#include<boost/algorithm/string.hpp>
//#include<boost/algorithm/string/classification.hpp>

#include "sp_funct.h"

using namespace std;

static vector<pid_t> pid_table;
static string orig_line;
int uid;

int cml_loop(int sockfd, char* buffer, Client &client)
{
    // set environment
    initalenv(client);

    int status = 1;
    int n_flag, up_flag;
    bool e_flag = false;
    vector<int> fd_in_out = { STDIN_FILENO, STDOUT_FILENO};
    int last_out = STDOUT_FILENO;
    string line;
    vector<string> tokens;
    vector<Command> command;
    uid = client.s_id;

    line = string(buffer);
    // 確保原始cml給user pipe print出
    orig_line = line;

    //not null command
    //size = 1 represent line only has '\0'
    if(line.size()!=1) {
        // 判斷各種pipe
        // numb pipe input
        n_flag = is_num_pipe(line , &e_flag);

        // num_pipe中的counter減1
        for (auto P = client.num_pipe_list.begin(); P != client.num_pipe_list.end();) {
            if (P->sub_counter()) {
                if(P->is_counter_zero() ){
                    fd_in_out[0] = P->get_fd_in();
                    last_out = P->get_fd_out();
                    client.num_pipe_list.erase(P);
                } else
                    P++;
            } else{
                P++;
            }
        }

        // from user pipe
        int fup_flag = from_user_pipe(line);
        if( fup_flag  < 0)
            return 1;
        else if(fup_flag){
            // 找到此sender的pipe資訊
            fd_in_out[0] = client.userpipe_map.find(fup_flag)->second.fd_in;    // in接到新的input
            last_out = client.userpipe_map.find(fup_flag)->second.fd_out;       // 尚未關的out接到lastout
            // 刪掉此user_pipe
            client.userpipe_map.erase(fup_flag);
        }
        // user pipe input
        if((up_flag = to_user_pipe(line)) < 0)
            return 1;

        for(auto &p : client.num_pipe_list){
            //p.debugger();
            if(n_flag && n_flag == p.get_fd_counter()) {
                //進同一個pipe
                fd_in_out[1] = p.get_fd_out();
                //flag清空，此command蛻變為一般command僅output進pipe(非stdout)
                n_flag = 0;
                line = dele_num_pipe(line);
                //cout << line << endl ;
                break;
            }
        }

        //number pipe先暫存在command_list中
        if (n_flag || up_flag) {
            if(n_flag)
                tokens = cml_split_line(dele_num_pipe(line), '|');
            else
                tokens = cml_split_line(line, '|');
            for (auto const &a : tokens) {
                command.push_back(cml_split_space(a));
            }
            vector<int> debugger = cml_execute(command, true, e_flag, fd_in_out, last_out);
            status = debugger[2];
            //cout << "here will implement number pipe\n";
            //cout << dele_num_pipe(line) << "num: " << flag << endl;
            //cout << " in_pipe_num: " << debugger[0] << endl;
            //cout << " out_pipe_num: " << debugger[1] << endl;
            if(n_flag){
                Num_pipe num_pipe(n_flag, debugger[1], debugger[0]);
                client.num_pipe_list.push_back(num_pipe);
            }
            else if(up_flag){
                int index = find_index_byID(up_flag);
                User_pipe user_pipe(debugger[1], debugger[0]);
                client_set[index].userpipe_map.insert(std::pair<int,User_pipe>(uid, user_pipe));
            }

        } else {
            //若不是number pipe直接執行
            tokens = cml_split_line(line, '|');
            //cout << "full cml:" << line << endl;
            for (auto const &a : tokens) {
                command.push_back(cml_split_space(a));
            }
            status = cml_execute(command, false, e_flag, fd_in_out, last_out)[2];
        }

    }
    return status;

}

string dele_num_pipe(const string line)
{
    vector<string> tmp = cml_split_space(line);
    //delete last number and (| or ! )
    tmp.pop_back();
    string result;
    for (auto const& s : tmp) { result += s + " "; };
    return result;
}
//ex: "|5" 回傳 5 ,default return 0
int is_num_pipe(const string line , bool *e_flag)
{
    vector<string> tmp = cml_split_space(line);
    string cmp = tmp.back();
    if(cmp[0] == '|' || cmp[0] == '!' ){
        if(cmp[0] == '!')
            *e_flag = true;
        cmp.erase(0,1);
        return stoi(cmp);
    }
    return 0;
}
//ex: ">5" 回傳 5 (id),default return 0, <0 表錯誤
int to_user_pipe(string &line)
{
    string msg;
    vector<string> tmp, orig;
    tmp = cml_split_space(line);
    orig = cml_split_space(orig_line);
    //auto P = client.num_pipe_list.begin(); P != client.num_pipe_list.end();
    for(auto i = tmp.begin(); i != tmp.end();){
        if((*i)[0] == '>' && i->size() > 1){
            string tmp_id;
            tmp_id = *i;
            tmp_id.erase(0,1);
            int rid = stoi(tmp_id);
            int s_index = find_index_byID(uid);
            int r_index = find_index_byID(rid);
            // 找不到id
            if( r_index < 0 ){
                // receiver id not find
                msg = "*** Error: user #" + to_string(rid) + " does not exist yet. ***\n";
                write (STDERR_FILENO, msg.c_str(), msg.length());
                return -1;
            }
            else{
                for(auto &j : client_set[r_index].userpipe_map)
                {
                    // 已經傳過 *** Error: the pipe #<sender_id>->#<receiver_id> already exists. ***
                    if(j.first == uid)
                    {
                        msg = "*** Error: the pipe #" + to_string(uid) + "->#" + to_string(rid) + " already exists. ***\n";
                        write (STDERR_FILENO, msg.c_str(), msg.length());
                        return -1;
                    }
                }
                // *** <sender_name> (#<sender_id>) just piped ’<command>’ to <receiver_name> (#<receiver_id>) ***
                msg = "*** "+ client_set[s_index].name +" (#" + to_string(uid)
                      + ") just piped '";
                for (auto s = orig.begin(); s != orig.end(); s++)
                {
                    msg += *s;
                    if(s != orig.end()-1)
                        msg += " ";
                }
                msg = msg + "' to "
                        + client_set[r_index].name +" (#" + to_string(rid)
                        + ") ***\n";
                broadcast(msg.c_str(), msg.length());
                tmp.erase(i);
                line.clear();
                for (auto const& s : tmp) { line += s + " "; };
                return rid;
            }
        }
        else
            i++;
    }
    return 0;

}

//ex: "<5" 回傳 5 (id),default return 0, <0 表錯誤
int from_user_pipe(string &line)
{
    string msg;
    vector<string> tmp, orig;
    tmp = cml_split_space(line);
    orig = cml_split_space(orig_line);
    //auto P = client.num_pipe_list.begin(); P != client.num_pipe_list.end();
    for(auto i = tmp.begin(); i != tmp.end();){
        if((*i)[0] == '<' && i->size() > 1){
            string tmp_id;
            tmp_id = *i;
            tmp_id.erase(0,1);
            int sid = stoi(tmp_id);
            int my_index = find_index_byID(uid);
            int s_index  = find_index_byID(sid);
            // 找不到sender id
            if( s_index < 0 ){
                // receiver id not find
                msg = "*** Error: user #" + to_string(sid) + " does not exist yet. ***\n";
                write (STDERR_FILENO, msg.c_str(), msg.length());
                return -1;
            }
            else{
                for(auto &j : client_set[my_index].userpipe_map)
                {
                    // 找到sender *** <receiver_name> (#<receiver_id>) just received from <sender_name> (#<sender_id>) by ’<command>’ ***
                    if(j.first == sid)
                    {
                        msg = "*** "+ client_set[my_index].name +" (#" + to_string(uid)
                              + ") just received from "
                              + client_set[s_index].name +" (#" + to_string(sid)
                              + ") by '";
                        for (auto s = orig.begin(); s != orig.end(); s++)
                        {
                            msg += *s;
                            if(s != orig.end()-1)
                                msg += " ";
                        }
                        msg += "' ***\n";
                        broadcast(msg.c_str(), msg.length());
                        tmp.erase(i);
                        line.clear();
                        for (auto const& s : tmp) { line += s + " "; };
                        return sid;
                    }
                }
                // sender沒有傳過 *** Error: the pipe #<sender_id>->#<receiver_id> does not exist yet. ***
                msg = "*** Error: the pipe #" + to_string(sid) + "->#" + to_string(uid) + " does not exist yet. ***\n";
                write (STDERR_FILENO, msg.c_str(), msg.length());
                return -1;

            }
        }
        else
            i++;
    }
    return 0;

}

string cml_read_line()
{
    //char *line = NULL;
    string line;
    getline(cin, line);
    return line;
}
vector<string> cml_split_line(const string& s, char delimiter)
{
    vector<string> tokens;
    string token;
    istringstream tokenStream(s);
    while (getline(tokenStream, token, delimiter))
    {
        tokens.push_back(token);
    }
    return tokens;
}

vector<string> cml_split_space(string line)
{
    istringstream iss(line);
    vector<std::string> results((istream_iterator<string>(iss)),
                                istream_iterator<string>());
    return results;
}
vector<int> cml_execute(vector<Command> command, bool is_num_pipe
        , bool is_errpr_pipe, vector<int> real_in, int last_out)

{
    //built-in command
    vector<int> nothing;
    nothing.push_back(0);
    nothing.push_back(1);
    if(command.size() == 1) {
        vector<string> tokens = command[0];
        int res = 1;
        /* execute the command accordingly */
        if (tokens[0] == "exit") {
            if(real_in[0] != STDIN_FILENO){
                close(real_in[0]);
            }
            if(last_out != STDOUT_FILENO) {
                close(last_out);
            }
            res = 0;
            nothing.push_back(res); //status
            return nothing;
        } else if (tokens[0] == "printenv") {
            res = printenv (tokens.size(), tokens);
            nothing.push_back(res); //status
            return nothing;
        } else if (tokens[0] == "setenv") {
            res = setupenv (tokens.size(), tokens);
            nothing.push_back(res); //status
            return nothing;
        } else if (tokens[0] == "who") {
            res = who ();
            nothing.push_back(res); //status
            return nothing;
        } else if (tokens[0] == "name") {
            res = name (tokens.size(), tokens);
            nothing.push_back(res); //status
            return nothing;
        } else if (tokens[0] == "tell") {
            res = tell (tokens.size(), tokens);
            nothing.push_back(res); //status
            return nothing;
        } else if (tokens[0] == "yell") {
            res = yell(tokens.size(), tokens);
            nothing.push_back(res); //status
            return nothing;
        }
    }
    //common command
    nothing = execute_cmd_seq(command, is_num_pipe, is_errpr_pipe, std::move(real_in), last_out);
    nothing.push_back(1);
    return nothing;
}

vector<int> execute_cmd_seq(vector<Command> command, bool is_num_pipe, bool is_error_pipe,
                            vector<int> real_in, int last_out)
{
    pid_table.clear();
    int fd_in = real_in[0];
    int fd_out = STDOUT_FILENO;
    bool e_flag = false;
    if(last_out != STDOUT_FILENO){
        close(last_out);
        //cout << "close(0):" << last_out << endl;
    }

    int cmd_count = static_cast<int>(command.size());
    int pipeline_count;
    if(is_num_pipe)
        pipeline_count = cmd_count;
    else
        pipeline_count = cmd_count - 1;
    int pipes_fd[MAX_CMD_COUNT][2];
    bool is_not_stdin = real_in[0] != STDIN_FILENO;
    for (int C = 0; C < cmd_count; C++)
    {
        if(is_num_pipe || C != cmd_count - 1){
            if (pipe(pipes_fd[C]) == -1)
            {
                fprintf(stderr, "Error: Unable to create pipe. (%d)\n", C);
                exit(EXIT_FAILURE);
            }
        }
        if(C != 0)
            fd_in = pipes_fd[C-1][0];

        if(!is_num_pipe && C == cmd_count - 1){
            if(real_in[1] == STDOUT_FILENO)
                fd_out = STDOUT_FILENO;
            else
                fd_out = real_in[1];
        }
        else
            fd_out = pipes_fd[C][1];
        //check if it is error pipe
        if(is_num_pipe && C == cmd_count - 1){
            if(is_error_pipe)
                e_flag = true;
        }
        //cout << "in:" << fd_in << " out:"<< fd_out << endl;

        /* use creat_proc fork() Child Process */
        args_execute(command[C], fd_in, fd_out, pipeline_count, pipes_fd, e_flag);

        if(C == 0 && is_not_stdin){
            close(fd_in);
            //cout << "close(1):" << fd_in << endl;
        }

        if(is_num_pipe && C == pipeline_count - 1)
        {
            if(fd_in != STDIN_FILENO) {
                close(fd_in);
                //cout << "C:" << C << " close(2):" << fd_in << endl;
            }
            fd_out = pipes_fd[C][0];
            fd_in = pipes_fd[C][1];
        } else {
            if(fd_in != STDIN_FILENO) {
                close(fd_in);
                //cout << "C:" << C << " close(3):" << fd_in << endl;
            }
            if(fd_out != real_in[1]) {
                close(fd_out);
                //cout << "C:" << C << " close(4):" << fd_out << endl;

            }
        }

    }
    //cout << "return in:" << fd_in << " out:"<< fd_out << endl;
    //後面條件為確保此Non-number不為導入別人pipe退化的number pipe
    if(!is_num_pipe && fd_out == STDOUT_FILENO) {
        for (int &pid : pid_table) {
            int child;
            waitpid(pid, &child, 0);
        }
    }
    vector<int> result = {fd_in,fd_out};
    return result;
}

void args_execute(Command tokens, int fd_in, int fd_out,
                  int pipeline_count, int pipes_fd[][2], bool is_error_pipe)
{
    /*cout << "error pipe? " << is_error_pipe << "\nfork():" ;
    for(auto &c : tokens){
        cout << c << " ";
    }
    cout << endl;*/
    pid_t pid;
    signal(SIGPIPE, sigpipe_handler);
    signal(SIGCHLD, SIG_IGN);

    while((pid = fork()) < 0) {
        usleep (1000);
    }
    pid_table.push_back(pid);
    if(pid == 0){
        //Child process
        vector<char*> tokenc;
        int fd;     //for redirection File
        string path;
        path += tokens[0];

        /* change fd_in & fd_out to stdin & stdout */
        if (fd_in != STDIN_FILENO){
            dup2(fd_in, STDIN_FILENO);
            close(fd_in);
        }
        if(is_error_pipe)
            dup2(fd_out, STDERR_FILENO);
        if (fd_out != STDOUT_FILENO){
            dup2(fd_out, STDOUT_FILENO);
            close(fd_out);
        }
        for(auto i = tokens.begin(); i != tokens.end(); i++){

            if(*i == ">"){
                //implement redirection
                //printf("%s\n",(i+1)->c_str());
                //fflush(stdout);
                if((fd = open((i+1)->c_str(), O_RDWR | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR)) < 0) {
                    fprintf(stderr, "Can't open - (%s)\n", strerror(errno));
                    exit(EXIT_FAILURE);
                }
                if(dup2(fd, STDOUT_FILENO) < 0){
                    fprintf(stderr, "Can't dup2 - (%s)\n", strerror(errno));
                    exit(EXIT_FAILURE);
                }
                close(fd);
                break;
            }
            tokenc.emplace_back(const_cast<char *>(i->c_str()));

        }
        //NULL terminate
        tokenc.push_back(nullptr);

        if(execvp(path.c_str(),tokenc.data()) < 0 ){
            //perror("Unknown command");
            fprintf(stderr,"Unknown command: [%s].\n",tokenc[0]);
        }
        exit(EXIT_FAILURE);
    }
}

void closepipe(Client &c)
{
    // close number pipe
    for (auto &i : c.num_pipe_list)
    {
        close(i.get_fd_in());
        close(i.get_fd_out());
        //printf("close(1): %d,%d",i.get_fd_in(),i.get_fd_out());
    }
    // close myself user pipe
    for (auto &i : c.userpipe_map)
    {
        close(i.second.fd_in);
        close(i.second.fd_out);
        //printf("close(2): %d,%d",i.second.fd_in,i.second.fd_out);
    }
    // close i send user pipe
    for (auto &i : client_set)
    {
        for (auto &j : i.userpipe_map)
        {
            if(j.first == c.s_id)
            {
                close(j.second.fd_in);
                close(j.second.fd_out);
                //printf("close(3): %d,%d",j.second.fd_in,j.second.fd_out);
                i.userpipe_map.erase(c.s_id);
            }
        }
    }
}

void sigpipe_handler(int dummy)
{
    fprintf(stderr, "%d: caught a SIGPIPE\n", getpid());
    exit(1);
}

void childHandler(int signo) {
    int status;
    pid_t pid ;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        //do nothing
    }
}