/* do not care about the 
 * situation that pipe char <|>
 * is inside "" or '' because
 * sc_cli doesnt support 
 * this format arguments */
static int
handle_pipe_command(char* cmdbuf)
{
    char* ptr;
    int command_cnt;
    char** command;
    char** pcommand;
    const char* delim;
    char* resultbuf;
    size_t resultbuf_len;
    char* next_command;
    char* pipe_ch_pos;
    int pipe_ch_cnt;
    int retval;
    size_t len;
    size_t cmdlen;

    len = 0;
    cmdlen = strlen(cmdbuf);
    pipe_ch_cnt = 0;
    ptr = cmdbuf;

    if( strstr(ptr, "||") ){
        printf("%% Illegal pipe char.\n");
        return -EINVAL;
    }

    while( len < cmdlen ){
        pipe_ch_pos = strchr(ptr, '|');
        if( !pipe_ch_pos ){
            break;
        }
        ptr = pipe_ch_pos + 1;
    }
   
    if( pipe_ch_cnt == 0 ){
        return 0;
    }else if( pipe_ch_cnt > 10 ){
        printf("%% Too much pipe char.\n");
        return -EINVAL;
    }
     
    command_cnt = pipe_ch_cnt + 1;
    command = (char*)alloca(sizeof(char*) * command_cnt);
    memset(command, 0, sizeof(char*) * command_cnt);
    
    delim = "|";
    ptr = cmdbuf;
    pcommand = command;
    command_cnt = 0;
    while(ptr && (*pcommand = strsep(&ptr, delim))){
        if( strchr(**pcommand, delim) ){
            continue;
        }
        pcommand ++;
        command_cnt ++;
    }
    
    int prev_cmd_pipe_fd[2] = {-1, -1};
    int curr_cmd_pipe_fd[2] = {-1, -1};

    for(int i = 0 ; i < command_cnt ; i ++ ){
        if( pipe(pipe_fd) < 0 ){
            printf("%% failed to create pipe fd: %s\n", strerror(errno));
            return -errno;
        }
        
        /* get the command name, if it is grep, we deal with it */
        ptr = strdupa(command[i]);
        char char* cmd_name = strsep

        retval = fork();
        if( retval < 0 ){
            close(pipe_fd[0]);
            close(pipe_fd[1]);
            return -errno;
        }else if(retval == 0){
            close(STDOUT_FILENO);
            close(STDOUT_FILENO);
            close(STDERR_FILENO);
            dup2(STDIN_FILENO, pipe_fd[0]);
            dup2(STDOUT_FILENO, pipe_fd[1]);
            dup2(STDERR_FILENO, pipe_fd[1]);
            /* update command buffer */
            /* setup signal shot condition */
            /* system will release all resources */
            return 0;
        }else {
            resultbuf_len = 4096;
            resultbuf = malloc(resultbuf_len);
            if( !resultbuf ){
                retval = -errno;
                goto return_error;
            }
            while(1){
                while(0 > (retval=read(pipe_fd[0], resultbuf, resultbuf_len))
                        && errno == EINTR);
                if( retval == 0 ){
                    break;
                }
                if(retval == resultbuf_len){

                }
            }
         }

    }

    return 0;

return_error:
}
