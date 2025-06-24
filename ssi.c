#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <limits.h>
#include <signal.h>
#include <errno.h>
#include <ctype.h>

#define MAX_INPUT_LINE 1024
#define MAX_COMMANDS 1024

typedef struct Node {
    char program[MAX_INPUT_LINE];
    char arguments[MAX_INPUT_LINE];
    pid_t pid;
    struct Node *next;
} Node;

// create a new node with a copy of the string
Node *createNode(const char *program, const char *arguments, int pid) {
    Node *node = malloc(sizeof(Node));

    node->pid = pid;

    strcpy(node->program, program);
    strcpy(node->arguments, arguments);
    node->next = NULL;
    return node;
}

void addNode(Node **head, Node *new_node) {
    if (!*head) {
        *head = new_node;
        return;
    }
    Node *current = *head;
    while (current->next) {
        current = current->next;
    }
    current->next = new_node;
}

int checkIfTerminated(Node **head, int numNodes){
    if(numNodes > 0){
        Node *current = *head;
        Node *prev = NULL;

        while (current) {
            int status;
            pid_t time = waitpid(current->pid, &status, WNOHANG);
            if (time == -1 && errno != ECHILD) {exit(1);}    

            if(WEXITSTATUS(status) == 1){

                Node *temp = current;
                
                if (prev == NULL) { // head
                    *head = current->next;
                    current = *head;
                } else { // not head
                    prev->next = current->next;
                    current = prev->next;
                }
                
                free(temp);
                
                numNodes--;
                continue;

            }

            else if (time == current->pid) { // finished
                printf("%d: %s %s has terminated.\n", current->pid, current->program, current->arguments);
                
                Node *temp = current;
                
                if (prev == NULL) { // head
                    *head = current->next;
                    current = *head;
                } else { // not head
                    prev->next = current->next;
                    current = prev->next;
                }
                
                free(temp);
                
                numNodes--;
                continue;
            }
            prev = current;
            current = current->next;
        }
    }
    return numNodes;
}

void printList(Node *head, int num_nodes) {
    if (num_nodes > 0){
        Node *current = head;
        while (current){
            printf("%d: %s %s\n", current->pid, current->program, current->arguments);
            current = current->next;
        }
        printf("Total Background jobs: %d\n", num_nodes);
        return;
    }
    return;
}

void getPrompt() {
    char cwd[MAX_INPUT_LINE];
    getcwd(cwd, MAX_INPUT_LINE);
    char hostname[MAX_INPUT_LINE];
    gethostname(hostname, MAX_INPUT_LINE);
    printf("%s@%s: %s > ", getlogin(), hostname, cwd);
}

void parse_command(char *cmd, char **argv) {
    int argc = 0;
    char *tok = strtok(cmd, " \t");
    while (tok) {
        argv[argc++] = tok;
        tok = strtok(NULL, " \t");
    }
    argv[argc] = NULL;
}

void remove_char_at_position(char *str, int position) {
    int length = strlen(str);
    if (position >= 0 && position < length) {
        for (int i = position; i < length - 1; i++) {
            str[i] = str[i + 1];
        }
        str[length - 1] = '\0';
    }
}

int is_empty(const char *s){
    while(*s != '\0'){
        if(!isspace((unsigned char)*s))
            return 0;
        s++;
    }
    return 1;
}

