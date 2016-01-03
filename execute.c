#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>

#include "execute.h"
#include "command.h"

static int run_program(Command *cmd);
static int execute_command_with_pipe(Command *cmd);
/** Builtin command handlers */
static int execute_builtin_cd(char *argv[]);
static int execute_builtin_exit(char *argv[]);

/**
 * Execute a Command (with or without pipes.)
 */
int execute_command(Command *cmd)
{

    // Check for builtin commands
    if (strcmp(cmd->argv[0], "cd") == 0) {
        return execute_builtin_cd(cmd->argv);
    }

    if (strcmp(cmd->argv[0], "exit") == 0) {
        return execute_builtin_exit(cmd->argv);
    }

    if (cmd->pipe_to == NULL) {

        /* TODO: Implement executing a simple command without pipes.
         *
         * Use `fork` to run the function `run_program` in a child process, and
         * wait for the child to terminate.
         */
        pid_t child = fork();
        if (child < 0 ){
            perror("fork");
            return -1;
        }
        if(child == 0){
            //child code
            run_program(cmd);
            exit(0);
        }else{
            //parent code for waiting
            int status;
            int mode = 0;
            if( waitpid(child, &status, mode) < 0){
                perror("waitpid");
                exit(EXIT_FAILURE);
            }
        }

        return 0;
    }
    else {
        return execute_command_with_pipe(cmd);
    }

    return EXIT_FAILURE;
}

/**
 * `cd` -- change directory (builtin command)
 *
 * Changes directory to a path specified in the `argv` array.
 *
 * Example:
 *      argv[0] = "cd"
 *      argv[1] = "csc209/a3"
 *
 * Your command MUST handle either paths relative to the current working
 * directory, and absolute paths relative to root (/).
 *
 * Example:
 *      relative path:      cd csc209/a3
 *      absolute path:      cd /u/csc209h/summer/
 *
 * Be careful and do not assume that any argument was necessarily passed at all
 * (in which case `argv[1]` would be NULL, because `argv` is a NULL terminated
 * array)
 */
static int execute_builtin_cd(char *argv[])
{
    char *arg = argv[1];
    int is_absolute = (arg && arg[0] == '/');
    if(arg == NULL){
        printf("ERROR: please provide a valid path\n");
        return -1;
    }
    if(is_absolute){
        int status;
        status = chdir(arg);
        return status;
    }else{ 
        long size;
        char *buf;
        char *current_working_directory;
        size = pathconf(".", _PC_PATH_MAX);
        if((buf = (char *)malloc((size_t)size)) != NULL){
            current_working_directory = getcwd(buf, (size_t) size);
        }else{
            perror("malloc");
            exit(EXIT_FAILURE);
        }
        strcat( current_working_directory, "/");
        strcat(current_working_directory, arg);
        int status;
        status = chdir(current_working_directory);
        free(buf);
        return status;

    }
    /*
     * TODO: Implement this function using `chdir(2)` and `getcwd(3)`.
     *
     * If the argument is absolute, you can pass it directly to chdir().
     *
     * If it is relative, you will first have to get the current working
     * directory, append the relative path to it and chdir() to the resulting
     * directory.
     *
     * This function should return whatever value chdir() returns.
     */

    return EXIT_FAILURE;
}

/**
 * `exit` -- exit the shell (builtin command)
 *
 * Terminate the shell process with status 0 using `exit(3)`. This function
 * should never return.
 *
 * You can optionally take an integer argument (a status) and exit with that
 * code.
 */
static int execute_builtin_exit(char *argv[])
{
    int status = 0;
    if(argv[0] != NULL){
        status = atoi(argv[0]);
    }
    exit(status);
    return EXIT_FAILURE;
}

/**
 * Execute the program specified by `cmd->argv`.
 *
 * Setup any requested redirections for STDOUT, STDIN and STDERR, and then use
 * `execvp` to execute the program.
 *
 * This function should return ONLY IF there is an failure while exec'ing the
 * program. This implies that this function should only be run in a forked
 * child process.
 */
