#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
int n=0;
char *cmd_str = NULL;

 for(int i=1; i<argc; i++){
   if(strcmp(argv[i], "-n")==0 && i+1<argc) {
        n = atoi(argv[++i]);

   } else if(strcmp(argv[i], "-c") == 0 && i+1 < argc) {
      cmd_str = argv[++i];
  } 

 }
    if(n <= 0 || !cmd_str) {
        fprintf(stderr, "Usage: %s -n <num_threads> -c \"commands\"\n", argv[0]);
         return 1;
   }
    printf("Threads: %d\n",n);
    printf("Commands: %s\n",cmd_str);
     return 0;
}
