#include <http_server.h>
#include <iostream>

HTTP_Server * server;  // global for SIGUSR1 handler

void PrintHelp(char * name){
  std::cerr << "\tUsage: %s <name xml configuration file>\n\n";
}

int main(int argc, char * argv[]){
  if (argc < 2){
    std::cerr << "Error!!! Too few arguments!\n";
    PrintHelp(argv[0]);
    return EXIT_FAILURE;
  }

  server = new HTTP_Server(argv[1]);
  server->Run();

  delete server;

  return EXIT_SUCCESS;
}
