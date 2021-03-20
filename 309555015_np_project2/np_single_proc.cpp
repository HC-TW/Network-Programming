#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <map>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_ARG_COUNT (15)
#define MAX_CMD_COUNT (100)

extern char **environ;

using namespace std;

struct number_pipe
{
    char sign;  // '|' or '!'
    int number; // output to next ith command line
    int npipe[2];
    int waitP; // waitPid NOHANG count
    number_pipe()
    {
        sign = '|';
        number = 0;
        npipe[0] = STDIN_FILENO;
        npipe[1] = STDOUT_FILENO;
        waitP = 0;
    }
};
struct user_pipe
{
    int senderId;
    int receiverId;
    int upipe[2];
    user_pipe()
    {
        senderId = 0;
        receiverId = 0;
        upipe[0] = STDIN_FILENO;
        upipe[1] = STDOUT_FILENO;
    }
};
struct User
{
    int id;
    string name;
    string ip;
    int port;
    int sockfd;
    User()
    {
        id = 0;
        name = "(no name)";
        ip = "";
        port = 0;
        sockfd = 0;
    }
};

void error(const char *msg)
{
    perror(msg);
    exit(1);
}
/* Load a command sequence from standard input. */
/* Push command to string vector */
vector<string> readAndParseCommandSeq(string line, struct number_pipe &numberPipeOut, struct user_pipe &userPipeIn, struct user_pipe &userPipeOut, string &userPipeCmd)
{
    string str_temp = "";
    string cmd_temp = "";
    vector<string> cmd;
    stringstream ss;

    ss.str(line);
    for (int i = 0; ss >> str_temp; i++) // extract string from line
    {
        userPipeCmd += str_temp + " ";
        if (str_temp != "|")
        {
            if ((str_temp[0] != '|' && str_temp[0] != '!' && str_temp[0] != '<' && str_temp[0] != '>') || str_temp == ">") // command sequence
            {
                cmd_temp += str_temp + " ";
            }
            else
            {
                bool isPipe = true;
                for (int j = 1; j < str_temp.length(); j++)
                {
                    if (!isdigit(str_temp[j]))
                        isPipe = false;
                }
                if (!isPipe)
                {
                    cmd_temp += str_temp + " ";
                }
                else
                {
                    stringstream ss2(str_temp.substr(1));
                    if (str_temp[0] == '|' || str_temp[0] == '!') // number pipe
                    {
                        if (str_temp[0] == '!') 
                            numberPipeOut.sign = '!';
                        ss2 >> numberPipeOut.number;
                    }
                    else
                    {
                        if (str_temp[0] == '<') // user pipe
                            ss2 >> userPipeIn.senderId;
                        else
                            ss2 >> userPipeOut.receiverId;
                    }
                }
            }
        }
        else // meet '|', command ends
        {
            cmd_temp.pop_back(); // remove space from the end of string
            cmd.push_back(cmd_temp);
            cmd_temp = "";
        }
    }
    if (cmd_temp != "")
    {
        userPipeCmd.pop_back(); // remove space from the end of string
        cmd_temp.pop_back(); 
        cmd.push_back(cmd_temp);
    }
    return cmd;
}

