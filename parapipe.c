#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>


struct worker_arg {
  char **commands;
  int cmd_count;
 char **lines;
 int start_idx;  
  int line_count;
 int end_idx;
  char **results;
 
};


//queue for streaming results to receiver
typedef struct Node {
    char *data;
    struct Node *next;
} Node;

static Node *q_head = NULL;
static Node *q_tail = NULL;
static pthread_mutex_t q_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t q_cond = PTHREAD_COND_INITIALIZER;
static int workers_running = 0; // number of worker threads still running

void enqueue_result(char *s) {
    Node *n = malloc(sizeof(Node));
    n->data = s; // take ownership of string
    n->next = NULL;

    pthread_mutex_lock(&q_mutex);
    if (!q_head) {
        q_head = q_tail = n;
    } else {
        q_tail->next = n;
        q_tail = n;
    }
    pthread_cond_signal(&q_cond);
    pthread_mutex_unlock(&q_mutex);
}

char *dequeue_result() {
    pthread_mutex_lock(&q_mutex);
    Node *n = q_head;
    if (!n) {
        pthread_mutex_unlock(&q_mutex);
        return NULL;
    }
    q_head = n->next;
    if (!q_head) q_tail = NULL;
    pthread_mutex_unlock(&q_mutex);

    char *s = n->data;
    free(n);
    return s;
}

void *worker(void *arg) {
struct worker_arg *warg = (struct worker_arg *)arg;
  struct worker_arg *wa = (struct worker_arg *)arg;

 
   int num_cmds = wa->cmd_count;
    if (num_cmds <= 0) {
       goto done;
      }

 //create pipes between commands
    int (*pipes)[2] = NULL;
    if (num_cmds > 1) {
        pipes = malloc(sizeof(int[2]) * (num_cmds - 1));
        for (int i = 0; i < num_cmds - 1; i++) {
         if (pipe(pipes[i]) < 0) {
             perror("pipe");
              
              for (int j = 0; j < i; j++) {
 close(pipes[j][0]);
 close(pipes[j][1]);
 }
          free(pipes);
           goto done;
         }
       }
    }

 //create in_pipe for first command 
    int in_pipe[2];
    if (pipe(in_pipe) < 0) {
        perror("pipe");
        if (pipes) {
            for (int i = 0; i < num_cmds - 1; i++) {
 close(pipes[i][0]);
 close(pipes[i][1]);
 }
            free(pipes);
        }
        goto done;
    }

 //create out_pipe 
    int out_pipe[2];
    if (pipe(out_pipe) < 0) {
        perror("pipe");
    close(in_pipe[0]);
    close(in_pipe[1]);
        if (pipes) {
       for (int i = 0; i < num_cmds - 1; i++) {
 close(pipes[i][0]);
 close(pipes[i][1]);
     }
          free(pipes);
       }
      goto done;
    }


//fork all commands 
    pid_t *pids = calloc(num_cmds, sizeof(pid_t));
    for (int c = 0; c < num_cmds; c++) {
      pid_t pid = fork();
      if (pid < 0) {
        perror("fork");
      
       pids[c] = -1;
        continue;
      }
      if (pid == 0) {
            
        if (c == 0) {
          
        dup2(in_pipe[0], STDIN_FILENO);
            
         } else {
          
           dup2(pipes[c-1][0], STDIN_FILENO);
           
        }

   
  if (c == num_cmds - 1) {
          
       dup2(out_pipe[1], STDOUT_FILENO);
         
       } else{
         
       dup2(pipes[c][1], STDOUT_FILENO);
              
            }

//close all unused fds in child
close(in_pipe[0]);
 close(in_pipe[1]);
  close(out_pipe[0]); 
 close(out_pipe[1]);
      if (pipes) {
         for (int j = 0; j < num_cmds - 1; j++) {
            close(pipes[j][0]); close(pipes[j][1]);
                }
         } 


 //prepare argv 
    char *cmddup = strdup(wa->commands[c]);
    char *argv[64];
     int ai = 0;
     char *tok = strtok(cmddup, " ");
         while (tok && ai < 63) {
             argv[ai++] = tok;
              tok = strtok(NULL, " ");
                }
         argv[ai] = NULL;

 //exec
    execvp(argv[0], argv);
     
      perror("execvp failed");
         free(cmddup);
         _exit(1);
          } else {
//parent
      pids[c] = pid;
        }
    }

   
//closed unused fds
close(in_pipe[0]);
    
    if (pipes) {
        for (int j = 0; j < num_cmds - 1; j++) {
           close(pipes[j][0]);
            close(pipes[j][1]);
        }
    }
close(out_pipe[1]); 

for (int l = wa->start_idx; l < wa->end_idx; l++) {
 char *line = wa->lines[l];
  if (!line) continue;
   ssize_t towrite = strlen(line);
   ssize_t written = 0;
      while (written < towrite) {
         ssize_t w = write(in_pipe[1], line + written, towrite - written);
           if (w < 0) {
              if (errno == EINTR) continue;
                perror("write to pipeline");
                break;
            }
            written += w;
        }
    }
    
    close(in_pipe[1]); 

//read 
    char readbuf[2048];
    ssize_t nread;
    char *acc = NULL;
    size_t acc_len = 0;

    while ((nread = read(out_pipe[0], readbuf, sizeof(readbuf))) > 0) {

     char *newacc = realloc(acc, acc_len + nread + 1);    
    if (!newacc) {
     perror("realloc");
    free(acc);
     acc = NULL; acc_len = 0;
        break;
            }

        acc = newacc;
        memcpy(acc + acc_len, readbuf, nread);
        acc_len += nread;
        acc[acc_len] = '\0';

        char *start = acc;
        char *newline;
        while ((newline = memchr(start, '\n', acc + acc_len - start))) {
            size_t linelen = newline - start + 1;
            char *outline = malloc(linelen + 1);
            memcpy(outline, start, linelen);
            outline[linelen] = '\0';
            enqueue_result(outline);
            start = newline + 1;
        }

        size_t remain = acc + acc_len - start;
        if (remain > 0) memmove(acc, start, remain);
        acc_len = remain;
        if (acc) acc[acc_len] = '\0';
    }

    if (acc && acc_len > 0) {
        char *final = malloc(acc_len + 2);
        memcpy(final, acc, acc_len);
        final[acc_len] = '\n';
        final[acc_len + 1] = '\0';
        enqueue_result(final);
    }
    free(acc);
    close(out_pipe[0]);

    // Wait for children
    for (int c = 0; c < num_cmds; c++) {
      if (pids[c] > 0) waitpid(pids[c], NULL, 0);
      }
    free(pids);
    if (pipes) free(pipes);

 done:
   pthread_mutex_lock(&q_mutex);
    workers_running--;
    pthread_cond_signal(&q_cond);
    pthread_mutex_unlock(&q_mutex);


    return NULL;
                 }


