#include <errno.h>
#include <fcntl.h>
#include <glob.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

typedef struct {
    char** tokens;
    int num_tokens;
} Command;

char* dup_string(const char* src) {
    size_t len = strlen(src) + 1;
    char* dest = malloc(len);
    if (dest == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    memcpy(dest, src, len);
    return dest;
}

Command expand_wildcards(Command command) {
    bool has_wildcard = false;
    for (int i = 0; i < command.num_tokens; i++) {
        if (strchr(command.tokens[i], '*') != NULL) {
            has_wildcard = true;
            break;
        }
    }

    if (!has_wildcard) {
        return command;
    }

    Command expanded_command;
    expanded_command.tokens = NULL;
    expanded_command.num_tokens = 0;

    for (int i = 0; i < command.num_tokens; i++) {
        char* token = command.tokens[i];
        if (strchr(token, '*') != NULL) {
            glob_t globbuf;
            glob(token, 0, NULL, &globbuf);

            int new_tokens_count = expanded_command.num_tokens + globbuf.gl_pathc;
            expanded_command.tokens = realloc(expanded_command.tokens, (new_tokens_count + 1) * sizeof(char*));

            for (size_t j = 0; j < globbuf.gl_pathc; j++) {
                expanded_command.tokens[expanded_command.num_tokens++] = dup_string(globbuf.gl_pathv[j]);
            }

            globfree(&globbuf);
        } else {
            expanded_command.tokens = realloc(expanded_command.tokens, (expanded_command.num_tokens + 1) * sizeof(char*));
            expanded_command.tokens[expanded_command.num_tokens++] = dup_string(token);
        }
    }

    expanded_command.tokens[expanded_command.num_tokens] = NULL;
    return expanded_command;
}

void free_expanded_command(Command* command) {
    for (int i = 0; i < command->num_tokens; i++) {
        free(command->tokens[i]);
    }
    free(command->tokens);
}

int is_built_in_command(char* command) {
    if (strcmp(command, "cd") == 0) {
        return true;
    } else if (strcmp(command, "pwd") == 0) {
        return true;
    } else if (strcmp(command, "echo") == 0) {
        return true;
    }

    return false;
}

int is_path(char* command) {
    if (command[0] == '/') {
        return true;
    }

    return false;
}

char* search_for_file_from_base_dirs(char* file_name, struct stat** file_status) {
    char built_path[512];
    char* result = NULL;

    char* paths[] = {"/usr/local/sbin", "/usr/local/bin", "/usr/sbin",
                     "/usr/bin", "/sbin", "/bin"};

    int stat_status = 0;
    for (int i = 0; i < 6; i++) {
        memset((void*)built_path, '\0', sizeof(built_path));
        strcat(built_path, paths[i]);
        strcat(built_path, "/");
        strcat(built_path, file_name);

        // stat(): On success, zero is returned.  On error, -1 is returned, and
        // errno is set appropriately.
        // ** First checks if the file exists, if it does then it will get data
        // from **file_status. Then it will check the
        // ** st_ino attribute to see if the file is executable. If executable,
        // return path else empty string
        if ((stat_status = stat(built_path, *file_status)) == 0) {
            if (access(built_path, X_OK) == 0) {
                // file can be executed
                result = (char*)malloc(strlen(built_path) + 1);
                strcpy(result, built_path);
                return result;
            }
        }
    }
    if (stat_status == -1) {
        perror("stat: Error searching for file");
    }

    // use stat() to check for the existence of a file, rather than traversing
    // the directory itself
    //** return empty string, malloc for the purpose to store on the heap.
    result = (char*)malloc(1);
    result[0] = '\0';
    return result;
}

void remove_token(Command* cmd, int index) {
    if (index < 0 || index >= cmd->num_tokens) {
        // invalid index, do nothing
        return;
    }
    // printf("Attempting to remove %s: \n", cmd->tokens[index]);
    // printf("Attempting to free at index: %d\n", index);

    // free memory of token to be removed
    free(cmd->tokens[index]);

    // shift remaining tokens to the left
    for (int i = index; i < cmd->num_tokens - 1; i++) {
        cmd->tokens[i] = cmd->tokens[i + 1];
    }

    // resize tokens array and add NULL terminator
    cmd->num_tokens--;
    cmd->tokens = (char**)realloc(cmd->tokens, (cmd->num_tokens + 1) * sizeof(char*));
    cmd->tokens[cmd->num_tokens] = NULL;
}

void print_cwd() {
    char cwd[1024];

    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("%s\n", cwd);
    } else {
        perror("getcwd() error");
    }
}