/* Purpose: Create child process and redirect io. */
int createProcess(char **argv, int fd_in, int fd_out, int pipeline_count, int pipes_fd[][2], char sign, map<int, vector<struct number_pipe> > fdNpipes, struct number_pipe numberPipeIn, vector<struct user_pipe> upipes, string &userPipeCmd, int sockfd)
{
    pid_t pid;
    while ((pid = fork()) < 0)
    {
        int status;
        wait(&status);
        // error("Error: Unable to fork.\n");
    }
    if (pid == 0)
    {
        /* dup fd to STDIN, STDOUT, STDERR if needed*/
        if (fd_in != STDIN_FILENO)
        {
            dup2(fd_in, STDIN_FILENO);
        }
        if (fd_out != STDOUT_FILENO)
        {
            dup2(fd_out, STDOUT_FILENO);
        }
        if (sign == '!')
        {
            dup2(fd_out, STDERR_FILENO);
        }
        else
        {
            dup2(sockfd, STDERR_FILENO);
        }
        /* close useless pipe */
        for (int P = 0; P < pipeline_count; P++)
        {
            if (P == MAX_CMD_COUNT)
                break;
            close(pipes_fd[P][0]);
            close(pipes_fd[P][1]);
        }
        for (int i = 0; i < fdNpipes.size(); i++)
        {
            vector<struct number_pipe> npipes = fdNpipes[i];
            for (int j = 0; j < npipes.size(); j++)
            {
                close(npipes[j].npipe[0]);
                close(npipes[j].npipe[1]);
            }
        }
        for (int i = 0; i < upipes.size(); i++)
        {
            close(upipes[i].upipe[0]);
            close(upipes[i].upipe[1]);
        }
        if (numberPipeIn.number == 0)
        {
            close(numberPipeIn.npipe[0]);
            close(numberPipeIn.npipe[1]);
        }
        /* execute command */
        if (execvp(argv[0], argv) == -1)
        {
            userPipeCmd = "";
            fprintf(stdout, "Unknown command: [%s].\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }
    return pid;
}

/* built-in command: set environment variable */
void setEnv(char *cstr, map<string, string> &env)
{
    char *pch[2];
    strtok(cstr, " ");
    for (int i = 0; i < 2; i++)
    {
        pch[i] = strtok(NULL, " ");
    }
    setenv(pch[0], pch[1], 1);
    env[pch[0]] = pch[1];
}

/* clear environment variables */
void clearEnv()
{
    int i = 1;
    char *s = *environ;
    for (; s; i++)
    {
        unsetenv(strtok(s, "="));
        s = *(environ + i);
    }
}

/* built-in command: print environment variable */
void printEnv(char *cstr, int sockfd)
{
    char *pch;
    strtok(cstr, " ");
    pch = strtok(NULL, " ");
    if ((pch = getenv(pch)) != NULL)
    {
        strcat(pch, "\n");
        write(sockfd, pch, strlen(pch));
    }
}

/* set up client env */
void setClientEnv(map<string, string> env)
{
    for (auto &element : env)
    {
        setenv(element.first.c_str(), element.second.c_str(), 1);
    }
}

/* built-in command: list user */
void who(int sockfd, struct User users[])
{
    string whoHeader = "<ID>\t<nickname>\t<IP:port>\t<indicate me>\n";
    write(sockfd, whoHeader.c_str(), whoHeader.length());
    for (int i = 0; i < 30; i++)
    {
        struct User user = users[i];
        if (user.id != 0)
        {
            string userInfo = to_string(user.id) + "\t" + user.name + "\t" + user.ip + ":" + to_string(user.port);
            if (user.sockfd == sockfd)
                userInfo += "\t<-me\n";
            else
                userInfo += "\n";
            write(sockfd, userInfo.c_str(), userInfo.length());
        }
    }
}

/* built-in command: send user message */
void tell(char *cstr, string myUserName, int sockfd, struct User users[], map<int, struct User> fdUsers)
{
    char *pch;
    string id, message = "", tellMsg;
    strtok(cstr, " ");
    id = strtok(NULL, " ");
    pch = strtok(NULL, " ");
    while (pch != NULL)
    {
        message += pch;
        message += " ";
        pch = strtok(NULL, " ");
    }
    if (message.length() > 0)
        message.pop_back();
    
    bool isId = true;
    for (int i = 1; i < id.length(); i++)
    {
        if (!isdigit(id[i]))
            isId = false;
    }
    if (isId)
    {
        struct User user = users[stoi(id) - 1];
        if (user.id != 0)
        {
            tellMsg = "*** " + myUserName + " told you ***: " + message + "\n";
            write(user.sockfd, tellMsg.c_str(), tellMsg.length());
        }
        else
        {
            tellMsg = "*** Error: user #" + id + " does not exist yet. ***\n";
            write(sockfd, tellMsg.c_str(), tellMsg.length());
        }
    }
    else
    {
        for (auto &element : fdUsers)
        {
            struct User user = element.second;
            if (user.name == id)
            {
                tellMsg = "*** " + myUserName + " told you ***: " + message + "\n";
                write(user.sockfd, tellMsg.c_str(), tellMsg.length());
                break;
            }
        }
    }
    
    
    
}

/* Broadcast */
void broadcast(string Msg, map<int, struct User> fdUsers)
{
    for (auto &element : fdUsers)
    {
        write(element.first, Msg.c_str(), Msg.length());
    }
}

/* built-in command: Broadcast the message */
void yell(char *cstr, string myUserName, map<int, struct User> fdUsers)
{
    char *pch;
    string message = "", yellMsg;
    strtok(cstr, " ");
    pch = strtok(NULL, " ");
    while (pch != NULL)
    {
        message += pch;
        message += " ";
        pch = strtok(NULL, " ");
    }
    if (message.length() > 0)
        message.pop_back();
    yellMsg = "*** " + myUserName + " yelled ***: " + message + "\n";
    broadcast(yellMsg, fdUsers);
}

/* built-in command: change your name */
void name(char *cstr, int sockfd, struct User users[], map<int, struct User> &fdUsers)
{
    string name, nameMsg;
    bool haveSameName = false;
    strtok(cstr, " ");
    name = strtok(NULL, " ");
    for (auto &element : fdUsers)
    {
        if (element.second.name == name)
        {
            if (element.first != sockfd)
            {
                haveSameName = true;
            }
            break;
        }
    }
    if (!haveSameName)
    {
        users[fdUsers[sockfd].id-1].name = name;
        fdUsers[sockfd].name = name;
        nameMsg = "*** User from " + fdUsers[sockfd].ip + ":" + to_string(fdUsers[sockfd].port) + " is named '" + name + "'. ***\n";
        broadcast(nameMsg, fdUsers);
    }
    else
    {
        nameMsg = "*** User '" + name + "' already exists. ***\n";   
        write(sockfd, nameMsg.c_str(), nameMsg.length());
    } 
}

/* execute commands */
int executeCommand(char *cstr, int fd_in, int fd_out, int pipeline_count, int pipes_fd[][2], char sign, map<int, vector<struct number_pipe> > fdNpipes, struct number_pipe numberPipeIn, vector<struct user_pipe> upipes, string &userPipeCmd, int sockfd)
{
    int pid;
    char *argv[MAX_ARG_COUNT];
    ofstream file;
    string filename = "";
    bzero(argv, sizeof(argv));

    argv[0] = strtok(cstr, " \t\n\r");
    for (int j = 1; j <= MAX_ARG_COUNT; j++)
    {
        argv[j] = strtok(NULL, " \t\n\r");
        if (argv[j] == NULL)
        {
            break;
        }
        else if (strcmp(argv[j], ">") == 0)
        {
            argv[j] = NULL;
            filename = strtok(NULL, " \t\n\r");
        }
    }
    if (!filename.empty()) // write file
    {
        fd_out = open(filename.c_str(), O_RDWR | O_CREAT | O_TRUNC, S_IRWXU);
        pid = createProcess(argv, fd_in, fd_out, pipeline_count, pipes_fd, sign, fdNpipes, numberPipeIn, upipes, userPipeCmd, sockfd);
        close(fd_out);
    }
    else // normal command
    {
        pid = createProcess(argv, fd_in, fd_out, pipeline_count, pipes_fd, sign, fdNpipes, numberPipeIn, upipes, userPipeCmd, sockfd);
    }
    return pid;
}
/* wait/ waitpid Process */
void waitProcess(struct number_pipe &numberPipeOut, vector<int> &pids)
{
    int status;
    if (numberPipeOut.number != 0)
    {
        if (waitpid(-1, &status, WNOHANG) == 0)
            numberPipeOut.waitP++;
    }
    else
    {
        if (!pids.empty())
        {
            waitpid(pids.front(), &status, 0);
            pids.erase(pids.begin());
        }
        else
        {
            wait(&status);
        }
    }
}
/* Purpose: Create several child process and redirect the standard output
 * to the standard input of the later process.
 */
void dealWithCommandSeq(struct number_pipe numberPipeIn, vector<string> cmd, struct number_pipe &numberPipeOut,
                        struct user_pipe &userPipeIn, struct user_pipe &userPipeOut, map<int, map<string, string> > &clientenvs, 
                        vector<struct user_pipe> &upipes, map<int, vector<struct number_pipe> > fdNpipes, map<int, struct User> &fdUsers, 
                        struct User users[], string &userPipeCmd, int sockfd)
{
    vector<int> pids;
    int cmd_count = cmd.size();
    int pipeline_count = cmd_count;
    int pipes_fd[MAX_CMD_COUNT][2];
    bool userPipeError = false;
    string userPipeErrMsg;
    int upipesEraseIndex = 0;

    for (int P = 0; P < pipeline_count; P++)
    {
        if (P == MAX_CMD_COUNT)
            break;
        if (pipe(pipes_fd[P]) == -1)
        {
            fprintf(stderr, "Error: Unable to create pipe. (%d)\n", P);
            exit(EXIT_FAILURE);
        }
    }

    for (int C = 0; C < cmd_count; C++)
    {
        /* pipe limit avoid */
        if (C % (MAX_CMD_COUNT-1) == 0 && C != 0)
        {
            int ignoreP = (MAX_CMD_COUNT-1) - (C / (MAX_CMD_COUNT-1) - 1) % MAX_CMD_COUNT;
            for (int P = 0; P < MAX_CMD_COUNT; P++)
            {
                if (P == ignoreP)
                    continue;
                close(pipes_fd[P][0]);
                close(pipes_fd[P][1]);
            }
            for (int C = 0; C < (MAX_CMD_COUNT-1); C++)
            {
                waitProcess(numberPipeOut, pids);
            }
            for (int P = 0; P < MAX_CMD_COUNT; P++)
            {
                if (P == ignoreP)
                    continue;
                if (pipe(pipes_fd[P]) == -1)
                {
                    fprintf(stderr, "Error: Unable to create pipe. (%d)\n", P);
                    exit(EXIT_FAILURE);
                }
            }
        }
        /* redirect I/O */
        int fd_in = pipes_fd[C % MAX_CMD_COUNT][0];
        int fd_out = (C == cmd_count - 1) ? (sockfd) : (pipes_fd[(C + 1) % MAX_CMD_COUNT][1]);

        /* read number pipe and user pipe */
        if (C == 0)
        {
            int senderId = userPipeIn.senderId;
            if (numberPipeIn.number == 0)
                fd_in = numberPipeIn.npipe[0];
            if (senderId != 0)
            {
                if (senderId >= 1 && senderId <= 30)
                {
                    if (users[senderId-1].id == 0)
                    {
                        userPipeErrMsg = "*** Error: user #" + to_string(senderId) + " does not exist yet. ***\n";
                        write(sockfd, userPipeErrMsg.c_str(), userPipeErrMsg.length());
                        userPipeError = true;
                    }
                    else 
                    {
                        bool haveSender = false;
                        int receiverId = fdUsers[sockfd].id;
                        for (int i = 0; i < upipes.size(); i++)
                        {
                            if (upipes[i].receiverId == receiverId && upipes[i].senderId == senderId)
                            {
                                userPipeIn = upipes[i];
                                fd_in = userPipeIn.upipe[0];
                                upipesEraseIndex = i;
                                haveSender = true;
                                break;
                            }
                        }
                        if (!haveSender)
                        {
                            userPipeErrMsg = "*** Error: the pipe #" + to_string(senderId) + "->#" + to_string(receiverId) + " does not exist yet. ***\n";
                            write(sockfd, userPipeErrMsg.c_str(), userPipeErrMsg.length());
                            userPipeError = true;
                        }
                    }
                }
                else
                {
                    userPipeErrMsg = "*** Error: user #" + to_string(senderId) + " does not exist yet. ***\n";
                    write(sockfd, userPipeErrMsg.c_str(), userPipeErrMsg.length());
                    userPipeError = true;
                }
            }
        }
        /* write number pipe and user pipe */
        if (C == cmd_count - 1)
        {
            if (numberPipeOut.number != 0)
            {
                for (int i = 0; i < fdNpipes[sockfd].size(); i++)
                {
                    vector<struct number_pipe> npipes = fdNpipes[sockfd];
                    if (npipes[i].number == numberPipeOut.number)
                    {
                        numberPipeOut.number = -i - 1;
                        fd_out = npipes[i].npipe[1];
                        break;
                    }
                }
                if (numberPipeOut.number > 0)
                {
                    if (pipe(numberPipeOut.npipe) == -1)
                    {
                        fprintf(stderr, "Error: Unable to create pipe.\n");
                        exit(EXIT_FAILURE);
                    }
                    fd_out = numberPipeOut.npipe[1];
                }
            }
            int receiverId = userPipeOut.receiverId;
            if (receiverId != 0)
            {
                if (receiverId >= 1 && receiverId <= 30)
                {
                    if (users[receiverId-1].id == 0)
                    {
                        userPipeErrMsg = "*** Error: user #" + to_string(receiverId) + " does not exist yet. ***\n";
                        write(sockfd, userPipeErrMsg.c_str(), userPipeErrMsg.length());
                        userPipeError = true;
                        fd_out = STDOUT_FILENO;
                    }
                    else
                    {
                        int senderId = fdUsers[sockfd].id;
                        for (int i = 0; i < upipes.size(); i++)
                        {
                            if (upipes[i].senderId == senderId && upipes[i].receiverId == receiverId)
                            {
                                userPipeErrMsg = "*** Error: the pipe #" + to_string(senderId) + "->#" + to_string(receiverId) + " already exists. ***\n";
                                write(sockfd, userPipeErrMsg.c_str(), userPipeErrMsg.length());
                                userPipeError = true;
                                fd_out = STDOUT_FILENO;
                                break;
                            }
                        }
                        if (!userPipeError)
                        {
                            userPipeOut.senderId = senderId;
                            if (pipe(userPipeOut.upipe) == -1)
                            {
                                fprintf(stderr, "Error: Unable to create pipe.\n");
                                exit(EXIT_FAILURE);
                            }
                            fd_out = userPipeOut.upipe[1];
                            upipes.push_back(userPipeOut);
                        }
                    }
                }
                else
                {
                    userPipeErrMsg = "*** Error: user #" + to_string(receiverId) + " does not exist yet. ***\n";
                    write(sockfd, userPipeErrMsg.c_str(), userPipeErrMsg.length());
                    userPipeError = true;
                    fd_out = STDOUT_FILENO;
                }
            }
        }
        char *cstr = &cmd[C][0];
        /* built-in commands */
        if (strncmp(cstr, "setenv", 6) == 0)
        {
            setEnv(cstr, clientenvs[sockfd]);
        }
        else if (strncmp(cstr, "printenv", 8) == 0)
        {
            printEnv(cstr, sockfd);
        }
        else if (strcmp(cstr, "who") == 0)
        {
            who(sockfd, users);
        }
        else if (strncmp(cstr, "tell", 4) == 0)
        {
            tell(cstr, fdUsers[sockfd].name, sockfd, users, fdUsers);
        }
        else if (strncmp(cstr, "yell", 4) == 0)
        {
            yell(cstr, fdUsers[sockfd].name, fdUsers);
        }
        else if (strncmp(cstr, "name", 4) == 0)
        {
            name(cstr, sockfd, users, fdUsers);
        }
        else /* executable commands */
        {
            int pid = executeCommand(cstr, fd_in, fd_out, pipeline_count, pipes_fd, numberPipeOut.sign, fdNpipes, numberPipeIn, upipes, userPipeCmd, sockfd);
            if (numberPipeOut.number == 0)
            {
                pids.push_back(pid);
            }
        }
    }
    /* broadcast user pipe success message */
    string userPipeMsg;
    if (userPipeCmd != "")
    {
        if (!userPipeError)
        {
            if (userPipeIn.senderId != 0)
            {
                userPipeMsg = "*** " + users[userPipeIn.receiverId-1].name + " (#" + to_string(userPipeIn.receiverId) + ") just received from " + users[userPipeIn.senderId-1].name + " (#" + to_string(userPipeIn.senderId) + ") by '" + userPipeCmd + "' ***\n";  
                broadcast(userPipeMsg, fdUsers);
            }
            if (userPipeOut.receiverId != 0)
            {
                userPipeMsg = "*** " + users[userPipeOut.senderId-1].name + " (#" + to_string(userPipeOut.senderId) + ") just piped '" + userPipeCmd + "' to " + users[userPipeOut.receiverId-1].name + " (#" + to_string(userPipeOut.receiverId) + ") ***\n";
                broadcast(userPipeMsg, fdUsers);
            }
        }
    }
    /* close useless pipes and wait child processes */
    for (int P = 0; P < pipeline_count; P++)
    {
        if (P == MAX_CMD_COUNT)
            break;
        close(pipes_fd[P][0]);
        close(pipes_fd[P][1]);
    }
    if (numberPipeIn.number == 0)
    {
        close(numberPipeIn.npipe[0]);
        close(numberPipeIn.npipe[1]);
        for (int i = 0; i < numberPipeIn.waitP; i++) // wait finished number pipe related processes
        {
            waitProcess(numberPipeOut, pids);
        }
    }
    if (userPipeIn.senderId != 0 && !userPipeError)
    {
        close(userPipeIn.upipe[0]);
        close(userPipeIn.upipe[1]);
        upipes.erase(upipes.begin()+upipesEraseIndex);
    }
    for (int C = 0; C < cmd_count % (MAX_CMD_COUNT-1); C++)
    {
        waitProcess(numberPipeOut, pids);
    }
}

void checkNumberPipeIn(struct number_pipe &numberPipeIn, struct number_pipe numberPipeOut, vector<struct number_pipe> &npipes)
{
    numberPipeIn.number = -1;
    if (numberPipeOut.number > 0)
    {
        npipes.push_back(numberPipeOut);
    }
    else if (numberPipeOut.number < 0)
    {
        npipes[-(numberPipeOut.number + 1)].waitP += numberPipeOut.waitP;
    }
    for (int i = 0; i < npipes.size(); i++)
    {
        npipes[i].number--;
        if (npipes[i].number == 0)
        {
            numberPipeIn = npipes[i];
            npipes.erase(npipes.begin() + i);
            i--;
        }
    }
}
/* single process TCP server set up */
void TCPserver(int portno)
{
    struct User users[30];
    vector<struct user_pipe> upipes;
    map<int, struct User> fdUsers;
    map<int, vector<struct number_pipe> > fdNpipes;
    map<int, struct number_pipe> fdNumberPipeIns;
    map<int, map<string, string> > clientenvs;

    /* server related variables */
    int opt = 1;
    int client_socket[30], max_clients = 30, fd, maxfd;
    fd_set fdset;
    int sockfd, newsockfd;
    struct sockaddr_in serv_addr, cli_addr;
    socklen_t clilen;
    string welcomeMsg = "****************************************\n** Welcome to the information server. **\n****************************************\n";
    char buffer[15001];

    clearEnv();
    setenv("PATH", "bin:.", 1);

    /* initialize client_socket[] */
    for (int i = 0; i < max_clients; i++)
    {
        client_socket[i] = 0;
    }
    /* create TCP socket */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        error("ERROR opening socket");
    }
    cout << "TCP Socket successfully created...\n";

    /* set master socket to allow multiple connections */
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0)
    {
        error("setsockopt");
    }
    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    /* binding server addr structure to tcp sockfd */
    if (::bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        error("ERROR on binding");
    }
    cout << "TCP Socket successfully binded...\n";

    /* listening incoming connection */
    listen(sockfd, 30);
    cout << "Server listening...\n";
    clilen = sizeof(cli_addr);

    while (1)
    {
        FD_ZERO(&fdset);
        FD_SET(sockfd, &fdset);
        maxfd = sockfd;
        /* add child sockets to set */
        for (int i = 0; i < max_clients; i++)
        {
            fd = client_socket[i];
            if (fd > 0)
                FD_SET(fd, &fdset);
            if (fd > maxfd)
                maxfd = fd;
        }
        /* wait for an activity on one of the sockets */
        if (select(maxfd + 1, &fdset, NULL, NULL, NULL) < 0 && errno != EINTR)
        {
            error("select error");
        }
        /* If something happened on the master socket, it's an incoming connection */
        if (FD_ISSET(sockfd, &fdset))
        {
            string loginMsg;
            newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
            for (int i = 0; i < 30; i++)
            {
                if (users[i].id == 0)
                {
                    //vector<struct number_pipe> npipes;
                    struct number_pipe numberPipeIn;
                    numberPipeIn.number = -1;
                    users[i].id = i + 1;
                    users[i].ip = inet_ntoa(cli_addr.sin_addr);
                    users[i].port = ntohs(cli_addr.sin_port);
                    users[i].sockfd = newsockfd;
                    clientenvs[newsockfd]["PATH"] = "bin:.";
                    fdUsers[newsockfd] = users[i];
                    fdNpipes[newsockfd];
                    fdNumberPipeIns[newsockfd] = numberPipeIn;
                    loginMsg = "*** User '" + users[i].name + "' entered from " + users[i].ip + ":" + to_string(users[i].port) + ". ***\n";
                    break;
                }
            }
            write(newsockfd, welcomeMsg.c_str(), welcomeMsg.length());
            for (auto &element : fdUsers)
            {
                if (element.first != newsockfd)
                    write(element.first, loginMsg.c_str(), loginMsg.length());
                else
                    write(element.first, (loginMsg + "% ").c_str(), loginMsg.length() + 2);
            }
            // inform user of socket number - used in send and receive commands
            printf("New connection, socket fd is %d, ip is: %s, port: %d\n",
                   newsockfd, inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));

            // add new socket to array of sockets
            for (int i = 0; i < max_clients; i++)
            {
                if (client_socket[i] == 0)
                {
                    client_socket[i] = newsockfd;
                    break;
                }
            }
        }
        /* else some IO operation happened on other sockets */
        for (int i = 0; i < max_clients; i++)
        {
            fd = client_socket[i];
            if (FD_ISSET(fd, &fdset))
            {
                clearEnv();
                setClientEnv(clientenvs[fd]);
                bzero(buffer, 15001);
                string line = "", userPipeCmd = "";
                struct number_pipe numberPipeOut;
                struct user_pipe userPipeIn;
                struct user_pipe userPipeOut;

                read(fd, buffer, 15000);
                line = buffer;
                if (line[line.length()-1] == '\n')
                    line.pop_back();
                if (line[line.length()-1] == '\r')
                    line.pop_back();
                if (line == "exit")
                {
                    int userId = fdUsers[fd].id;
                    string logoutMsg = "*** User '" + fdUsers[fd].name + "' left. ***\n";
                    broadcast(logoutMsg, fdUsers);
                    for (int i = 0; i < upipes.size(); i++)
                    {
                        struct user_pipe userPipe = upipes[i];
                        if (userPipe.receiverId == userId)
                        {
                            close(userPipe.upipe[0]);
                            close(userPipe.upipe[1]);
                            upipes.erase(upipes.begin() + i);
                        }
                    }
                    users[userId - 1] = User();
                    clientenvs.erase(fd);
                    fdUsers.erase(fd);
                    fdNpipes.erase(fd);
                    fdNumberPipeIns.erase(fd);
                    close(fd);
                    client_socket[i] = 0;
                }
                else if (line != "")
                {
                    dealWithCommandSeq(fdNumberPipeIns[fd], readAndParseCommandSeq(line, numberPipeOut, userPipeIn, userPipeOut, userPipeCmd), numberPipeOut, userPipeIn, userPipeOut, clientenvs, upipes, fdNpipes, fdUsers, users, userPipeCmd, fd);
                    // check if the previous command lines need to output to next command line
                    checkNumberPipeIn(fdNumberPipeIns[fd], numberPipeOut, fdNpipes[fd]);
                    write(fd, "% ", 2);
                }
            }
        }
    }
    close(sockfd);
}
int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        error("ERROR, no port provided\n");
    }
    cout << "Port Is Available...\n";
    TCPserver(atoi(argv[1]));
}