#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/wait.h>

void *worker(void *arg) {
    printf("my Worker thread started\n");
     
int fd[2];
    if (pipe(fd) == -1) {
        perror("pipe");
        return NULL;
    }

 pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return NULL;
    }
if (pid == 0) {

dup2(fd[1], STDOUT_FILENO);  
        close(fd[0]);    
        close(fd[1]);

execlp("echo", "echo", "Hello from worker", NULL);

perror("exec");
        exit(1);

} else{

close(fd[1]);  
        char buf[128];
        int n = read(fd[0], buf, sizeof(buf)-1);
        if (n > 0) {
            buf[n] = '\0';
            printf("Worker got: %s", buf);
 }
        close(fd[0]);
        wait(NULL);  

}


    return NULL;
}

void *receiver(void *arg) {
    printf("receiver thread started\n");
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

//Split commands by '->'
int cmd_count = 0;
    char **commands = NULL;
    char *token = strtok(cmd_str, "->");
    while (token) {
        while (*token == ' ') token++; // trim 
        char *end = token + strlen(token) - 1;
        while (end > token && *end == ' ') *end-- = '\0'; // trim

        commands = realloc(commands, sizeof(char*) * (cmd_count + 1));
        commands[cmd_count++] = strdup(token);

        token = strtok(NULL, "->");
    }

    printf("Parsed %d commands:\n", cmd_count);
    for (int i = 0; i < cmd_count; i++) {
        printf("[%d]: %s\n", i, commands[i]);
    }


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

//

 pthread_t workers[n];
 pthread_t receiv;

for (int i = 0; i < n; i++) {
        pthread_create(&workers[i], NULL, worker, NULL);
    }

//one reveiver thread
pthread_create(&receiv, NULL, receiver, NULL);

for (int i = 0; i < n; i++) {
 pthread_join(workers[i], NULL);
}
pthread_join(receiv, NULL);

// free memory 
  for (int i = 0; i < count; i++) {
        free(all_lines[i]);
       }

 free(all_lines);

  printf("Threads: %d\n",n);
  printf("Commands: %s\n",cmd_str);


return 0;

}