const char* get_cwd() {
    char* cwd = malloc(sizeof(char) * 1024);

    const char* path = getcwd(cwd, sizeof(cwd));
    printf("%s", path);

    return path;
}

Command parse_tokens(char* command_str) {
    Command command;
    command.tokens = NULL;
    command.num_tokens = 0;
    int token_capacity = 10;

    // Allocate memory for tokens
    command.tokens = (char**)malloc(token_capacity * sizeof(char*));

    // Split command_str into tokens
    char* token_start = command_str;
    char* token_end;

    while (1) {
        // Skip leading spaces or tabs
        while (*token_start == ' ' || *token_start == '\t') {
            token_start++;
        }

        if (*token_start == '\0') {
            break;
        }

        token_end = strpbrk(token_start, " \t");

        if (token_end == NULL) {
            // Allocate memory and copy the last token content
            command.tokens[command.num_tokens] = (char*)malloc(strlen(token_start) + 1);
            strcpy(command.tokens[command.num_tokens], token_start);
            command.num_tokens++;
            break;
        } else {
            *token_end = '\0';
            // Allocate memory and copy token content
            command.tokens[command.num_tokens] = (char*)malloc(strlen(token_start) + 1);
            strcpy(command.tokens[command.num_tokens], token_start);
            command.num_tokens++;
            token_start = token_end + 1;
        }
    }

    // Allocate extra memory for NULL terminator
    command.tokens = (char**)realloc(command.tokens, (command.num_tokens + 1) * sizeof(char*));

    // Add NULL terminator to array
    command.tokens[command.num_tokens] = NULL;

    return command;
}

Command* parse_commands(int* num_commands) {
    char command_str[1024];

    // Prompt for input
    write(STDOUT_FILENO, "mysh> ", 6);

    // Read input from stdin
    ssize_t read_len = read(STDIN_FILENO, command_str, 1024);

    if (read_len == -1) {
        perror("read");
        return NULL;
    } else if (read_len == 0) {
        // Reached end of file
        fprintf(stderr, "Error: Reached end of file.\n");
        return NULL;
    } else {
        // Remove trailing newline character
        if (command_str[read_len - 1] == '\n') {
            command_str[read_len - 1] = '\0';
            read_len--;
        }

        Command* commands = NULL;
        *num_commands = 0;
        int command_capacity = 10;

        // Allocate memory for commands
        commands = (Command*)malloc(command_capacity * sizeof(Command));

        // Split input into commands separated by "|"
        char* command_start = command_str;
        char* command_end;
        while ((command_end = strchr(command_start, '|')) != NULL) {
            *command_end = '\0';

            // Parse command into tokens and store it in the commands array
            commands[*num_commands] = parse_tokens(command_start);

            (*num_commands)++;
            command_start = command_end + 1;
        }

        // Parse the last command
        commands[*num_commands] = parse_tokens(command_start);
        (*num_commands)++;

        // Reallocate commands array to the actual size
        commands = (Command*)realloc(commands, *num_commands * sizeof(Command));

        return commands;
    }
}

Command* parse_commands_from_args(int argc, char** argv, int* num_commands) {
    // Join all arguments into a single string
    int total_length = 0;
    for (int i = 1; i < argc; i++) {
        total_length += strlen(argv[i]) + 1;
    }
    char* command_str = (char*)malloc(total_length);
    command_str[0] = '\0';
    for (int i = 1; i < argc; i++) {
        strcat(command_str, argv[i]);
        if (i < argc - 1) {
            strcat(command_str, " ");
        }
    }

    // Split input into commands separated by "|"
    Command* commands = NULL;
    *num_commands = 0;
    int command_capacity = 10;

    // Allocate memory for commands
    commands = (Command*)malloc(command_capacity * sizeof(Command));

    // Parse each command into tokens and store it in the commands array
    char* command_start = command_str;
    char* command_end;
    while ((command_end = strchr(command_start, '|')) != NULL) {
        *command_end = '\0';

        // Parse command into tokens and store it in the commands array
        commands[*num_commands] = parse_tokens(command_start);

        (*num_commands)++;
        command_start = command_end + 1;
    }

    // Parse the last command
    commands[*num_commands] = parse_tokens(command_start);
    (*num_commands)++;

    // Reallocate commands array to the actual size
    commands = (Command*)realloc(commands, *num_commands * sizeof(Command));

    free(command_str);

    return commands;
}

