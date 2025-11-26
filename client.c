#include <stdio.h>
#include <stdlib.h>
/* You will to add includes here */


int main(int argc, char *argv[]){
  
  /*use select to read from the server and STDIN*/

  if (argc < 3) {
    printf("Too few arguments!\nExpected: <server-ip>:<server:port> <nickname>\n");
    return 1;
  }

  if (argc > 3) {
    printf("Too many arguments!\nExpected: <server-ip>:<server:port> <nickname>\n");
    return 1;
  }
  

}