int main() {
    Node *head = NULL;
    int numNodes = 0;

    while (1) {
        char command[MAX_INPUT_LINE];
        int isBackgroundProcess = 0;

        getPrompt();

        // ignore SIGINT to avoid closing the whole shell
        signal(SIGINT, SIG_IGN);  

        if (!fgets(command, sizeof(command), stdin)) break;

        
        
        

        numNodes = checkIfTerminated(&head, numNodes);

        if (is_empty(command)) { continue; }
            
        command[strcspn(command, "\n")] = 0; 

        if (command[0] == 'b' && command[1] == 'g' && command[2] == ' ') {
            isBackgroundProcess = 1;
            remove_char_at_position(command, 0);
            remove_char_at_position(command, 0);
            remove_char_at_position(command, 0);
        }
        
        int num_cmds = 0;
        char *cmds[256];
        char *segment = strtok(command, "|");
        while (segment) {
            cmds[num_cmds++] = segment;
            segment = strtok(NULL, "|");
        }
        
        pid_t pids[256];
        int prev_pipe = -1;
        int pipefd[2];
        
        for (int i = 0; i < num_cmds; i++) {
            int argc = 0;
            char *copy = strdup(cmds[i]);
            strtok(copy, " \t");
            while (strtok(NULL, " \t")) argc++;
            argc++;
            free(copy);
            char *originalArgs = strdup(cmds[i]);
            char *args[argc + 1];
            parse_command(cmds[i], args);
            args[argc] = NULL;

            int first_length = strlen(args[0]);
            for(int j = 0; j < first_length + 1; j++) {
                remove_char_at_position(originalArgs, 0);
            }

            //cd
            //cd ..
            //cd ~
            //cd ~/
            //cd ~//
            //cd ~CSC360
            //cd ~/CSC360
        
            if (strcmp(args[0], "cd") == 0) {
                const char *home_dir = getenv("HOME");

                if (argc == 1 || (argc >= 2 && args[1][0] == '~' && args[1][1] == '\0') || (argc >= 2 && args[1][0] == '~' && args[1][1] == '/' && args[1][2] == '\0')) { //nothing follows the tilda, or ~/
                    if (chdir(home_dir) == -1) {fprintf(stderr, "%s: Not a directory.\n", home_dir);} 
                    continue;
                }else if(argc >= 2 && args[1][0] == '~' && args[1][1] != '/'){
                    fprintf(stderr, "%s: Not a directory.\n", args[1]); 
                    continue;
                }else if(argc >= 2 && args[1][0] == '~' && args[1][1] == '/' && args[1][2] != '\0'){
                    remove_char_at_position(args[1], 0); //remove ~
                    remove_char_at_position(args[1], 0); //remove /
                    if (chdir(home_dir) == -1) {fprintf(stderr, "%s: Not a directory.\n", home_dir);} //go to home directory
                }
                if (chdir(args[1]) == -1) {fprintf(stderr, "%s: Not a directory.\n", args[1]); }
                continue;
            } else if (strcmp(args[0], "bglist") == 0) {
                printList(head, numNodes);
                continue;
            } else if (strcmp(args[0], "exit") == 0) {
                exit(0);
            }
            

            int lastCommand = (i == num_cmds - 1);

            if (!lastCommand) {
                if (pipe(pipefd) == -1) {exit(1);}  
            }
            

            pid_t pid = fork();
            if (pid < 0) { // exit if fork fails
                return 1;
            }
            

            if (isBackgroundProcess) {

                if (pid == 0) { // child
                    int devnull = open("/dev/null", O_WRONLY);
                    if (devnull == -1) {exit(1);} 
                    dup2(devnull, STDOUT_FILENO);
                    close(devnull);
                    execvp(args[0], args);
                    fprintf(stderr, "%s: No such file or directory.\n", args[0]);
                    exit(1);
                } else if (pid > 0) { // parent

                    char cwd[MAX_INPUT_LINE];
                    getcwd(cwd, MAX_INPUT_LINE);

                    char buffer[MAX_INPUT_LINE*3];
                    snprintf(buffer, sizeof(buffer), "%s/%s", cwd, args[0]);

                    Node *new_node = createNode(buffer, originalArgs, pid);
                    addNode(&head, new_node);
                    numNodes++;
                } else {exit(EXIT_FAILURE);}                

            } else {

                if (pid == 0) {

                    signal(SIGINT, SIG_DFL);

                    if (prev_pipe != -1) {
                        dup2(prev_pipe, STDIN_FILENO);
                        close(prev_pipe);
                    }
                    if (!lastCommand) {
                        dup2(pipefd[1], STDOUT_FILENO);
                        close(pipefd[0]);
                        close(pipefd[1]);
                    }
                    if (execvp(args[0], args) == -1) {
                        if (errno == ENOENT) {
                            printf("%s: No such file or directory.\n", args[0]);
                        } else {
                            perror("execvp");
                        }
                        exit(1);
                    }
                    exit(1);
                }
                
                pids[i] = pid;

                if (prev_pipe != -1) close(prev_pipe);
                
                if (!lastCommand) {
                    close(pipefd[1]);
                    prev_pipe = pipefd[0];
                }
            }
        }
        
    
        for (int i = 0; i < num_cmds; i++) {
            if (!isBackgroundProcess) {
                if (waitpid(pids[i], NULL, 0) == -1 && errno != ECHILD) {exit(1);} 
            }
        }
    }
    return 0;
}