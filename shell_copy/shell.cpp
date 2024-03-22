#include <iostream>
#include <fstream>
#include <string>
#include <queue>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <utime.h>
#include <sstream>
using namespace std;

#define LSH_RL_BUFSIZE 1024
#define LSH_TOK_BUFSIZE 64
#define LSH_TOK_DELIM " \t\r\n\a"

//All the built-in commands
char *builtin_str[] = {
  "cd",
  "help",
  "exit",
  "ls",
  "touch",
  "history",
  "cp"
};
//The queue of current history
queue<string> command_history;
//converts char** args into string to make args pushable to queue
void push_command_to_history(char **args) {
    stringstream ss;
    for (int i = 0; args[i] != NULL; ++i) {
        ss << args[i] << " ";
    }
    string command = ss.str();
    command_history.push(command);
}
//prints the history by making a copy of queue
int lsh_history(char **args) {
    int count = 1;
    queue<string> temp = command_history;
    while (!temp.empty()) {
        cout << count << ". " << temp.front() << endl;
        temp.pop();
        count++;
    }
    return 1;
}
//clears history_queue
int lsh_clear(char **args){
    while (!command_history.empty()) {
        command_history.pop();
    }
    cout << "lsh: command history cleared" << endl;
    return 1;
}

// Define the function for cp command
int lsh_cp(char **args) {
    if (args[1] == NULL || args[2] == NULL) {
        cerr << "lsh: expected source and destination filenames for cp command" << endl;
        return 1;
    }

    ifstream source(args[1], ios::binary);
    if (!source) {
        cerr << "lsh: error opening source file " << args[1] << endl;
        return 1;
    }

    ofstream dest(args[2], ios::binary | ios::trunc);
    if (!dest) {
        cerr << "lsh: error opening destination file " << args[2] << endl;
        return 1;
    }

    dest << source.rdbuf();
    source.close();
    dest.close();

    cout << "lsh: file " << args[1] << " copied to " << args[2] << endl;
    return 1;
}

//touch command
int lsh_touch(char **args) {
    if (args[1] == NULL) {
        cerr << "lsh: expected filename for touch command" << endl;
        return 1;
    }

    // Check if the file exists
    if (access(args[1], F_OK) != -1) {
        // File exists, update its timestamp
        struct utimbuf ut;
        ut.actime = ut.modtime = time(NULL); // Update both access and modification times
        if (utime(args[1], &ut) == -1) {
            cerr << "lsh: error updating timestamp of file " << args[1] << endl;
        } else {
            cout << "lsh: updated timestamp of file " << args[1] << endl;
        }
    } else {
        // File doesn't exist, create it
        ofstream file(args[1]);
        if (!file) {
            cerr << "lsh: error creating file " << args[1] << endl;
        } else {
            cout << "lsh: created file " << args[1] << endl;
            file.close();
        }
    }
    return 1;
}


//cd command
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

//ls command
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
//help command
int lsh_help(char **args) {
    printf("Dhanush Kumar's LSH\n");
    printf("Type program names and arguments, and hit enter.\n");
    printf("The following are built in:\n");

    for (int i = 0; i < lsh_num_builtins(); i++) {
        printf("  %s\n", builtin_str[i]);
    }

    printf("Use the man command for information on other programs.\n");
    return 1;
}
//exit command
int lsh_exit(char **args) {
    return 0;
}
//defalts to launch if no other functions match
//Since i used Mac OS i had to search 2 directories :
//: /bin/command and usr/bin/command as most commands come in these 2 directories
int lsh_launch(char **args) {
    pid_t pid;
    int status;
    char command_path[1024];

    // Try executing command from /bin
    snprintf(command_path, sizeof(command_path), "/bin/%s", args[0]);
    pid = fork();
    if (pid == 0) {
        // Child process
        if (execvp(command_path, args) == -1) {
            // Try executing command from /usr/bin if /bin fails
            snprintf(command_path, sizeof(command_path), "/usr/bin/%s", args[0]);
            if (execvp(command_path, args) == -1) {
                // If command is not found in both directories, print "command not found"
                std::cerr << "lsh: command not found: " << args[0] << std::endl;
                exit(EXIT_FAILURE);
            }
        }
        exit(EXIT_SUCCESS);
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


//matches args with in built functions or defaults to lsh_launch
int lsh_execute(char **args) {
    push_command_to_history(args);
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
    }else if (strcmp(args[0], "touch") == 0) {
        return lsh_touch(args);
    }else if (strcmp(args[0], "history") == 0) {
        return lsh_history(args);
    }else if (strcmp(args[0], "clear") == 0) {
        return lsh_clear(args);
    }else if (strcmp(args[0], "cp") == 0) {
        return lsh_cp(args);
    }else {
        // For other commands, execute in a child process
        return lsh_launch(args);
    }
}
//Reads the line with constant allocation if memmory is required
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
//Tokenisation of read line 
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
//Runs untill exit
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