//receiver
void *receiver(void *arg) {
 (void)arg; 
    while (1) {
      pthread_mutex_lock(&q_mutex);
      while (!q_head && workers_running > 0) {
          pthread_cond_wait(&q_cond, &q_mutex);
          }

 //if queue empty 
    if (!q_head && workers_running == 0) {
         pthread_mutex_unlock(&q_mutex);
             break;
            }
//else dequeue one
    Node *n = q_head;
       if (n) {
          q_head = n->next;
           if (!q_head) q_tail = NULL;
          }
   pthread_mutex_unlock(&q_mutex);

     if (n) {
 //print and free node
     printf("%s", n->data);
     fflush(stdout); fflush(stdout); 
      free(n->data);
      free(n);
        }
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
 pthread_t *workers = malloc(sizeof(pthread_t) * n);
    int lines_per_thread = (count + n - 1) / n;

   pthread_mutex_lock(&q_mutex);
   workers_running = 0;
   pthread_mutex_unlock(&q_mutex);

 int worker_idx = 0;
 for (int i = 0; i < n; i++) {
        int start_idx = i * lines_per_thread;
        int end_idx = (i+1) * lines_per_thread;
        if (start_idx >= count) break; 

        if (end_idx > count) end_idx = count;

        struct worker_arg *warg = malloc(sizeof(struct worker_arg));
        warg->commands = commands;
        warg->cmd_count = cmd_count;
        warg->lines = all_lines;
        warg->start_idx = start_idx;
        warg->end_idx = end_idx;
        warg->results = results;

        pthread_mutex_lock(&q_mutex);
        workers_running++; 
        pthread_mutex_unlock(&q_mutex);

        pthread_create(&workers[worker_idx++], NULL, worker, warg);
    }


//one reveiver thread
pthread_t receiv;
    pthread_create(&receiv, NULL, receiver, results);

    // join workers
    for (int i = 0; i < n; i++) {
        pthread_join(workers[i], NULL);
    }

    pthread_mutex_lock(&q_mutex);
    pthread_cond_signal(&q_cond);
    pthread_mutex_unlock(&q_mutex);

//join receiver
    pthread_join(receiv, NULL);


// free memory 
  for (int i = 0; i < count; i++) free(all_lines[i]);
    free(all_lines);
    for (int i = 0; i < cmd_count; i++) free(commands[i]);
    free(commands);
    for (int i = 0; i < count; i++) free(results[i]);
    free(results);
     free(workers);

   printf("Threads: %d\n", worker_idx);
  


return 0;

}
