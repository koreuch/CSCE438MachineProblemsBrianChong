// #include <glog/logging.h>

// TODO: Implement Chat Server.
#include <glog/logging.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "interface.h"

//including all the header files that are also in the client side code

int main(int argc, char *argv[]){
    google::InitGoogleLogging(argv[0]);

    LOG(INFO) << "Starting Server";

}

// THis is just me adding a comment for git to make a branch for MP1