void split_command(Command* command, Command** commands, int* num_commands) {
    int i, j, k;
    for (i = 0; i < command->num_tokens; i++) {
        if (strcmp(command->tokens[i], "|") == 0) {
            // Create a new Command struct and copy the tokens up to this point
            Command* new_command = (Command*)malloc(sizeof(Command));
            new_command->num_tokens = i;
            new_command->tokens = (char**)malloc(sizeof(char*) * i);
            for (j = 0; j < i; j++) {
                new_command->tokens[j] = (char*)malloc(sizeof(char) * (strlen(command->tokens[j]) + 1));
                strcpy(new_command->tokens[j], command->tokens[j]);
            }
            // Add the new Command struct to the array
            (*num_commands)++;
            *commands = (Command*)realloc(*commands, sizeof(Command) * (*num_commands));
            (*commands)[(*num_commands) - 1] = *new_command;
            // Free the memory allocated for the new Command struct
            for (j = 0; j < i; j++) {
                free(new_command->tokens[j]);
            }
            free(new_command->tokens);
            free(new_command);
            // Move the remaining tokens to the beginning of the original Command struct
            int remaining_tokens = command->num_tokens - i - 1;
            memmove(command->tokens, &command->tokens[i + 1], sizeof(char*) * remaining_tokens);
            command->num_tokens = remaining_tokens;
            i = -1;  // Start iterating from the beginning of the updated tokens
        }
    }
    // Create a new Command struct for the remaining tokens
    if (command->num_tokens > 0) {
        Command* new_command = (Command*)malloc(sizeof(Command));
        new_command->num_tokens = command->num_tokens;
        new_command->tokens = (char**)malloc(sizeof(char*) * command->num_tokens);
        for (k = 0; k < command->num_tokens; k++) {
            new_command->tokens[k] = (char*)malloc(sizeof(char) * (strlen(command->tokens[k]) + 1));
            strcpy(new_command->tokens[k], command->tokens[k]);
        }
        // Add the new Command struct to the array
        (*num_commands)++;
        *commands = (Command*)realloc(*commands, sizeof(Command) * (*num_commands));
        (*commands)[(*num_commands) - 1] = *new_command;
        // Free the memory allocated for the new Command struct
        for (k = 0; k < command->num_tokens; k++) {
            free(new_command->tokens[k]);
        }
        free(new_command->tokens);
        free(new_command);
    }
}

void free_command(Command command) {
    // Free individual token strings
    for (int i = 0; i < command.num_tokens; i++) {
        free(command.tokens[i]);
    }

    // Free array of tokens
    free(command.tokens);
}

// Built-in command: cd
void cd(Command command) {
    // Extension 3.2: Home directory
    if (command.num_tokens == 1) {
        char* home_dir = getenv("HOME");
        if (chdir(home_dir) != 0) {
            printf("!");
            perror("cd");
        }
        return;
    }

    if (access(command.tokens[1], F_OK) == 0) {
        if (chdir(command.tokens[1]) != 0) {
            printf("!");
            perror("cd");
        }
    } else {
        printf("!");
        perror("cd");
    }
}

void print_tokens(char** tokens) {
    int i = 0;

    // Print each token
    while (tokens[i] != NULL) {
        printf("%s\n", tokens[i++]);
    }
}

int is_path_to_executable(char* path) {
    if (path[0] == '/') {
        return true;
    }
    return false;
}

