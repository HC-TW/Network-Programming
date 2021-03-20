#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_ARG_COUNT (15)
#define MAX_CMD_COUNT (500)

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
int createProcess(char **argv, int fd_in, int fd_out, int pipeline_count, int pipes_fd[][2], char sign, vector<struct number_pipe> npipes, struct number_pipe numberPipeIn)
{
    pid_t pid;
    while ((pid = fork()) < 0)
    {
        int status;
        wait(&status);
        /* fprintf(stderr, "Error: Unable to fork.\n");
        exit(EXIT_FAILURE); */
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
        /* close useless pipe */
        for (int P = 0; P < pipeline_count; P++)
        {
            if (P == 500)
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
            fprintf(stderr, "Unknown command: [%s].\n", argv[0]);
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

/* unset environment variable */
void unsetEnv(char *cstr)
{
    char *pch[2];
    strtok(cstr, " ");
    for (int i = 0; i < 2; i++)
    {
        pch[i] = strtok(NULL, " ");
    }
    unsetenv(pch[0]);
}

/* print environment variable */
void printEnv(char *cstr)
{
    char *pch;
    strtok(cstr, " ");
    pch = strtok(NULL, " ");
    if (getenv(pch) != NULL)
    {
        cout << getenv(pch) << endl;
    }
}

/* execute commands */
int executeCommand(char *cstr, int fd_in, int fd_out, int pipeline_count, int pipes_fd[][2], char sign, vector<struct number_pipe> npipes, struct number_pipe numberPipeIn)
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
        pid = createProcess(argv, fd_in, fd_out, pipeline_count, pipes_fd, sign, npipes, numberPipeIn);
        close(fd_out);
    }
    else // normal command
    {
        pid = createProcess(argv, fd_in, fd_out, pipeline_count, pipes_fd, sign, npipes, numberPipeIn);
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
void dealWithCommandSeq(struct number_pipe numberPipeIn, vector<string> cmd, struct number_pipe &numberPipeOut, vector<struct number_pipe> npipes)
{
    vector<int> pids;
    int cmd_count = cmd.size();
    int pipeline_count = cmd_count;
    int pipes_fd[MAX_CMD_COUNT][2];

    for (int P = 0; P < pipeline_count; P++)
    {
        if (P == 500)
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
        if (C % 499 == 0 && C != 0)
        {
            int ignoreP = 499 - (C / 499 - 1) % 500;
            for (int P = 0; P < MAX_CMD_COUNT; P++)
            {
                if (P == ignoreP)
                    continue;
                close(pipes_fd[P][0]);
                close(pipes_fd[P][1]);
            }
            for (int C = 0; C < 499; C++)
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
        int fd_in = pipes_fd[C % 500][0];
        int fd_out = (C == cmd_count - 1) ? (STDOUT_FILENO) : (pipes_fd[(C + 1) % 500][1]);

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
        else if (strncmp(cstr, "unsetenv", 8) == 0)
        {
            unsetEnv(cstr);
        }
        else if (strncmp(cstr, "printenv", 8) == 0)
        {
            printEnv(cstr);
        }
        else if (strcmp(cstr, "exit") == 0)
        {
            exit(EXIT_SUCCESS);
        }
        else /* executable commands */
        {
            int pid = executeCommand(cstr, fd_in, fd_out, pipeline_count, pipes_fd, numberPipeOut.sign, npipes, numberPipeIn);
            if (numberPipeOut.number == 0)
            {
                pids.push_back(pid);
            }
        }
    }
    /* close useless pipes and wait child processes */
    for (int P = 0; P < pipeline_count; P++)
    {
        if (P == 500)
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
    for (int C = 0; C < cmd_count % 499; C++)
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

int main()
{
    vector<struct number_pipe> npipes;
    struct number_pipe numberPipeIn;
    numberPipeIn.number = -1;
    setenv("PATH", "bin:.", 1);

    while (1)
    {
        string line = "";
        struct number_pipe numberPipeOut;

        cout << "% ";
        getline(cin, line);
        if (cin.eof())
        {
            break;
        }
        if (line != "")
        {
            dealWithCommandSeq(numberPipeIn, readAndParseCommandSeq(line, numberPipeOut), numberPipeOut, npipes);
            // check if the previous command lines need to output to next command line
            checkNumberPipeIn(numberPipeIn, numberPipeOut, npipes);
        }
    }
}