static int run_program(Command *cmd)
{
    /** TODO: Setup redirections.
     *
     * For each non-NULL `in_filename`, `out_filename` and `err_filename` field
     * of `cmd`, perform the following steps:
     *
     *  - Use `open` to acquire a new file descriptor (make sure you have
     *      correct flags and permissions)
     *  - Use `dup2` to duplicate the newly opened file descriptors to the
     *      standard I/O file descriptors (use the symbolic constants
     *      STDOUT_FILENO, STDIN_FILENO and STDERR_FILENO.)
     *  - Use `close` to close the opened file, so as to not leave an open
     *      descriptor across an exec*() call
     */
    if(cmd->in_filename != NULL){
        int in;
        in = open(cmd->in_filename, O_RDONLY,0644);
        if(in < 0){
            perror("open");
            exit(EXIT_FAILURE);
        }
        if(dup2(in, STDIN_FILENO) < 0){
            perror("dup2");
            return EXIT_FAILURE;
        }
        if(close(in) < 0){
            perror("close");
            exit(EXIT_FAILURE);
        }

    }
    if(cmd->out_filename != NULL){
        int out;
        out = open(cmd->out_filename, O_WRONLY| O_CREAT | O_TRUNC,0644);
        if(out < 0){
            perror("open");
            exit(EXIT_FAILURE);
        }
        if(dup2(out, STDOUT_FILENO) < 0){
            perror("dup2");
            return EXIT_FAILURE;
        }
        if(close(out)<0){
            perror("close");
            exit(EXIT_FAILURE);
        }

    }
    if(cmd->err_filename != NULL){
        int err;
        err = open(cmd->err_filename, O_RDWR | O_APPEND, 0644);
        if(err < 0){
            perror("open");
            exit(EXIT_FAILURE);
        }
        if(dup2(err, STDERR_FILENO) < 0 ){
            perror("dup2");
            return EXIT_FAILURE;
        }
        if(close(err) < 0){
            perror("close");
            exit(EXIT_FAILURE);
        }
    }

    char *bin_name = cmd->argv[0];
    if(execvp(bin_name, cmd->argv)){
        fprintf(stderr,"incorrect spelling or doesn't exist: \"%s\" \n", bin_name);
        perror("");
        return -1;
    }

    /* TODO: Use `execvp` to replace current process with the command program
     *
     * In the case of an error, use `perror` to indicate the name of the
     * command that failed.
     */

    return EXIT_FAILURE;
}

/**
 * Execute (at least) two Commands connected by a pipe
 */
static int execute_command_with_pipe(Command *cmd)
{
    assert (cmd->pipe_to != NULL);

    /* TODO: Use `pipe(2)` to create a pair of connected file descriptors.
     *
     * These will be used to attach the STDOUT of one command to the STDIN of
     * the next.
     *
     * Be sure to check for and report any errors when creating the pipe.
     */

    int pipefds[2];
    if(pipe(pipefds) < 0){
        perror("pipe");
        return EXIT_FAILURE;
    }
    pid_t child1 = fork();
    if(child1 < 0 ){
        perror("fork");
        exit(1);
    }

    if(child1 == 0){
        //this is the child body
        if(close(pipefds[0])<0){
            perror("close");
            exit(1);
        } //closing the read end of the pipe
        if(close(STDOUT_FILENO)< 0){
            perror("close");
            exit(EXIT_FAILURE);
        }
        if(dup2(pipefds[1], STDOUT_FILENO)<0){
            perror("dup2");
            exit(1);
        }
        if(run_program(cmd)){
            exit( EXIT_FAILURE);
        }
        exit(0); //insert an error check maybe later on
    }else{
        //this is the body of the parent
        pid_t child2 = fork();
        if(child2 < 0 ){
            perror("fork");
            printf("child 2 failed to fork \n");
            return EXIT_FAILURE;
        }

        if(child2 == 0){
            //this is the body of child2
            if(close(pipefds[1]) < 0){
                perror("close");
                exit(EXIT_FAILURE);
            } //closing the write end
            if(close(STDIN_FILENO)<0){
                perror("close");
                exit(EXIT_FAILURE);
            }
            if(dup2(pipefds[0], STDIN_FILENO)<0){
                perror("dup2");
                exit(EXIT_FAILURE);
            }
            execute_command(cmd->pipe_to);
            exit(0);

        }else{
            //body of the parent
            if(close(pipefds[0])<0){
                perror("close");
                exit(EXIT_FAILURE);
            }
            if(close(pipefds[1])<0){
                perror("close");
                exit(EXIT_FAILURE);
            }
            int status;
            int status2;
            int mode = 0;
            if( waitpid(child1, &status, mode) < 0){
                perror("waitpid");
                exit(EXIT_FAILURE);
            }
            if( waitpid(child2, &status2, mode) < 0 ){
                perror("waitpid");
                exit(EXIT_FAILURE);
            }


        }


    }

    /* TODO: Fork a new process.
     *
     * In the child:
     *      - Close the read end of the pipe
     *      - Close the STDOUT descriptor
     *      - Connect STDOUT to the write end of the pipe (the one you didn't close)
     *      - Call `run_program` to run this command
     *
     *  In the parent:
     *      - Fork a second child process
     *      - In child 2:
     *          - Close the write end of the pipe
     *          - Close the STDIN descriptor
     *          - Connect STDIN to the read end of the pipe
     *          - Call `execute_command` to execute the `pipe_to` command
     *      - In the parent:
     *          - Close both ends of the pipe
     *          - Wait for both children to terminate.
     *
     * NOTE: This is a recursive approach. You may find it illuminating instead
     * to consider how you could implement the execution of the whole pipeline
     * in an interative style, using only a single parent, with one child per
     * command. If you wish, feel free to implement this instead of what is
     * outlined above.
     */

    return EXIT_SUCCESS;

}


