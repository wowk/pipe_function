#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <alloca.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>


/* do not care about the 
 * situation that pipe char <|>
 * is inside "" or '' because
 * sc_cli doesnt support 
 * this format arguments */

static char*
strtrim(char* s)
{
    char* pstart = s;
    char* pend = s + strlen(s) - 1;

    while(isspace(*pstart)){
        *pstart = 0;
        pstart ++;
    }

    while(isspace(*pend)){
        *pend = '\0';
        pend --;
    }

    return pstart;
}

static ssize_t
readall(int fd, char** buffer)
{
    size_t size;
    size_t total_size;
    size_t bufsize = PIPE_BUF;

    total_size = 0;
    *buffer = (char*)malloc(bufsize);
    if( !(*buffer) ){
        printf("failed to malloc buffer: %s\n", strerror(errno));
        return -errno;
    }

    while(1){
        while(0 > (size=read(fd, total_size + *buffer, bufsize - total_size)) && errno == EINTR);
        if( size < 0 ){
            free(*buffer);
            printf("failed to read data from pipe: %s\n", strerror(errno));
            return -errno;
        }
        else if( size < bufsize - total_size){
            printf("got final data: %d\n", size);
            total_size += size;
            break;
        }
        else{
            printf("got data\n");
            total_size += size;
            bufsize <<= 1;
            *buffer = (char*)realloc(*buffer, bufsize);
        }
    }

    return total_size;
}

static int inline
isquotation(char ch)
{
    return (ch == '\'' || ch== '\"');
}

static int
parse_arg(char* pstr, char* argv[], size_t max_argv_cnt)
{
    char left_quotation = 0;
    char* ptr = pstr;
    size_t argv_cnt = 0;

    while(argv_cnt < max_argv_cnt && *ptr){
        if(isspace(*ptr)){
            *ptr ++ = 0;
            continue;
        }
        if(isquotation(*ptr)){
            left_quotation = *ptr;
            *ptr++ = 0;
            argv[argv_cnt++] = ptr;
            while(*ptr && (*ptr != left_quotation || *(ptr-1) == '\\')){
                ptr ++;
            }
            if( *ptr == 0 ){
                return -1;
            }else if(*(ptr+1) == 0){
                *ptr = 0;
                return argv_cnt;
            }else if(isspace(*(ptr+1))){
                *ptr = 0;
                ptr++;
            }else{
                return -1;
            }
        }
        else{
            argv[argv_cnt++] = ptr++;
            while(*ptr && !isspace(*ptr)){
                if(isquotation(*ptr)){
                    if(*(ptr-1) != '\\'){
                        return -1;
                    }
                }else if(isspace(*ptr)){
                    *ptr ++ = 0;
                    break;
                }
                ptr ++;
            }
        }
    }

    return argv_cnt;
}

