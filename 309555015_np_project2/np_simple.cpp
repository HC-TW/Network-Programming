#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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
void error(const char *msg)
{
    perror(msg);
    exit(1);
}
/* Load a command sequence from standard input. */
/* Push command to string vector */
vector<string> readAndParseCommandSeq(string line, struct number_pipe &numberPipeOut)
{
    string str_temp = "";
    string cmd_temp = "";
    vector<string> cmd;
    stringstream ss;

    ss.str(line);
    for (int i = 0; ss >> str_temp; i++) // extract string from line
    {
        if (str_temp != "|")
        {
            if (str_temp[0] != '|' && str_temp[0] != '!') // command sequence
            {
                cmd_temp += str_temp + " ";
            }
            else
            {
                if (str_temp[0] == '!') // number pipe
                {
                    numberPipeOut.sign = '!';
                }
                stringstream ss2(str_temp.substr(1));
                ss2 >> numberPipeOut.number;
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
        cmd_temp.pop_back(); // remove space from the end of string
        cmd.push_back(cmd_temp);
    }
    return cmd;
}

/* Purpose: Create child process and redirect io. */
int createProcess(char **argv, int fd_in, int fd_out, int pipeline_count, int pipes_fd[][2], char sign, vector<struct number_pipe> npipes, struct number_pipe numberPipeIn, int sockfd)
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
        for (int i = 0; i < npipes.size(); i++)
        {
            close(npipes[i].npipe[0]);
            close(npipes[i].npipe[1]);
        }
        if (numberPipeIn.number == 0)
        {
            close(numberPipeIn.npipe[0]);
            close(numberPipeIn.npipe[1]);
        }
        /* execute command */
        if (execvp(argv[0], argv) == -1)
        {
            fprintf(stdout, "Unknown command: [%s].\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }
    return pid;
}

/* set environment variable */
void setEnv(char *cstr)
{
    char *pch[2];
    strtok(cstr, " ");
    for (int i = 0; i < 2; i++)
    {
        pch[i] = strtok(NULL, " ");
    }
    setenv(pch[0], pch[1], 1);
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

/* print environment variable */
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

/* execute commands */
int executeCommand(char *cstr, int fd_in, int fd_out, int pipeline_count, int pipes_fd[][2], char sign, vector<struct number_pipe> npipes, struct number_pipe numberPipeIn, int sockfd)
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
        pid = createProcess(argv, fd_in, fd_out, pipeline_count, pipes_fd, sign, npipes, numberPipeIn, sockfd);
        close(fd_out);
    }
    else // normal command
    {
        pid = createProcess(argv, fd_in, fd_out, pipeline_count, pipes_fd, sign, npipes, numberPipeIn, sockfd);
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
void dealWithCommandSeq(struct number_pipe numberPipeIn, vector<string> cmd, struct number_pipe &numberPipeOut, vector<struct number_pipe> npipes, int sockfd)
{
    vector<int> pids;
    int cmd_count = cmd.size();
    int pipeline_count = cmd_count;
    int pipes_fd[MAX_CMD_COUNT][2];

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

        /* read number pipe */
        if (C == 0)
        {
            if (numberPipeIn.number == 0)
            {
                fd_in = numberPipeIn.npipe[0];
            }
        }
        /* write number pipe */
        if (C == cmd_count - 1)
        {
            if (numberPipeOut.number != 0)
            {
                for (int i = 0; i < npipes.size(); i++)
                {
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
        }
        char *cstr = &cmd[C][0];
        /* built-in commands */
        if (strncmp(cstr, "setenv", 6) == 0)
        {
            setEnv(cstr);
        }
        else if (strncmp(cstr, "printenv", 8) == 0)
        {
            printEnv(cstr, sockfd);
        }
        else /* executable commands */
        {
            int pid = executeCommand(cstr, fd_in, fd_out, pipeline_count, pipes_fd, numberPipeOut.sign, npipes, numberPipeIn, sockfd);
            if (numberPipeOut.number == 0)
            {
                pids.push_back(pid);
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
    vector<struct number_pipe> npipes;
    struct number_pipe numberPipeIn;
    numberPipeIn.number = -1;
    clearEnv();
    setenv("PATH", "bin:.", 1);

    int opt = 1;
    int sockfd, newsockfd;
    struct sockaddr_in serv_addr, cli_addr;
    socklen_t clilen;
    char buffer[15001];

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
    listen(sockfd, 5);
    cout << "Server listening...\n";
    clilen = sizeof(cli_addr);

    while (1)
    {
        newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
        // inform user of socket number - used in send and receive commands
        printf("New connection, socket fd is %d, ip is: %s, port : %d\n",
               newsockfd, inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
        while (1)
        {
            bzero(buffer, 15001);
            string line = "";
            struct number_pipe numberPipeOut;

            write(newsockfd, "% ", 2);
            read(newsockfd, buffer, 15000);
            line = buffer;
            if (line[line.length()-1] == '\n')
                line.pop_back();
            if (line[line.length()-1] == '\r')
                line.pop_back();
            if (line == "exit")
            {
                close(newsockfd);
                break;
            }
            if (line != "")
            {
                dealWithCommandSeq(numberPipeIn, readAndParseCommandSeq(line, numberPipeOut), numberPipeOut, npipes, newsockfd);
                // check if the previous command lines need to output to next command line
                checkNumberPipeIn(numberPipeIn, numberPipeOut, npipes);
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