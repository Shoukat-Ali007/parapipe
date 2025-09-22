#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

void *worker(void *arg) {
    printf("my Worker thread started\n");
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

return 0;

}