void execute_command(Command command, int input_fd, int output_fd) {
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        // Child process
        if (input_fd != STDIN_FILENO) {
            dup2(input_fd, STDIN_FILENO);
            close(input_fd);
        }

        if (output_fd != STDOUT_FILENO) {
            dup2(output_fd, STDOUT_FILENO);
            close(output_fd);
        }

        // Handle input and output redirection
        int i = 0;
        while (i < command.num_tokens) {
            if (strcmp(command.tokens[i], "<") == 0 && i + 1 < command.num_tokens) {
                char* input_file = command.tokens[i + 1];
                int fd = open(input_file, O_RDONLY);
                if (fd == -1) {
                    perror(input_file);
                    exit(EXIT_FAILURE);
                }
                dup2(fd, STDIN_FILENO);
                close(fd);

                remove_token(&command, i);
                remove_token(&command, i);  // remove the file name token
            } else if (strcmp(command.tokens[i], ">") == 0 && i + 1 < command.num_tokens) {
                char* output_file = command.tokens[i + 1];
                int fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
                if (fd == -1) {
                    perror(output_file);
                    exit(EXIT_FAILURE);
                }
                dup2(fd, STDOUT_FILENO);
                close(fd);

                remove_token(&command, i);
                remove_token(&command, i);  // remove the file name token
            } else {
                i++;
            }
        }

        Command expanded_command = expand_wildcards(command);
        if (strcmp(expanded_command.tokens[0], "cd") == 0) {
            cd(expanded_command);
        } else if (strcmp(expanded_command.tokens[0], "pwd") == 0) {
            print_cwd();
        } else if (strcmp(expanded_command.tokens[0], "exit") == 0) {
            free_expanded_command(&expanded_command);
            exit(EXIT_SUCCESS);
        } else if (is_path_to_executable(expanded_command.tokens[0]) == true) {
            if (execvp(expanded_command.tokens[0], expanded_command.tokens) == -1) {
                perror("execvp");
                printf("!");
            }
        } else {
            struct stat* fs = malloc(sizeof(struct stat));
            char* path = search_for_file_from_base_dirs(expanded_command.tokens[0], &fs);
            if (execvp(path, expanded_command.tokens) == -1) {
                perror("execvp");
                printf("!");
            }

            free(path);
            free(fs);
        }

        free_expanded_command(&expanded_command);
        perror("execvp");
        exit(EXIT_FAILURE);
    } else {
        if (strcmp(command.tokens[0], "cd") == 0) {
            cd(command);
        } else if (strcmp(command.tokens[0], "pwd") == 0) {
            print_cwd();
        } else if (strcmp(command.tokens[0], "exit") == 0) {
            free_command(command);
            exit(EXIT_SUCCESS);
        }

        int child_status;
        if (wait(&child_status) == -1) {
            perror("waitpid");
            exit(EXIT_FAILURE);
        }
        // printf("Child exited with status: %d\n", child_status);
    }
}

int main(int argc, char** argv) {
    // int original_stdout = dup(STDOUT_FILENO);
    // int original_stdin = dup(STDIN_FILENO);

    while (true) {
        if (argc == 1) {
            int num_commands = 0;

            // Interactive mode
            Command* commands = parse_commands(&num_commands);
            int pipes[num_commands - 1][2];  // array of pipes

            for (int i = 0; i < num_commands; i++) {
                Command command = commands[i];

                if (i < num_commands - 1) {
                    if (pipe(pipes[i]) == -1) {  // create pipe
                        perror("pipe");
                        exit(EXIT_FAILURE);
                    }
                }

                execute_command(command, i == 0 ? STDIN_FILENO : pipes[i - 1][0], i == num_commands - 1 ? STDOUT_FILENO : pipes[i][1]);
                if (i > 0) {
                    close(pipes[i - 1][0]);
                }
                if (i < num_commands - 1) {
                    close(pipes[i][1]);
                }
            }
        } else {
            int num_commands = 0;
            Command* commands = parse_commands_from_args(argc, argv, &num_commands);
            int pipes[num_commands - 1][2];  // array of pipes

            for (int i = 0; i < num_commands; i++) {
                Command command = commands[i];

                if (i < num_commands - 1) {
                    if (pipe(pipes[i]) == -1) {  // create pipe
                        perror("pipe");
                        exit(EXIT_FAILURE);
                    }
                }

                execute_command(command, i == 0 ? STDIN_FILENO : pipes[i - 1][0], i == num_commands - 1 ? STDOUT_FILENO : pipes[i][1]);
                if (i > 0) {
                    close(pipes[i - 1][0]);
                }
                if (i < num_commands - 1) {
                    close(pipes[i][1]);
                }
            }

            return 0;
        }
    }
}
