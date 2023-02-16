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
#include <sys/socket.h>
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
    
    //should bind using 8080
    int num_used_ports = 0;
    service.sin_family = AF_INET;  
    service.sin_addr.s_addr = INADDR_ANY;  
    service.sin_port = htons( 8080 );
    int master_socket;
    if ((master_socket = socket(AF_INET, SOCK_STREAM,IPPROTO_TCP)) == 0){
		LOG(ERROR) << "ERROR: could not create server side socket";
		exit(EXIT_FAILURE);
    }
    // std::cout << "REMOVE ME: able to get master socket assigned\n";

    // if ((bind(master_socket, (sockaddr*)&service, sizeof(service))) <0 ){
    //     LOG(ERROR) << "ERROR: could not bind serverside socket";
	// 	exit(EXIT_FAILURE);
    // }

    bool bind_succeed = false;
    while (!bind_succeed){
        if ((bind(master_socket, (sockaddr*)&service, sizeof(service))) <0 ){
        LOG(ERROR) << "ERROR: could not bind serverside socket. Retrying";
        }
        else{
            bind_succeed = true;
        }
    }

    // std::cout << "REMOVE ME: able to bind server socket\n";

    if ((listen(master_socket, 20)) == -1){
        LOG(ERROR) << "ERROR: could not listen in ";
		exit(EXIT_FAILURE);
    }
            // std::cout << master_socket << std::endl;

    // std::thread socket_listener_thread(socketListener, master_socket);
    int last_socket;
    int new_socket;
    std::vector<int> socket_list;
    std::vector<int> chat_socket_list;
    socket_list.push_back(master_socket);
    std::vector<std::pair<std::string, int>> chat_sockets;
    char buf[MAX_DATA];
    while (true){
        // std::cout << "Waiting for messages from client\n";

        fd_set sockets; // temporary fix
        FD_ZERO(&sockets);
        FD_SET(master_socket, &sockets);

        last_socket = master_socket;
        std::vector<int> sockets_to_remove;

        //have a list only for the chatroom based sockets
        std::string client_reply_string;

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
                    sockets_to_remove.push_back(i);

                    int main_socket = socket_list[i];
                    if (recv(main_socket, buf, MAX_DATA, 0) > 0){
                        // std::cout << "Message received from one of the clients";
                        // display_message(buf);
                        std::cout << "\n";
                        std::string command(buf);
                        std::vector<std::string> command_list;

                        // if (command.length() < 5){
                        //     close(socket_list[i]);
                        //     close(master_socket);
                        //     LOG(ERROR) << "Improper command\n";
                        //     exit(EXIT_FAILURE);
                        // }
                        struct Reply message_to_client;
                        int pos = 0;
                        std::cout << "THe command sent over is " << command << std::endl;
                        while ((pos = command.find(' ')) != std::string::npos){
                            std::string substring = command.substr(0, pos);
                            command = command.substr(pos + 1);
                            command_list.push_back(substring);
                            // std::cout << substring << " HERE" << std::endl;
                        }
                        if (command.length() > 0){
                            command_list.push_back(command);
                        }
                    
                        // for (int i = 0; i < command_list.size(); ++i){
                        //     std::cout << i << std::endl;
                        //     // std::cout << command_list[i] << std::endl;
                        // }

                        // now doing branches depending on the commands
                        for (auto x: command_list){
                            std::cout << "Part of the command is " << x << std::endl;
                        }
                        command = command_list[0];
                        std::string name;
                        // std::cout << "command is " << command << std::endl;
                        if (command != "LIST"){
                            name = command_list[1];
                        }

                        std::cout << "THat and error occurs or not" << std::endl;
                        if (command == "CREATE"){
                            std::cout << name << std::endl;
                            service.sin_family = AF_INET;  
                            service.sin_addr.s_addr = INADDR_ANY;  
                            service.sin_port = 0;
                            // num_used_ports += 1;
                            // this way no ports are reused

                            //port is set to 0 since it will just use the next available port

                            int copy_socket;
                            if ((copy_socket = socket(AF_INET, SOCK_STREAM,IPPROTO_TCP)) == 0){
                                LOG(ERROR) << "ERROR: could not create chat socket";
                                exit(EXIT_FAILURE);
                            }

                            bool bind_success = false;
                            while (!bind_success){
                                if ((bind(copy_socket, (sockaddr*)&service, sizeof(service))) <0 ){
                                LOG(ERROR) << "ERROR: could not bind chat socket. Retrying";
                                }
                                else{
                                    bind_success = true;
                                }
                            }


                            if ((listen(copy_socket, 20)) == -1){
                                LOG(ERROR) << "ERROR: could not listen in on chat socket";
                                exit(EXIT_FAILURE);
                            }
                            // have socket activated for listening now
                            
                            bool found = false;
                            for (auto x : chat_sockets){
                                std::cout << "PORT NUM: " << x.second << std::endl;
                                if (name == x.first){
                                    found = true;
                                }
                            }
                            int port_number;

                            if (!found){
                                chat_socket_list.push_back(copy_socket);
                                // printf("port number %d\n", ntohs(service.sin_port));
                                std::cout << "The name of the room is " << name << std::endl;
                                socklen_t sockLength = sizeof(service);

                                getsockname(copy_socket, (sockaddr*)&service, &sockLength);
                                port_number = ntohs(service.sin_port);
                                chat_sockets.push_back({name,port_number});
                            }
                    
                            if (found){
                                client_reply_string = std::to_string(1);
                                client_reply_string += ',';               
                            }
                            else{
                                client_reply_string = std::to_string(0);
                                client_reply_string += ',';           
                            }

                            client_reply_string += std::to_string(0);
                            client_reply_string += ",";
                            client_reply_string += std::to_string(port_number);
                            client_reply_string += ",";
                            client_reply_string += "noList";
                            // std::cout << "clientReplyString: " << client_reply_string << std::endl;
                            
                        }
                        else if (command == "JOIN"){
                            
                        }
                        else if (command == "DELETE"){

                        }
                        else if (command == "LIST"){
                            client_reply_string = ",,,";
                            for(int j = 0; j < chat_sockets.size(); ++j){
                                std::cout << chat_sockets[j].first << "THIS IS THE KEY" << std::endl;
                                client_reply_string += (std::string(chat_sockets[j].first) + ", "); 
                            }
                        }
                        std::cout << "The client reply string is " << client_reply_string << std::endl;
                        send(main_socket, client_reply_string.c_str(), MAX_DATA, 0);
                        
                    }//end reading in request
                    else{
                        LOG(ERROR) << "ERROR: error in communicating to server message\n";
                        exit(EXIT_FAILURE);
                    }

                }
            }
        }
        // std::cout << "Size of the socket list is " << socket_list.size() << std::endl;
        // std::cout << "Size of sockets to remove is " << sockets_to_remove.size() << std::endl;

        // for (int i = 0; i < sockets_to_remove.size(); ++i){
        //     std::cout << "Removing socket : " << sockets_to_remove[i] << std::endl;
        // }
        for (int i = 0; i< socket_list.size(); ++i){
            std::cout << socket_list[i] << std::endl;
            // std::cout << "Socket here" << std::endl;
        }

        for (int i = 0; i < sockets_to_remove.size(); ++i){ // removing the sockets that have already communicated once to server
            // std::cout << master_socket << std::endl;
            // std::cout << socket_list[i] << std::endl;

            socket_list.erase(socket_list.begin() +sockets_to_remove[i], socket_list.begin() + (sockets_to_remove[i] + 1));
            // close(socket_list[i]);
        }
        sockets_to_remove.clear();
    }

    return 0;
    // struct sock_addr_master_socket

}

// THis is just me adding a comment for git to make a branch for MP1