static int
handle_pipe_command(char* cmds, char* cmd, size_t cmd_len, bool* sub_process)
{
    char* ptr;
    char* token;
    int command_cnt;
    char** command;
    char** pcommand;
    const char* delim;
    char* resultbuf;
    ssize_t resultbuf_len;
    char* pipe_ch_pos;
    int pipe_ch_cnt;
    int retval;
    int pipe_fd[2];

    ptr = cmds;
    if( strstr(ptr, "||") ){
        printf("%% Illegal pipe char.\n");
        return -EINVAL;
    }

    /* count the number of pipe chars */
    pipe_ch_cnt = 0;
    while(1){
        pipe_ch_pos = strchr(ptr, '|');
        if( !pipe_ch_pos ){
            break;
        }
        ptr = pipe_ch_pos + 1;
        pipe_ch_cnt ++;
    }
   
    /* if no pipe char found, just return */
    if( pipe_ch_cnt == 0 ){
        printf("no pipe char found, just go the normal way\n");
        return 0;
    }
    /* if more than 10 pipe char found, complain it */
    else if( pipe_ch_cnt > 10 ){
        printf("%% Too much pipe char.\n");
        return -EINVAL;
    }
     
    /* extract commands from the buffer and store it in array <command>*/
    command_cnt = pipe_ch_cnt + 1;
    command = (char**)alloca(sizeof(char*) * command_cnt);
    memset(command, 0, sizeof(char*) * command_cnt);
    
    delim = "|";
    token = cmds;
    pcommand = command;
    command_cnt = 0;
    while(token && (*pcommand = strsep(&token, delim))){
        if( strchr(delim, **pcommand) ){
            continue;
        }
        printf("command: %s$\n", *pcommand);
        *pcommand = strtrim(*pcommand);
        pcommand ++;
        command_cnt ++;
    }

    resultbuf = NULL;
    resultbuf_len = 0;
    pipe_fd[0] = pipe_fd[1] = -1;
    for(int i = 0 ; i < command_cnt ; i ++ ){
        printf("exec commmand: %s\n", command[i]);
        if( pipe2(pipe_fd, O_DIRECT) < 0 ){
            printf("%% failed to create pipe fd: %s\n", strerror(errno));
            retval = -errno;
            goto return_error;
        }
        
        retval = fork();
        if( retval < 0 ){
            retval = -errno;
            goto return_error;
        }else if(retval == 0){
            dup2(pipe_fd[0], STDIN_FILENO);
            dup2(pipe_fd[1], STDOUT_FILENO);
            //dup2(pipe_fd[1], STDERR_FILENO);
            
            //char buf[1025] = "";
            //buf[1024] = 0;
            //ssize_t size = read(STDIN_FILENO, buf, 1024);
            //buf[size] = 0;
            
            /* only support <grep> currently */
            if(!strncmp(command[i], "grep ", 5) ){
                char* argv[33];
                char* file = "/bin/grep";
                fprintf(stderr, "child got command: %s\n", command[i]);
                int arg_cnt = parse_arg(command[i], argv,32);
                if( arg_cnt < 0 ){
                    exit(-1);
                }
                argv[arg_cnt] = 0; 
                if(0 > execvp(file, &argv[1])){
                    fprintf(stderr, "failed to execvp <%s>: %s\n", file, strerror(errno));
                    exit(-errno);
                }
                /* execvp never return except error happend*/
            }else{
                fprintf(stderr, "child got command: %s\n", command[i]);
                snprintf(cmd, cmd_len, "%s", command[i]);
                *sub_process = true;
            }
            
            return 0;
        }else {
            int wstatus;
            pid_t pid = (pid_t)retval;
            if( resultbuf_len > 0 ){
                printf("write data: <%s> to child\n", resultbuf);
                while((0 > (retval=write(pipe_fd[1], resultbuf, resultbuf_len))) && errno == EINTR);
                printf("write done\n");
                if( retval < 0 ){
                    printf("failed to write data to pipe: %s\n", strerror(errno));
                    goto return_error;
                }
                free(resultbuf);
                resultbuf = NULL;
                resultbuf_len = 0;
            }
            close(pipe_fd[1]);
            pipe_fd[1] = -1;

            waitpid(pid, &wstatus, 0);
            wstatus = WEXITSTATUS(wstatus);
            resultbuf_len = readall(pipe_fd[0], &resultbuf);
            resultbuf[resultbuf_len] = 0;
            if(0 > resultbuf_len){
                retval = -errno;
                goto return_error;
            }
            printf("read: %s\n", resultbuf);
            close(pipe_fd[0]);
         }
    }

    printf("%s\n", resultbuf);
    free(resultbuf);

    return 0;

return_error:
    if(resultbuf){
        free(resultbuf);
    }
    if( pipe_fd[0] > 0){
        close(pipe_fd[0]);
        close(pipe_fd[1]);
    }

    return 0;
}


int main(int argc, char** argv)
{
    char cmd[128];
    bool sub_process = false;
    handle_pipe_command(argv[1], cmd, sizeof(cmd), &sub_process);

    if( sub_process ){
        system(cmd);
    }

    return 0;
}
