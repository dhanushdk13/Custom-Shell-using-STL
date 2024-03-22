#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define LSH_RL_BUFSIZE 1024
#define LSH_TOK_BUFSIZE 64
#define LSH_TOK_DELIM " \t\r\n\a"

char *builtin_str[] = {
  "cd",
  "help",
  "exit",
  "ls"
};


int lsh_cd(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "lsh: expected argument to \"cd\"\n");
    } else if (strcmp(args[1], "..") == 0) {
        char cwd[1024];
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            // Get parent directory by removing last directory name from the current path
            char *lastSlash = strrchr(cwd, '/');
            if (lastSlash != NULL) {
                *lastSlash = '\0';
                if (chdir(cwd) != 0) {
                    perror("lsh");
                }
            } else {
                fprintf(stderr, "lsh: failed to navigate to parent directory\n");
            }
        } else {
            perror("lsh");
        }
    } else {
        if (chdir(args[1]) != 0) {
            perror("lsh");
        }
    }
    return 1;
}


int lsh_ls(char **args) {
    args[0] = "/bin/ls";
    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        if (execvp(args[0], args) == -1) {
            perror("lsh");
            exit(EXIT_FAILURE);
        }
    } else if (pid < 0) {
        // Error forking
        perror("lsh");
    } else {
        // Parent process
        wait(NULL); // Wait for child to finish
    }
    return 1;
}

int lsh_num_builtins() {
    return sizeof(builtin_str) / sizeof(char *);
}

int lsh_help(char **args) {
    printf("Stephen Brennan's LSH\n");
    printf("Type program names and arguments, and hit enter.\n");
    printf("The following are built in:\n");

    for (int i = 0; i < lsh_num_builtins(); i++) {
        printf("  %s\n", builtin_str[i]);
    }

    printf("Use the man command for information on other programs.\n");
    return 1;
}

int lsh_exit(char **args) {
    return 0;
}

int lsh_launch(char **args) {
    pid_t pid;
    int status;

    pid = fork();
    if (pid == 0) {
        // Child process
        if (execvp(args[0], args) == -1) {
            perror("lsh");
        }
        exit(EXIT_FAILURE);
    } else if (pid < 0) {
        // Error forking
        perror("lsh");
    } else {
        // Parent process
        do {
            waitpid(pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }

    return 1;
}

int lsh_execute(char **args) {
    if (args[0] == NULL) {
        // An empty command was entered.
        return 1;
    }
    
    if (strcmp(args[0], "cd") == 0) {
        return lsh_cd(args);
    } else if (strcmp(args[0], "ls") == 0) {
        return lsh_ls(args);
    } else if (strcmp(args[0], "help") == 0) {
        return lsh_help(args);
    } else if (strcmp(args[0], "exit") == 0) {
        return lsh_exit(args);
    } else {
        // For other commands, execute in a child process
        return lsh_launch(args);
    }
}

char *lsh_read_line(void) {
    int bufsize = LSH_RL_BUFSIZE;
    int position = 0;
    char *buffer = (char*)malloc(sizeof(char) * bufsize);
    int c;

    if (!buffer) {
        fprintf(stderr, "lsh: allocation error\n");
        exit(EXIT_FAILURE);
    }

    while (1) {
        // Read a character
        c = getchar();

        if (c == EOF || c == '\n') {
            buffer[position] = '\0';
            return buffer;
        } else {
            buffer[position] = c;
        }
        position++;

        // If we have exceeded the buffer, reallocate.
        if (position >= bufsize) {
            bufsize += LSH_RL_BUFSIZE;
            buffer = (char*)realloc(buffer, bufsize);
            if (!buffer) {
                fprintf(stderr, "lsh: allocation error\n");
                exit(EXIT_FAILURE);
            }
        }
    }
}

char **lsh_split_line(char *line) {
    int bufsize = LSH_TOK_BUFSIZE, position = 0;
    char **tokens = (char**)malloc(bufsize * sizeof(char*));
    char *token, **tokens_backup;

    if (!tokens) {
        fprintf(stderr, "lsh: allocation error\n");
        exit(EXIT_FAILURE);
    }

    token = strtok(line, LSH_TOK_DELIM);
    while (token != NULL) {
        tokens[position] = token;
        position++;

        if (position >= bufsize) {
            bufsize += LSH_TOK_BUFSIZE;
            tokens_backup = tokens;
            tokens = (char**)realloc(tokens, bufsize * sizeof(char*));
            if (!tokens) {
                free(tokens_backup);
                fprintf(stderr, "lsh: allocation error\n");
                exit(EXIT_FAILURE);
            }
        }

        token = strtok(NULL, LSH_TOK_DELIM);
    }
    tokens[position] = NULL;
    return tokens;
}

void lsh_loop(void) {
    char *line;
    char **args;
    int status;

    do {
        printf("> ");
        line = lsh_read_line();
        args = lsh_split_line(line);
        status = lsh_execute(args);

        free(line);
        free(args);
    } while (status);
}

int main(int argc, char **argv) {
    // Run command loop
    lsh_loop();
    return EXIT_SUCCESS;

}