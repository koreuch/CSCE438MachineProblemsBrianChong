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

#include <pthread.h>

#include <iostream>
#include <vector>
#include <string>
#include <map>
//for my own use, will remove the above later

//including all the header files that are also in the client side code

void *socketListener(void* master_socket){
    while (1){
        int master_socket1 = *((int*) master_socket);
        int copy_socket;
        copy_socket = accept(master_socket1, NULL, NULL);
        if (copy_socket == -1){
            LOG(ERROR) << "ERROR: failed to duplicate master socket";
            exit(EXIT_FAILURE);
        }
    }
}

int main(int argc, char *argv[]){
    google::InitGoogleLogging(argv[0]);

    LOG(INFO) << "Starting Server";
    sockaddr_in service;
    fd_set sockets;
    //should bind using 8080
    service.sin_family = AF_INET;  
    service.sin_addr.s_addr = INADDR_ANY;  
    service.sin_port = htons( 8080 );
    int master_socket;
    if ((master_socket = socket(AF_INET, SOCK_STREAM,IPPROTO_TCP)) == 0){
		LOG(ERROR) << "ERROR: could not create server side socket";
		exit(EXIT_FAILURE);
    }
    // std::cout << "REMOVE ME: able to get master socket assigned\n";

    if ((bind(master_socket, (sockaddr*)&service, sizeof(service))) <0 ){
        LOG(ERROR) << "ERROR: could not bind serverside socket";
		exit(EXIT_FAILURE);
    }

    // std::cout << "REMOVE ME: able to bind server socket\n";

    if ((listen(master_socket, 20)) == -1){
        LOG(ERROR) << "ERROR: could not listen in ";
		exit(EXIT_FAILURE);
    }
            std::cout << master_socket << std::endl;

    // std::thread socket_listener_thread(socketListener, master_socket);
    int last_socket;
    int new_socket;
    std::vector<int> socket_list;
    std::vector<int> chat_socket_list;
    socket_list.push_back(master_socket);
    std::map<std::string, int> chat_sockets;
    char buf[MAX_DATA];
    while (true){
        std::cout << "Waiting for messages from client\n";

        FD_ZERO(&sockets);
        FD_SET(master_socket, &sockets);

        last_socket = master_socket;

        //have a list only for the chatroom based sockets
        for (int i = 0; i < socket_list.size(); ++i){
            last_socket = socket_list[i];
            //this was just to check that new sockets weren't constnatlyl being created
            // std::cout << "num sockets in list is " << socket_list.size() << std::endl;
            if (last_socket > 0){
                FD_SET(socket_list[i], &sockets);
            }
            else{
                LOG(ERROR) << "ERROR: Invalid chat socket added in\n";
                exit(EXIT_FAILURE);
            }
        }
        
        int activity = select(last_socket + 1, &sockets, NULL, NULL, NULL);
        if (activity < 0){
            LOG(ERROR) << "ERROR: Invalid activity\n";
            exit(EXIT_FAILURE);
        }

        if (FD_ISSET(master_socket, &sockets)){
            int size = sizeof(service);
            if ((new_socket = accept(master_socket, 
                    (struct sockaddr *)&service, (socklen_t*)(&size)))<0){
                    LOG(ERROR) << "ERROR: unable to accept new socket connection\n";
                    exit(EXIT_FAILURE);
            }
            else{
                socket_list.push_back(new_socket);
            }
        }
        else{
            for (int i = 1; i < socket_list.size(); ++i){
                if (FD_ISSET(socket_list[i], &sockets)){
                    // in this case it would be to process incoming message
                    int main_socket = socket_list[i];
                    std::cout << "TRIED TO READ FROM THE SOCKET TWICE" << std::endl;
                    if (read(socket_list[i], &buf, MAX_DATA) > 0){
                        // std::cout << "Message received from one of the clients";
                        // display_message(buf);
                        std::cout << "\n";
                        std::string command(buf);
                        std::vector<std::string> command_list;

                        if (command.length() < 5){
                            close(socket_list[i]);
                            close(master_socket);
                            LOG(ERROR) << "Improper command\n";
                            exit(EXIT_FAILURE);
                        }
                        struct Reply message_to_client;
                        int pos = 0;
                        while ((pos = command.find(' ')) != std::string::npos){
                            std::string substring = command.substr(0, pos);
                            command = command.substr(pos + 1);
                            command_list.push_back(substring);
                            std::cout << substring << " HERE" << std::endl;
                        }
                        if (command.length() > 0){
                            command_list.push_back(command);
                        }
                        for (int i = 0; i < command_list.size(); ++i){
                            std::cout << i << std::endl;
                            std::cout << command_list[i] << std::endl;
                        }

                        // now doing branches depending on the commands
                        std::string client_reply_string;
                        command = command_list[0];
                        // std::cout << "command is " << command << std::endl;
                        std::string name = command_list[1];
                        if (command == "CREATE"){
                            std::cout << name << std::endl;
                            service.sin_family = AF_INET;  
                            service.sin_addr.s_addr = INADDR_ANY;  
                            service.sin_port = htons( 1024 );

                            //port is set to 0 since it will just use the next available port

                            int chat_socket;
                            if ((chat_socket = socket(AF_INET, SOCK_STREAM,IPPROTO_TCP)) == 0){
                                LOG(ERROR) << "ERROR: could not create chat socket";
                                exit(EXIT_FAILURE);
                            }
                            if ((bind(chat_socket, (sockaddr*)&service, sizeof(service))) <0 ){
                                LOG(ERROR) << "ERROR: could not bind chat socket";
                                exit(EXIT_FAILURE);
                            }
                            if ((listen(chat_socket, 20)) == -1){
                                LOG(ERROR) << "ERROR: could not listen in on chat socket";
                                exit(EXIT_FAILURE);
                            }
                            chat_socket_list.push_back(chat_socket);
                            // printf("port number %d\n", ntohs(service.sin_port));
                            std::cout << "Made it here: " << ntohs(service.sin_port) << std::endl;
                            chat_sockets[name] = ntohs(service.sin_port);

                            if (chat_sockets.find(name) == chat_sockets.end()){
                                client_reply_string = std::to_string(1);
                                client_reply_string += ',';
                            }
                            else{
                                client_reply_string = std::to_string(1);
                                client_reply_string += ',';
                            }
                            client_reply_string += std::to_string(0);
                            client_reply_string += ",";
                            client_reply_string += std::to_string(ntohs(service.sin_port));
                            client_reply_string += ",";
                            client_reply_string += "noList";
                            // std::cout << "clientReplyString: " << client_reply_string << std::endl;
                            
                        }
                        else if (command == "JOIN"){
                            
                        }
                        else if (command == "DELETE"){

                        }
                        else if (command == "LIST"){

                        }
                        std::cout << "came here again" << std::endl;
                        send(main_socket, client_reply_string.c_str(), MAX_DATA, 0);
                        
                    }//end reading in request
                    else{
                        LOG(ERROR) << "ERROR: error in communicating to server message\n";
                        exit(EXIT_FAILURE);
                    }

                }
            }
        } 
    }

    return 0;
    // struct sock_addr_master_socket

}

// THis is just me adding a comment for git to make a branch for MP1
