#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

// #define LSH_BUFFER_SIZE 1200
#define LSH_TOK_BUFF 70
#define LSH_TOK_DELIM " \t\r\n\a "

// Function Declarations 
int lsh_cd(char **args);
int lsh_help(char **args);
int lsh_exit(char **args);

// List of builtin commands
char *builtin_str[] = {
    "cd", "help" , "exit"
};

int (*builtin_func[])(char **) = {
    &lsh_cd , &lsh_help , &lsh_exit 
};

int lsh_num_builtins(){
    return sizeof(builtin_str)/sizeof(char *);
}

// BuiltIn Function Implementations 
int lsh_cd(char **args){
    if(args[1] == NULL){
        fprintf(stderr, "lsh: Expected an argument to \"cd\"\n");
    }
    else {
        if (chdir(args[1]) != 0) {
            perror("lsh");
        }
    }
    return 1;
}

int lsh_help(char **args){
    printf("Rahul Singhs LSH\n");
    printf("Type program names and arguments and hit enter.\n");
    printf("The following are builtin commands:\n");
    for(int i = 0; i < lsh_num_builtins(); i++){
        printf(" %s\n", builtin_str[i]);
    }
    printf("Use the main command for informantin of other programs.\n");
    return 1;
}

int lsh_exit(char **args){
    return 0;
}

char *lsh_readLine(void){
    /*
    int buffSize = LSH_BUFFER_SIZE;
    int position = 0;
    char *buffer = malloc(sizeof(char) * buffSize);
    int c;
    if(!buffer){
        fprintf(stderr,"lsh: Failed to allocate space\n");
        exit(EXIT_FAILURE);
    }
    while(1){
        c = getchar();
        if(c == EOF||c == '\n'){
            buffer[position] = '\0';
            return buffer;
        }
        else{
            buffer[position] = c;
        }
        position++;
        if(position >= buffSize){
            buffSize += LSH_BUFFER_SIZE;
            buffer = realloc(buffer, buffSize);
            if(!buffer){
                fprintf(stderr,"lsh: Failed to allocate space\n");
                exit(EXIT_FAILURE);
            }
        }
    }
    This is the core implementation of how the read line wrks but since to ease our work we will be using getline() that does all these manual work automatically 
    */
    char *line = NULL;
    size_t bufferSize = 0;
    if(getline(&line, &bufferSize, stdin) == -1){
        if(feof(stdin)){
            exit(EXIT_SUCCESS);
        }
        else{
            perror("lsh: Failed to read line");
            exit(EXIT_FAILURE);
        }
    }
    return line;
}

char **lsh_split_line(char *line){
    int position = 0 , bufferSize = LSH_TOK_BUFF;
    char **tokens = malloc(bufferSize * sizeof(char*));
    char *token;
    if(!tokens){
        fprintf(stderr , "lsh:Cannot allocate Tokenizer Array.");
        exit(EXIT_FAILURE);
    }
    token = strtok(line , LSH_TOK_DELIM);
    while(token != NULL){
        tokens[position] = token;
        position++;
        if(position >= bufferSize){
            bufferSize += bufferSize;
            tokens = realloc(tokens, bufferSize * sizeof(char*));
            if(!tokens){
                fprintf(stderr , "lsh:Cannot allocate Tokenizer Array");
                exit(EXIT_FAILURE);
            }
        }
        token = strtok(NULL , LSH_TOK_DELIM);
    }
    tokens[position] = NULL;
    return tokens;
}

int lsh_launch(char **args){
    pid_t pid , wpid;
    int status;
    pid = fork();
    if(pid == 0){
        // Child Process
        if(execvp(args[0], args) == -1){
            perror("lsh");
        }
        exit(EXIT_FAILURE);
    }
    else if(pid < 0){
        perror("lsh: Couldnt Fork");
    }
    else{
        do{
            wpid = waitpid(pid, &status, WUNTRACED);
        }while(!WIFEXITED(status) && !WIFSIGNALED(status));
    }
    return 1;
}

int lsh_execute(char **args){
    if(args[0] == NULL){
        return 1;
    }
    for(int i = 0; i < lsh_num_builtins(); i++){
        if(strcmp(args[0], builtin_str[i]) == 0){
            return (*builtin_func[i])(args);
        }
    }
    return lsh_launch(args);
}

void lsh_loop(void){
    char *line;
    char **args;
    int status;
    do{
        printf("> ");
        line = lsh_readLine();
        args = lsh_split_line(line);
        status = lsh_execute(args);
        free(line);
        free(args);
    } while(status);
}

int main(int argc, char **argv) {
    // Loop until the user enters "exit"
    lsh_loop();
    return EXIT_SUCCESS;
}