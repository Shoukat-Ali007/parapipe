#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

struct worker_arg {
  char **commands;
  int cmd_count;
 char **lines;
 int start_idx;  
  int line_count;
 int end_idx;
  char **results;
 
};

void *worker(void *arg) {
    struct worker_arg *warg = (struct worker_arg *)arg;

 // For each input line
  for(int l = warg->start_idx; l < warg->end_idx; l++) {
     char *input = warg->lines[l];
     int prev_fd[2];
     int next_fd[2];
     int last_output = -1; 

     pid_t pids[64]; 
     int pid_count = 0;


   for (int c = 0; c < warg->cmd_count; c++) {
     if (c != warg->cmd_count - 1) pipe(next_fd); 

     pid_t pid = fork();
      if (pid == 0) {
  //this is Child  
         if (c > 0) {
       dup2(prev_fd[0], STDIN_FILENO); 
      close(prev_fd[0]);
       close(prev_fd[1]);
                } else {
 // first read from input line
      int input_pipe[2];
     pipe(input_pipe);
     write(input_pipe[1], input, strlen(input));
      close(input_pipe[1]);
    dup2(input_pipe[0], STDIN_FILENO);
       close(input_pipe[0]);
        }

    if (c != warg->cmd_count - 1) {
     dup2(next_fd[1], STDOUT_FILENO); 
     close(next_fd[0]);
    close(next_fd[1]);
                }

  // Execute command
    char *cmd_args[10];
    char *token = strtok(warg->commands[c], " ");
   int idx = 0;
     while (token) {
       cmd_args[idx++] = token;
       token = strtok(NULL, " ");
        }
   cmd_args[idx] = NULL;

    execvp(cmd_args[0], cmd_args);
    perror("execvp failed");
    exit(1);
  } else {
   
//Parent 

 pids[pid_count++] = pid; 

    if (c > 0) {
      close(prev_fd[0]);
      close(prev_fd[1]);
     }
      if (c != warg->cmd_count - 1) {
    prev_fd[0] = next_fd[0];
    prev_fd[1] = next_fd[1];
     } else {
      last_output = prev_fd[0]; 
       }
    
    }
   }
for (int i = 0; i < pid_count; i++) {
            waitpid(pids[i], NULL, 0);
        }


  //Read output from last command 
    if (last_output != -1) {
       char buf[1024];
       int n = read(last_output, buf, sizeof(buf)-1);
       if (n > 0) {
          buf[n] = '\0';
           warg->results[l] = strdup(buf);
          }

            close(last_output);
        }
    }

    return NULL;
}

//receiver
void *receiver(void *arg) {
char **results = (char **)arg;
    for (int i = 0; results[i]; i++) {
        printf("%s", results[i]); 
    }    

    return NULL;
}

int main(int argc, char *argv[]) {
    int n = 0;
    char *cmd_str = NULL;

     for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0 && i+1 < argc) {
            n = atoi(argv[++i]);

        } else if (strcmp(argv[i], "-c") == 0 && i+1 < argc) {
            cmd_str = argv[++i];
        }
    }


 if (n <= 0 || !cmd_str) {
        fprintf(stderr, "Usage: %s -n <num_threads> -c \"commands\"\n", argv[0]);
        return 1;
  }

//Split commands by '->' for proper command now
int cmd_count = 0;
char **commands = NULL;
char *cmd_copy = strdup(cmd_str);
char *p = cmd_copy;

while (p) {
    char *arrow = strstr(p, "->");
    if (arrow) {
        *arrow = '\0';         
    }

  //trim
    while (*p == ' ') p++;
    char *end = p + strlen(p) - 1;
    while (end > p && *end == ' ') *end-- = '\0';
    if (*p) {
        commands = realloc(commands, sizeof(char*) * (cmd_count + 1));
        commands[cmd_count++] = strdup(p);
    }

    if (arrow)
        p = arrow + 2;  
    else
        p = NULL;
}

free(cmd_copy);


// now this is for read input

char *line = NULL;
size_t cap = 0;
ssize_t len;
  int count = 0;
char **all_lines = NULL;

  while ((len = getline(&line, &cap, stdin)) != -1) {
        all_lines = realloc(all_lines, sizeof(char*) * (count+1));
        all_lines[count] = strdup(line);  // coping
        count++;
    }
    free(line);

    printf("Read %d lines\n", count);

//result array
 char **results = calloc(count + 1, sizeof(char*));

//create worker thread
pthread_t workers[n];
    int lines_per_thread = (count + n - 1) / n;
for (int i = 0; i < n; i++) {
        struct worker_arg *warg = malloc(sizeof(struct worker_arg));
        warg->commands = commands;
        warg->cmd_count = cmd_count;
        warg->lines = all_lines;
        warg->start_idx = i * lines_per_thread;
        warg->end_idx = (i + 1) * lines_per_thread;
        if (warg->end_idx > count) warg->end_idx = count;
        warg->results = results;

        pthread_create(&workers[i], NULL, worker, warg);
    }


    for (int i = 0; i < n; i++) {
       pthread_join(workers[i], NULL);
    } 



//one reveiver thread
pthread_t receiv;
    pthread_create(&receiv, NULL, receiver, results);
    pthread_join(receiv, NULL);


// free memory 
  for (int i = 0; i < count; i++) free(all_lines[i]);
    free(all_lines);
    for (int i = 0; i < cmd_count; i++) free(commands[i]);
    free(commands);
    for (int i = 0; i < count; i++) free(results[i]);
    free(results);

  printf("Threads: %d\n",n);
  


return 0;

}
